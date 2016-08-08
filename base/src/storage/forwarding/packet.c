/**
 * \file storage/forwarding/packet.c
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Packet builder (source file)
 *
 * Copyright (C) 2016 CESNET, z.s.p.o.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of the Company nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * ALTERNATIVELY, provided that this notice is retained in full, this
 * product may be distributed under the terms of the GNU General Public
 * License (GPL) version 2 or later, in which case the provisions
 * of the GPL apply INSTEAD OF those given above.
 *
 * This software is provided ``as is, and any express or implied
 * warranties, including, but not limited to, the implied warranties of
 * merchantability and fitness for a particular purpose are disclaimed.
 * In no event shall the company or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even
 * if advised of the possibility of such damage.
 *
 */

#include <ipfixcol.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "packet.h"

/** Module description */
static const char *msg_module = "forwarding(packet)";


/** Default max number of elements in array */
#define AUX_ARR_DEF_SIZE  (16)
/** Default max number of parts for IPFIX message */
#define PARTS_DEF_SIZE    (16)

/** Size of headers (data/template set & template)  */
#define HEADER_SIZE       (4)
/** Minimal size of a packet (in bytes) */
#define PACKET_MIN_SIZE   (256)
/** Default max number of generated packets */
#define PACKET_MAX_NUM    (8)

/**
 * Maximal size of a (Options) Template Set in the packet (in bytes).
 * It is used as an auxiliary value, to avoid to long Template Sets.
 */
#define TMPLT_SET_MAX_LEN (512)

/**
 * \brief Type of the last IPFIX set inserted to the Packet builder
 */
enum FLOW_SET_TYPE {
	FST_NONE,                /**< Nothing inserted yet                */
	FST_DATA,                /**< Data Set                            */
	FST_TMPLT,               /**< Template Set (new templates)        */
	FST_TMPLT_WITHDRAW,      /**< Template Set (withdrawal)           */
	FST_OPT_TMPLT,           /**< Options Template Set (new templates)*/
	FST_OPT_TMPLT_WITHDRAW   /**< Options Template Set (withdrawal)   */
};

/**
 * \brief Auxiliary arrray for headers
 */
struct aux_array {
	size_t elem_size;   /**< Size of one element in the array         */

	uint8_t **arr_data; /**< Array of elements                        */
	size_t arr_size;    /**< Used elements                            */
	size_t arr_max;     /**< Available elements                       */
};

/**
 * \brief Structure of prepared packet
 */
struct packet_range {
	/** Start of a packet. First element is reserved for IPFIX header */
	struct iovec *start;
	/** Number of parts                                               */
	size_t size;
	/** Backup of the last part                                       */
	struct iovec backup;
	/** Number of data records                                        */
	unsigned int rec_cnt;
};

/**
 * \brief Parts of IPFIX packet
 */
struct packet_parts {
	/** Flag for preventing further insertion                         */
	bool insert_lock;

	/**
	 * Array of data fields to send.
	 * First (index 0) element is reserved for IPFIX packet header.
	 */
	struct iovec *rec_flds;
	/** Array of numbers of data records per a part of the packet     */
	unsigned int *rec_cnt;
	/** Array of positions of headers of Data/Template Sets           */
	bool *rec_set_start;
	/** Valid elements in the array                                   */
	size_t rec_size;
	/** Max size of the array                                         */
	size_t rec_max;

	/** Array of prepared packets (valid when insert_lock == true)    */
	struct packet_range **pkt_arr;
	/** Valid elements in the array of prepared packets               */
	size_t pkt_size;
	/** Max size of the array                                         */
	size_t pkt_max;

	/** Type of the last inserted Set                                 */
	enum FLOW_SET_TYPE last_set_type;
	/** Pointer to the header of the last Set                         */
	struct ipfix_set_header *last_set_header;
};

/**
 * \brief Main structure of the packet builder
 */
struct _fwd_bldr {
	/** All parts (templates & data sets) of the packet               */
	struct packet_parts *part_all;
	/** Sets & templates headers (always first 4 bytes)               */
	struct aux_array *headers;

	/** IPFIX packet header                                           */
	struct ipfix_header packet_header;

	/** Packet is closed (adding another parts is not permitted)      */
	bool is_complete;
};


/**
 * \brief Create auxiliary array
 * \param[in] elem_size Size of each element
 * \return Pointer or NULL
 */
static struct aux_array *arr_create(size_t elem_size)
{
	// Create a structure
	struct aux_array *res;
	res = (struct aux_array *) calloc(1, sizeof(struct aux_array));
	if (!res) {
		MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)",
			__FILE__, __LINE__);
		return NULL;
	}

	// Create new elements
	res->arr_max = AUX_ARR_DEF_SIZE;
	res->arr_size = 0;
	res->elem_size = elem_size;

	res->arr_data = (uint8_t **) calloc(res->arr_max, sizeof(uint8_t *));
	if (!res->arr_data) {
		MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)",
			__FILE__, __LINE__);
		free(res);
		return NULL;
	}

	return res;
}

/**
 * \brief Destroy auxiliary array
 * \param[in,out] arr Auxiliary array
 */
static void arr_destroy(struct aux_array *arr)
{
	if (!arr) {
		return;
	}

	for (size_t i = 0; i < arr->arr_max; ++i) {
		free(arr->arr_data[i]);
	}

	free(arr->arr_data);
	free(arr);
}

/**
 * \brief Clear all elements
 * \param[in,out] arr Auxiliary array
 */
static void arr_clear(struct aux_array *arr)
{
	arr->arr_size = 0;
}

/**
 * \brief Get a pointer to new element
 * \param[in,out] arr Auxiliary array
 * \warning Content of pointed memory is not initalized.
 * \return Pointer or NULL
 */
static void *arr_new(struct aux_array *arr)
{
	// Check if there is still empty memory block
	if (arr->arr_size == arr->arr_max) {
		// The array is full -> realloc
		size_t new_max = 2 * arr->arr_max;
		uint8_t **new_arr = (uint8_t **) realloc(arr->arr_data,
			new_max * sizeof(uint8_t *));
		if (!new_arr) {
			MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)",
				__FILE__, __LINE__);
			return NULL;
		}

		// Initialize new elements
		for (size_t i = arr->arr_max; i < new_max; ++i) {
			new_arr[i] = NULL;
		}

		arr->arr_data = new_arr;
		arr->arr_max = new_max;
	}

	// Check if memory block is initialized
	uint8_t **res = &arr->arr_data[arr->arr_size++];

	if (*res == NULL) {
		*res = (uint8_t *) calloc(1, arr->elem_size);
		if (*res == NULL) {
			MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)",
				__FILE__, __LINE__);
			arr->arr_size--;
			return NULL;
		}
	}

	return *res;
}

/**
 * \brief Clear parts of IPFIX packet
 * \param[in,out] prt Structure
 */
static void parts_clear(struct packet_parts *prt)
{
	prt->insert_lock = false;
	prt->pkt_size = 0;

	prt->last_set_type = FST_NONE;
	prt->last_set_header = NULL;

	// First (index 0) element is reserved for IPFIX packet header
	prt->rec_size = 1;
}

/**
 * \brief Create a structure for storing pointers to parts of IPFIX packet
 * \return Pointer or NULL
 */
static struct packet_parts *parts_create()
{
	// Create a structure
	struct packet_parts *res;
	res = (struct packet_parts *) calloc(1, sizeof(struct packet_parts));
	if (!res) {
		MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)",
			__FILE__, __LINE__);
		return NULL;
	}

	// Prepare parts of message
	res->rec_size = 0;
	res->rec_max = PARTS_DEF_SIZE;

	res->rec_flds = (struct iovec *) calloc(res->rec_max, sizeof(struct iovec));
	if (!res->rec_flds) {
		MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)",
			__FILE__, __LINE__);
		free(res);
		return NULL;
	}

	res->rec_cnt = (unsigned int *) calloc(res->rec_max, sizeof(unsigned int));
	if (!res->rec_cnt) {
		MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)",
			__FILE__, __LINE__);
		free(res->rec_flds);
		free(res);
		return NULL;
	}

	res->rec_set_start = (bool *) calloc(res->rec_max, sizeof(bool));
	if (!res->rec_set_start) {
		MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)",
			__FILE__, __LINE__);
		free(res->rec_flds);
		free(res->rec_cnt);
		free(res);
		return NULL;
	}

	res->pkt_size = 0;
	res->pkt_max = PACKET_MAX_NUM;
	res->pkt_arr = (struct packet_range **) calloc(res->pkt_max,
		sizeof(struct packet_range *));
	if (!res->pkt_arr) {
		MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)",
			__FILE__, __LINE__);
		free(res->rec_set_start);
		free(res->rec_flds);
		free(res->rec_cnt);
		free(res);
		return NULL;
	}

	// Initialization of a reserved header (of a packet)
	res->rec_flds[0].iov_base = NULL;
	res->rec_flds[0].iov_len = 0;
	res->rec_cnt[0] = 0;
	res->rec_set_start[0] = false;

	parts_clear(res);
	return res;
}

/**
 * \brief Destroy the structure
 * \param[in,out] prt Structure
 */
static void parts_destroy(struct packet_parts *prt)
{
	if (!prt) {
		return;
	}

	for (size_t i = 0; i < prt->pkt_max; ++i) {
		free(prt->pkt_arr[i]);
	}

	free(prt->pkt_arr);
	free(prt->rec_set_start);
	free(prt->rec_cnt);
	free(prt->rec_flds);
	free(prt);
}

/**
 * \brief Check if there is enought space for further insertion of new parts
 * \param[in] prt Structure
 * \return On success returns 0. Otherwise returns non-zero value.
 */
static int parts_check_size(struct packet_parts *prt)
{
	if (prt->rec_size < prt->rec_max) {
		return 0;
	}

	size_t new_max = 2 * prt->rec_max;

	// Reallocate
	struct iovec *new_arr = (struct iovec *) realloc(prt->rec_flds,
		new_max * sizeof(struct iovec));
	if (!new_arr) {
		MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)",
			__FILE__, __LINE__);
		return 1;
	}

	prt->rec_flds = new_arr;

	unsigned int *new_cnt = (unsigned int *) realloc(prt->rec_cnt,
		new_max * sizeof(unsigned int));
	if (!new_cnt) {
		MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)",
			__FILE__, __LINE__);
		return 1;
	}

	prt->rec_cnt = new_cnt;

	bool *new_rec_set = (bool *) realloc(prt->rec_set_start,
		new_max * sizeof(bool));
	if (!new_rec_set) {
		MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)",
			__FILE__, __LINE__);
		return 1;
	}

	prt->rec_set_start = new_rec_set;
	prt->rec_max = new_max;
	return 0;
}

/**
 * \brief Insert a new part of a packet
 * \param[in,out] prt Structure for parts
 * \param[in] data Pointer to the new part
 * \param[in] len Length of the new part
 * \param[in] set_start Start of a data/template set flag
 * \param[in] data_rec Data records in the new part
 * \return On success returns 0. Otherwise returns non-zero value.
 */
static int parts_insert(struct packet_parts *prt, const void *data, size_t len,
	bool set_start, unsigned int data_rec)
{
	if (prt->insert_lock) {
		return 1;
	}

	if (parts_check_size(prt)) {
		return 1;
	}

	prt->rec_flds[prt->rec_size].iov_base = (void *) data; // Ugly :(
	prt->rec_flds[prt->rec_size].iov_len = len;
	prt->rec_cnt[prt->rec_size] = data_rec;
	prt->rec_set_start[prt->rec_size] = set_start;

	prt->rec_size++;
	return 0;
}

/**
 * \brief Get a number of parts of one (data/template) set in internal
 * structures
 * \param[in] prt Structure for packet parts
 * \param[in] set_idx Index of a part with a header of the set
 * \return If the \p set_idx is not a header of the set returns -1.
 * If \p set_idx is out of range returns 0. Otherwise returns number of parts.
 */
static int parts_aux_set_parts(const struct packet_parts *prt, size_t set_idx)
{
	if (set_idx >= prt->rec_size) {
		return 0;
	}

	if (prt->rec_set_start[set_idx] != true) {
		// Not start of a data/template set
		return -1;
	}

	int parts = 0;
	for (size_t i = set_idx; i < prt->rec_size; ++i) {
		if (i != set_idx && prt->rec_set_start[i] == true) {
			// Start of a next set
			break;
		}

		parts++;
	}

	return parts;
}

/**
 * \brief Get a number of bytes of one (data/template) set in internal
 * structures
 * \param[in] pkt Structure for packet parts
 * \param[in] set_idx Index of a part with a header of the set
 * \return If the \p set_idx is not a header of the set returns -1.
 * If \p set_idx is out of range returns 0. Otherwise returns number of bytes.
 */
static int parts_aux_set_len(const struct packet_parts *prt, size_t set_idx)
{
	const int parts = parts_aux_set_parts(prt, set_idx);
	if (parts <= 0) {
		return parts;
	}

	int size = 0;
	for (size_t i = 0; i < ((unsigned int) parts); ++i) {
		size += prt->rec_flds[set_idx + i].iov_len;
	}

	return size;
}

/**
 * \brief Check if there is enought space for further insertion of packet ranges
 * \param[in] prt Structure for packet parts
 * \return On success returns 0. Otherwise returns non-zero value.
 */
static int parts_packets_check_size(struct packet_parts *prt)
{
	if (prt->pkt_size < prt->pkt_max) {
		return 0;
	}

	// Reallocation
	size_t new_max = 2 * prt->pkt_max;
	struct packet_range **new_ranges = (struct packet_range **)
			realloc(prt->pkt_arr, new_max * sizeof(struct packet_range *));
	if (!new_ranges) {
		MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)",
			__FILE__, __LINE__);
		return 1;
	}

	// Initialization of new elements
	for (size_t i = prt->pkt_max; i < new_max; ++i) {
		new_ranges[i] = NULL;
	}

	prt->pkt_arr = new_ranges;
	prt->pkt_max = new_max;
	return 0;
}

/**
 * \brief Create info about a packet (start position, parts count, etc.)
 * \param[in,out] prt Strucutre for packet parts
 * \param[in] start_index Index of first part (first set)
 * \param[in] parts Number of parts
 * \return On success returns 0. Otherwise returns non-zero value.
 */
static int parts_packet_new(struct packet_parts *prt, int start_index,
	size_t parts)
{
	if (parts == 0) {
		return 1;
	}

	if (parts_packets_check_size(prt)) {
		return 1;
	}

	// Check if memory block was initialized
	struct packet_range **res = &prt->pkt_arr[prt->pkt_size++];
	if (*res == NULL) {
		*res = (struct packet_range *) calloc(1, sizeof(struct packet_range));
		if (*res == NULL) {
			MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)",
				__FILE__, __LINE__);
			prt->pkt_size--;
			return 1;
		}
	}

	struct packet_range *range = *res;

	/*
	 * First position must be always reserved for IPFIX packet header, but
	 * "start_index" points to the first set of the packet, therefore -1.
	 * Because future usage can cause that the last part of previous packet
	 * can be lost/modified, we have to backup last part of every packet
	 * and recover it later (just for sure).
	 */
	range->start = &prt->rec_flds[start_index - 1];
	range->size = parts + 1;
	range->backup = prt->rec_flds[start_index + parts - 1];

	unsigned int cnt = 0;
	for (size_t i = 0; i < parts; ++i) {
		cnt += prt->rec_cnt[start_index + i];
	}
	range->rec_cnt = cnt;

	return 0;
}

/**
 * \brief Break parts into separate packets
 * \param[in,out] prt Structure for packet parts
 * \param[in] size Maximal size per one packet (can be exceeded if necessary)
 * \warning After this function, do not use parts_insert()
 * \return On success returns 0. Otherwise returns non-zero value.
 */
static int parts_packets_prepare(struct packet_parts *prt, uint16_t size)
{
	// Prevent further insertion
	prt->insert_lock = true;

	if (prt->rec_size == 1) {
		// Only IPFIX packet header -> nothing to do
		return 0;
	}

	// Create parts
	bool packet_start = true;
	const size_t max_size = (size < PACKET_MIN_SIZE) ? PACKET_MIN_SIZE : size;

	int start_index = 1;             // Start of a packet
	size_t cur_len = 0;              // Length in bytes
	unsigned int parts_cnt = 0;      // Added parts
	unsigned int packets_cnt = 0;    // Number of packets

	/*
	 *  Start from 2. element (i = 1), because first element is reserved
	 *  for IPFIX packet header
	 */
	size_t i = 1;
	while (i < prt->rec_size) {
		if (packet_start) {
			start_index = i;
			cur_len = IPFIX_HEADER_LENGTH;
			parts_cnt = 0;
			packet_start = false;
			packets_cnt++;
		}

		// Get properties of next data/template set
		int next_len = parts_aux_set_len(prt, i);
		if (next_len <= 0) {
			MSG_ERROR(msg_module, "Internal error (%s:%d)", __FILE__, __LINE__);
			return 1;
		}

		int next_parts = parts_aux_set_parts(prt, i);
		if (next_parts <= 0) {
			MSG_ERROR(msg_module, "Internal error (%s:%d)", __FILE__, __LINE__);
			return 1;
		}

		// Can fit into current packet?
		if (cur_len + (size_t) next_len <= max_size || parts_cnt == 0) {
			i += next_parts;
			parts_cnt += next_parts;
			cur_len += next_len;
			continue;
		}

		// Store info about current packet
		if (parts_packet_new(prt, start_index, parts_cnt)) {
			return 1;
		}

		packet_start = true;
	}

	if (i != prt->rec_size) {
		MSG_ERROR(msg_module, "Internal error (%s:%d)", __FILE__, __LINE__);
		return 1;
	}

	if (packets_cnt > 0) {
		// Store last packet
		if (parts_packet_new(prt, start_index, parts_cnt)) {
			return 1;
		}
	}

	return 0;
}

/* Create a packet builder */
fwd_bldr_t *bldr_create()
{
	// Create a structure
	fwd_bldr_t *res;
	res = (fwd_bldr_t *) calloc(1, sizeof(fwd_bldr_t));
	if (!res) {
		MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)",
			__FILE__, __LINE__);
		return NULL;
	}

	res->part_all = parts_create();
	if (!res->part_all) {
		bldr_destroy(res);
		return NULL;
	}

	res->headers = arr_create(HEADER_SIZE);
	if (!res->headers) {
		bldr_destroy(res);
		return NULL;
	}

	res->packet_header.version = htons(IPFIX_VERSION);
	res->is_complete = false;
	return res;
}

/* Destroy a packet builder */
void bldr_destroy(fwd_bldr_t *pkt)
{
	if (!pkt) {
		return;
	}

	arr_destroy(pkt->headers);
	parts_destroy(pkt->part_all);
	free(pkt);
}

/* Start of a new packet */
void bldr_start(fwd_bldr_t *pkt, uint32_t odid,	uint32_t exp_time)
{
	pkt->packet_header.export_time = htonl(exp_time);
	pkt->packet_header.observation_domain_id = htonl(odid);

	pkt->is_complete = false;

	parts_clear(pkt->part_all);
	arr_clear(pkt->headers);
}

/* End of a new packet(s) */
int bldr_end(fwd_bldr_t *pkt, uint16_t len)
{
	pkt->is_complete = true;
	if (parts_packets_prepare(pkt->part_all, len)) {
		return -1;
	}

	return 0;
}

/* Get a number of generated packets */
int bldr_pkts_cnt(const fwd_bldr_t *pkt)
{
	if (!pkt->is_complete) {
		return -1;
	}

	return pkt->part_all->pkt_size;
}

/* Get an Observation Domain ID (ODID) of packets */
uint32_t bldr_pkts_get_odid(const fwd_bldr_t *pkt)
{
	return ntohl(pkt->packet_header.observation_domain_id);
}

/**
 * \brief Get a packet defined by an index (auxiliary function)
 * \param[in,out] pkt Structure for packet builder
 * \param[in] seq_num IPFIX Sequence number
 * \param[in] idx Index of the packet i.e. idx < bldr_pkts_cnt()
 * \param[out] out_range Packet range (from first to last part)
 * \param[out] out_len Total lenght of the packet (in octets)
 * \return On success return 0. Otherwise returns non-zero value.
 */
static int bldr_pkts_get(fwd_bldr_t *pkt, uint32_t seq_num, size_t idx,
	struct packet_range **out_range, size_t *out_len)
{
	if (!pkt->is_complete) {
		// Internal structure not prepared
		return 1;
	}

	// Get the packet
	struct packet_parts *parts = pkt->part_all;
	if (idx >= parts->pkt_size) {
		// Out of range
		return 1;
	}

	struct packet_range *range = parts->pkt_arr[idx];
	// Recover the last value from a backup (just for sure)
	range->start[range->size - 1] = range->backup;

	// Get total length
	size_t total_len = IPFIX_HEADER_LENGTH;
	for (size_t i = 1; i < range->size; ++i) {
		// Calculate a length of the packet
		total_len += range->start[i].iov_len;
	}

	// Prepare packet header
	range->start[0].iov_base = &pkt->packet_header;
	range->start[0].iov_len = IPFIX_HEADER_LENGTH;
	pkt->packet_header.length = htons(total_len);
	pkt->packet_header.sequence_number = htonl(seq_num);

	*out_range = range;
	*out_len = total_len;
	return 0;
}

/* Get a packet defined by index in binary format */
int bldr_pkts_raw(fwd_bldr_t *pkt, uint32_t seq_num, size_t idx, size_t offset,
	char **new_packet, size_t *size, size_t *rec_cnt)
{
	struct packet_range *range;
	size_t packet_len;

	if (bldr_pkts_get(pkt, seq_num, idx, &range, &packet_len)) {
		return 1;
	}

	if (packet_len <= offset) {
		// Offset is out of range
		return 1;
	}

	packet_len -= offset;

	char *packet = (char *) malloc(packet_len);
	if (!packet) {
		MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)",
			__FILE__, __LINE__);
		return 1;
	}

	// Copy data
	unsigned int pos_total = 0;
	unsigned int pos_copy = 0;

	for (size_t i = 0; i < range->size; ++i) {
		char *begin = (char *) range->start[i].iov_base;
		size_t len = range->start[i].iov_len;

		if (pos_total < offset) {
			// Skip parts within offset area
			if (pos_total + len <= offset) {
				pos_total += len;
				continue;
			}

			// Move pointers to the end of the offset
			unsigned int diff = offset - pos_total;
			begin += diff;
			len -= diff;
			pos_total += diff;
		}

		memcpy(packet + pos_copy, begin, len);
		pos_copy += len;
		pos_total += len;
	}

	*new_packet = packet;
	*size = packet_len;
	*rec_cnt = range->rec_cnt;

	return 0;
}

/* Get a packet defined by index in the format suitable for sendmsg() */
int bldr_pkts_iovec(fwd_bldr_t *pkt, uint32_t seq_num,
	size_t idx, struct iovec **io, size_t *size, size_t *rec_cnt)
{
	struct packet_range *range;
	size_t packet_len;

	if (bldr_pkts_get(pkt, seq_num, idx, &range, &packet_len)) {
		return 1;
	}

	*io = range->start;
	*size = range->size;
	*rec_cnt = range->rec_cnt;

	return 0;
}

/* Add a Data set */
int bldr_add_dataset(fwd_bldr_t *pkt, const struct ipfix_data_set *data,
	uint16_t new_id, unsigned int rec)
{
	struct packet_parts *parts = pkt->part_all;
	parts->last_set_type = FST_DATA;
	parts->last_set_header = NULL;

	if (ntohs(data->header.flowset_id) != new_id) {
		// Add a new header
		struct ipfix_set_header *new_header = arr_new(pkt->headers);
		if (!new_header) {
			return 1;
		}

		new_header->length = data->header.length;
		new_header->flowset_id = htons(new_id);

		if (parts_insert(parts, new_header, HEADER_SIZE, true, 0)) {
			// Unable to insert the new header
			return 1;
		}

		// Add the rest of the Data set
		uint16_t body_size = ntohs(data->header.length) - HEADER_SIZE;
		uint8_t *body_ptr = ((uint8_t *) data) + HEADER_SIZE;

		if (parts_insert(parts, body_ptr, body_size, false, rec)) {
			return 1;
		}
	} else {
		// Just insert the Data set (no header change)
		const uint16_t size = ntohs(data->header.length);
		if (parts_insert(parts, data, size, true, rec)) {
			return 1;
		}
	}

	return 0;
}

/**
 * \brief Create and add a header of a (Options) Template Set
 * \param[in,out] pkt Packet builder
 * \param[in] type Type of the template (#TM_TEMPLATE or #TM_OPTIONS_TEMPLATE)
 * \param[in] is_withdrawal Is it a set for template withdrawals
 * \return On success returns 0. Otherwise returns non-zero value.
 */
static int bldr_aux_insert_set_header(fwd_bldr_t *pkt, int type,
	bool is_withdrawal)
{
	if (type != TM_TEMPLATE && type != TM_OPTIONS_TEMPLATE) {
		return 1;
	}

	struct packet_parts *parts = pkt->part_all;
	struct ipfix_set_header *new_header = arr_new(pkt->headers);
	if (!new_header) {
		return 1;
	}

	new_header->flowset_id = (type == TM_TEMPLATE)
		? htons(IPFIX_TEMPLATE_FLOWSET_ID)
		: htons(IPFIX_OPTION_FLOWSET_ID);
	new_header->length = htons(HEADER_SIZE);

	if (parts_insert(parts, new_header, HEADER_SIZE, true, 0)) {
		return 1;
	}

	parts->last_set_header = new_header;

	if (is_withdrawal) {
		// Template Withdrawal set
		parts->last_set_type = (type == TM_TEMPLATE)
			? FST_TMPLT_WITHDRAW : FST_OPT_TMPLT_WITHDRAW;
	} else {
		// Template Definition set
		parts->last_set_type = (type == TM_TEMPLATE)
			? FST_TMPLT : FST_OPT_TMPLT;
	}

	return 0;
}

/* Add a template */
int bldr_add_template(fwd_bldr_t *pkt, const void *data, size_t size,
	uint16_t new_id, int type)
{
	if (type != TM_TEMPLATE && type != TM_OPTIONS_TEMPLATE) {
		return 1;
	}

	struct packet_parts *parts = pkt->part_all;
	const enum FLOW_SET_TYPE prev_set = parts->last_set_type;

	/*
	 * Add a new header of a (Options) Template Set or use a previous Set of
	 * the same type and just update its header.
	 */
	bool new_set = true;
	if ((prev_set == FST_TMPLT && type == TM_TEMPLATE) ||
		(prev_set == FST_OPT_TMPLT && type == TM_OPTIONS_TEMPLATE)) {
		// A previous set of the same type exists
		new_set = false;

		size_t set_size = ntohs(parts->last_set_header->length);
		if (set_size + size > TMPLT_SET_MAX_LEN) {
			// Expanded template set would be too long, create a new one.
			new_set = true;
		}
	}

	if (new_set && bldr_aux_insert_set_header(pkt, type, false)) {
		return 1;
	}

	// Add the (Options) Template
	const struct ipfix_template_record *tmplt_header = data;
	if (ntohs(tmplt_header->template_id) != new_id) {
		// Add a new header
		struct ipfix_template_record *new_header = arr_new(pkt->headers);
		if (!new_header) {
			return 1;
		}

		new_header->template_id = htons(new_id);
		new_header->count = tmplt_header->count;

		if (parts_insert(parts, new_header, HEADER_SIZE, false, 0)) {
			return 1;
		}

		// Add the rest of the template
		size_t body_size = size - HEADER_SIZE;
		uint8_t *body_ptr = ((uint8_t *) data) + HEADER_SIZE;

		if (parts_insert(parts, body_ptr, body_size, false, 0)) {
			return 1;
		}
	} else {
		// Just insert the template
		if (parts_insert(parts, data, size, false, 0)) {
			return 1;
		}
	}

	// Update the header of the Template Set
	struct ipfix_set_header *header = parts->last_set_header;
	header->length = htons(ntohs(header->length) + size);
	return 0;
}

/* Add a template withdrawal */
int bldr_add_template_withdrawal(fwd_bldr_t *pkt, uint16_t id, int type)
{
	if (type != TM_TEMPLATE && type != TM_OPTIONS_TEMPLATE) {
		return 1;
	}

	struct packet_parts *parts = pkt->part_all;
	const enum FLOW_SET_TYPE prev_set = parts->last_set_type;

	/*
	 * Add a new header of a (Options) Template Set or use a previous Set of
	 * the same type and just update its header.
	 */
	bool new_set = true;
	if ((prev_set == FST_TMPLT_WITHDRAW && type == TM_TEMPLATE) ||
		(prev_set == FST_OPT_TMPLT_WITHDRAW && type == TM_OPTIONS_TEMPLATE)) {
		// A previous set of the same type exists
		new_set = false;

		size_t set_size = ntohs(parts->last_set_header->length);
		// Every withdrawal is 4 bytes (HEAD_SIZE)
		if (set_size + HEADER_SIZE > TMPLT_SET_MAX_LEN) {
			new_set = true;
		}
	}

	if (new_set && bldr_aux_insert_set_header(pkt, type, true)) {
		return 1;
	}

	// Add the template
	struct ipfix_template_record *new_record = arr_new(pkt->headers);
	if (!new_record) {
		return 1;
	}

	new_record->template_id = htons(id);
	new_record->count = htons(0); // Withdrawal identification

	if (parts_insert(parts, new_record, HEADER_SIZE, false, 0)) {
		return 1;
	}

	// Update the header of the Template Set
	struct ipfix_set_header *header = parts->last_set_header;
	header->length = htons(ntohs(header->length) + HEADER_SIZE);
	return 0;
}


