/**
 * \file utils/conversion/convert.c
 * \author Michal Kozubik <kozubik.michal@gmail.com>
 * \brief Packet conversion from Netflow v5/v9 or sFlow to IPFIX format.
 *
 * Copyright (C) 2015 CESNET, z.s.p.o.
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

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

#include <ipfixcol.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "convert.h"
#ifdef ENABLE_SFLOW
#include "sflow.h"
#include "sflowtool.h"
#endif

#define NETFLOW_V5_VERSION 5
#define NETFLOW_V9_VERSION 9

#define NETFLOW_V5_TEMPLATE_LEN 76
#define NETFLOW_V5_DATA_SET_LEN 52
#define NETFLOW_V5_NUM_OF_FIELDS 17
#define NETFLOW_V5_MAX_RECORD_COUNT 30

#define NETFLOW_V9_TEMPLATE_SET_ID 0
#define NETFLOW_V9_OPT_TEMPLATE_SET_ID 1

#define NETFLOW_V9_END_ELEM 21
#define NETFLOW_V9_START_ELEM 22

/* Offsets of timestamps in netflow v5 data record */
#define FIRST_OFFSET 24
#define LAST_OFFSET 28

/* IPFIX Element IDs used when creating Template Set */
#define SRC_IPV4_ADDR 8
#define DST_IPV4_ADDR 12
#define NEXTHOP_IPV4_ADDR 15
#define INGRESS_INTERFACE 10
#define EGRESS_INTERFACE 14
#define PACKETS 2
#define OCTETS 1
#define FLOW_START 152
#define FLOW_END 153
#define SRC_PORT 7
#define DST_PORT 11
#define PADDING 210
#define TCP_FLAGS 6
#define PROTO 4
#define TOS 5
#define SRC_AS 16
#define DST_AS 17

/* Defines for numbers of bytes */
#define BYTES_1 1
#define BYTES_2 2
#define BYTES_4 4
#define BYTES_8 8
#define BYTES_12 12

/* Defines for enterprise numbers unpacking in Netflow v9 */
#define DEFAULT_ENTERPRISE_NUMBER (~((uint32_t) 0))
#define ENTERPRISE_BIT 0x8000
#define TEMPLATE_ROW_SIZE 4

/** Identifier to MSG_* macros */
static char *msg_module = "convert";

/* Static creation of Netflow v5 Template Set */
static uint16_t netflow_v5_template[NETFLOW_V5_TEMPLATE_LEN/2]={\
		IPFIX_TEMPLATE_FLOWSET_ID,   NETFLOW_V5_TEMPLATE_LEN,\
		IPFIX_MIN_RECORD_FLOWSET_ID, NETFLOW_V5_NUM_OF_FIELDS,
		SRC_IPV4_ADDR, 				 BYTES_4,\
		DST_IPV4_ADDR, 				 BYTES_4,\
		NEXTHOP_IPV4_ADDR, 			 BYTES_4,\
		INGRESS_INTERFACE, 			 BYTES_2,\
		EGRESS_INTERFACE, 			 BYTES_2,\
		PACKETS, 					 BYTES_4,\
		OCTETS, 					 BYTES_4,\
		FLOW_START, 				 BYTES_8,\
		FLOW_END, 					 BYTES_8,\
		SRC_PORT, 					 BYTES_2,\
		DST_PORT, 					 BYTES_2,\
		PADDING, 					 BYTES_1,\
		TCP_FLAGS, 					 BYTES_1,\
		PROTO, 						 BYTES_1,\
		TOS, 						 BYTES_1,\
		SRC_AS, 					 BYTES_2,\
		DST_AS, 					 BYTES_2
};

static uint16_t netflow_v5_data_header[2] = {\
		IPFIX_MIN_RECORD_FLOWSET_ID, NETFLOW_V5_DATA_SET_LEN + sizeof(struct ipfix_set_header)
};

/* (New) IPFIX sequence numbers for NFv5, NFv9 and sFlow traffic streams */
static uint32_t ipfix_seq_no[3] = {0, 0, 0};

#define NF5_SEQ_NO  0
#define NF9_SEQ_NO  1
#define SF_SEQ_NO   2

static uint8_t inserted = 0;
static uint8_t plugin = UDP_PLUGIN;
static uint32_t buff_len = 0;

/**
 * \struct input_info_list
 * \brief  List structure for input info
 */
struct input_info_list {
	struct input_info_network info;
	struct input_info_list *next;
	uint32_t last_sent;
	uint16_t packets_sent;
};

struct templates_s {
	uint8_t cols;
	uint32_t max;
	int *templ;
};

static struct templates_s templates;
static struct input_info_list *info_list;

/**
 * \brief Convers static arrays from host to network byte order
 *
 */
static inline void modify()
{
	int i;
	for (i = 0; i < NETFLOW_V5_TEMPLATE_LEN / 2; i++) {
		netflow_v5_template[i] = htons(netflow_v5_template[i]);
	}

	netflow_v5_data_header[0] = htons(netflow_v5_data_header[0]);
	netflow_v5_data_header[1] = htons(netflow_v5_data_header[1]);
}

/**
  * \brief Prepare static variables used for inserting template and data sets
  *
  * Also allocate memory for templates
  *
  * \param[in] in_plugin Type of input plugin (UDP_PLUGIN...)
  * \param[in] len Length of buff used in plugins
  * \return 0 on success
  */
int convert_init(int in_plugin, int len)
{
	/* Initialize templates structure */
	templates.max = 30;
	templates.cols = 2;

	/* Allocate memory */
	templates.templ = malloc(templates.max * templates.cols * sizeof(int));

	if (templates.templ == NULL) {
		return 1;
	}

	/* Initialize allocated memory */
	unsigned int i;
	for (i = 0; i < (templates.max * templates.cols); i++) {
		templates.templ[i] = 0;
	}

	/* Fill in static variables */
	buff_len = len;
	plugin = in_plugin;

	/* Modify static variables for template & data set insertion */
	modify();

	return 0;
}

int templates_realloc()
{
	/* More templates are needed, realloc memory */
	templates.max += 20;

	templates.templ = realloc(templates.templ, templates.max * templates.cols * sizeof(int));

	if (templates.templ == NULL) {
		return 1;
	}

	/* Initialize allocated memory */
	unsigned int i;
	for (i = (templates.max - 20) * templates.cols; i < templates.max * templates.cols; i++) {
		templates.templ[i] = 0;
	}

	return 0;
}

/**
  * \brief Reallocate memory for templates
  */
void convert_close()
{
	free(templates.templ);
}

/**
 * \brief Inserts Template Set into packet and updates input_info
 *
 * Also sets total length of packet
 *
 * \param[out] packet Flow information data in the form of IPFIX packet
 * \param[in] flow_sample_count Number of flow samples in sFlow datagram
 * \param[out] len
 * \return Total length of packet
 */
uint16_t insert_template_set(char **packet, int flow_sample_count, ssize_t *len)
{
	/* Template Set insertion if needed */
	/* Check conf->info_list->info.template_life_packet and template_life_time */
	struct ipfix_header *header = (struct ipfix_header *) *packet;

	/* Remove last 4 bytes (padding) of each data record in packet */
	int i;
	for (i = flow_sample_count - 1; i > 0; i--) {
		uint32_t pos = IPFIX_HEADER_LENGTH + (i * (NETFLOW_V5_DATA_SET_LEN + BYTES_4));
		memmove(*packet + pos - BYTES_4, *packet + pos, (flow_sample_count - i) * NETFLOW_V5_DATA_SET_LEN);
	}
	*len -= (flow_sample_count * BYTES_4);

	/* Insert Data Set header */
	if (flow_sample_count > 0) {
		netflow_v5_data_header[1] = htons(NETFLOW_V5_DATA_SET_LEN * flow_sample_count + sizeof(struct ipfix_set_header));
		memmove(*packet + IPFIX_HEADER_LENGTH + BYTES_4, *packet + IPFIX_HEADER_LENGTH, buff_len - IPFIX_HEADER_LENGTH - BYTES_4);
		memcpy(*packet + IPFIX_HEADER_LENGTH, netflow_v5_data_header, BYTES_4);
		*len += sizeof(struct ipfix_set_header);
	} else {
		*len = IPFIX_HEADER_LENGTH;
	}

	if (plugin == UDP_PLUGIN) {
		uint32_t last = 0;
		if ((info_list == NULL) || ((info_list != NULL) && (info_list->info.template_life_packet == NULL) && (info_list->info.template_life_time == NULL))) {
			if (inserted == 0) {
				inserted = 1;
				memmove(*packet + IPFIX_HEADER_LENGTH + NETFLOW_V5_TEMPLATE_LEN, *packet + IPFIX_HEADER_LENGTH, buff_len - NETFLOW_V5_TEMPLATE_LEN - IPFIX_HEADER_LENGTH);
				memcpy(*packet + IPFIX_HEADER_LENGTH, netflow_v5_template, NETFLOW_V5_TEMPLATE_LEN);
				*len += NETFLOW_V5_TEMPLATE_LEN;
				return htons(IPFIX_HEADER_LENGTH + NETFLOW_V5_TEMPLATE_LEN + sizeof(struct ipfix_set_header) + (NETFLOW_V5_DATA_SET_LEN * flow_sample_count));
			} else {
				return htons(IPFIX_HEADER_LENGTH + sizeof(struct ipfix_set_header) + (NETFLOW_V5_DATA_SET_LEN * flow_sample_count));
			}
		}

		if (info_list != NULL) {
			if (info_list->info.template_life_packet != NULL) {
				if (info_list->packets_sent == strtol(info_list->info.template_life_packet, NULL, 10)) {
					last = ntohl(header->export_time);
				}
			}
			if ((last == 0) && (info_list->info.template_life_time != NULL)) {
				last = info_list->last_sent + strtol(info_list->info.template_life_time, NULL, 10);
				if (flow_sample_count > 0) {
					info_list->packets_sent++;
				}
			}
		}

		if (last <= ntohl(header->export_time)) {
			if (info_list != NULL) {
				info_list->last_sent = ntohl(header->export_time);
				info_list->packets_sent = 1;
			}
			memmove(*packet + IPFIX_HEADER_LENGTH + NETFLOW_V5_TEMPLATE_LEN, *packet + IPFIX_HEADER_LENGTH, buff_len - NETFLOW_V5_TEMPLATE_LEN - IPFIX_HEADER_LENGTH);
			memcpy(*packet + IPFIX_HEADER_LENGTH, netflow_v5_template, NETFLOW_V5_TEMPLATE_LEN);
			*len += NETFLOW_V5_TEMPLATE_LEN;

			return htons(IPFIX_HEADER_LENGTH + NETFLOW_V5_TEMPLATE_LEN + sizeof(struct ipfix_set_header) + (NETFLOW_V5_DATA_SET_LEN * flow_sample_count));
		} else {
			return htons(IPFIX_HEADER_LENGTH + sizeof(struct ipfix_set_header) + (NETFLOW_V5_DATA_SET_LEN * flow_sample_count));
		}
	} else if (inserted == 0) {
		inserted = 1;

		memmove(*packet + IPFIX_HEADER_LENGTH + NETFLOW_V5_TEMPLATE_LEN, *packet + IPFIX_HEADER_LENGTH, buff_len - NETFLOW_V5_TEMPLATE_LEN - IPFIX_HEADER_LENGTH);
		memcpy(*packet + IPFIX_HEADER_LENGTH, netflow_v5_template, NETFLOW_V5_TEMPLATE_LEN);
		*len += NETFLOW_V5_TEMPLATE_LEN;

		return htons(IPFIX_HEADER_LENGTH + NETFLOW_V5_TEMPLATE_LEN + sizeof(struct ipfix_set_header) + (NETFLOW_V5_DATA_SET_LEN * flow_sample_count));
	} else {
		return htons(IPFIX_HEADER_LENGTH + sizeof(struct ipfix_set_header) + (NETFLOW_V5_DATA_SET_LEN * flow_sample_count));
	}
}

/* \brief Inserts 64b timestamps into NetFlow v9 template
 *
 * Finds original timestamps (field ID 21 and 22) and replaces them with field ID 152 and 153
 *
 * \param templSet pointer to Template Set
 */
int insert_timestamp_template(struct ipfix_set_header *templSet)
{
	struct ipfix_template_record *tmp;
	uint16_t len, num, i, id;


	/* Get template set total length without set header length */
	len = ntohs(templSet->length) - sizeof(struct ipfix_template_record);

	/* Skip set header */
	tmp = (struct ipfix_template_record*) (templSet + 1);

	/* Iterate through all templates */
	while ((uint8_t *) tmp <= (uint8_t *) templSet + len) {
		/* Get template ID and number of elements */
		id = ntohs(tmp->template_id) - IPFIX_MIN_RECORD_FLOWSET_ID;
		num = ntohs(tmp->count);

		if (id >= templates.max) {
			templates.max += 20;
			templates.templ = realloc(templates.templ, templates.max * templates.cols * sizeof(int));

			if (templates.templ == NULL) {
				MSG_DEBUG(msg_module, "Failure to allocate templates.templ");
				return 1;
			}

			unsigned int i;
			for (i = (templates.max - 20) * templates.cols; i < templates.max * templates.cols; i++) {
				templates.templ[i] = 0;
			}
		}

		uint32_t len = templates.cols * id;
		uint32_t pos = len + 1;

		/* Input check: avoid overflow due to malicious flowset IDs */
		if (len > templates.max * templates.cols) {
			MSG_DEBUG(msg_module, "Failure due to malicious flowset ID, len=%lu", len);
			MSG_DEBUG(msg_module, "len=%lu, templates.max=%lu, templates.cols=%lu", len, templates.max, templates.cols);
			return 1;
		}

		/* Set default values for length and timestamp position */
		templates.templ[len] = 0;
		templates.templ[pos] = -1;

		/* Skip template record header */
		tmp = (struct ipfix_template_record*) (((uint8_t *) tmp) + 4);

		/* Iterate through all elements */
		for (i = 0; i < num; i++) {
			/* We will misuse ipfix_template_record to get to individual elements
			   Enterprise numbers are processed as well, hopefully they will not match */

			/* We are looking for timestamps - elements with id 21 (end) and 22 (start) */
			if (ntohs(tmp->template_id) == NETFLOW_V9_END_ELEM) {
				/* We don't know which one comes first so we need to check it */
				if (templates.templ[pos] == -1) {
					templates.templ[pos] = templates.templ[len];
				}

				/* Change element ID and element length (32b -> 64b) */
				tmp->template_id = htons(FLOW_END);
				tmp->count = htons(BYTES_8);
				templates.templ[len] += BYTES_4;
			} else if (ntohs(tmp->template_id) == NETFLOW_V9_START_ELEM) {
				/* Do the same thing for element 22 */
				if (templates.templ[pos] == -1) {
					templates.templ[pos] = templates.templ[len];
				}

				tmp->template_id = htons(FLOW_START);
				tmp->count = htons(BYTES_8);
				templates.templ[len] += BYTES_4;
			} else {
				templates.templ[len] += ntohs(tmp->count);
			}
		tmp = (struct ipfix_template_record*) (((uint8_t *) tmp) + 4);
		}
	}

	return 0;
}

/* \brief Inserts 64b timestamps into NetFlow v9 OPTIONS template
 *
 * Finds original timestamps (field ID 21 and 22) and replaces them with field ID 152 and 153
 *
 * \param templSet pointer to Template Set
 */
int insert_timestamp_otemplate(struct ipfix_set_header *templSet)
{
	struct ipfix_options_template_record *tmp;
	uint16_t len, num, i, id;

	/* Get template set total length without set header length */
	len = ntohs(templSet->length) - sizeof(struct ipfix_options_template_record);

	/* Skip set header */
	tmp = (struct ipfix_options_template_record*) (templSet + 1);

	/* Iterate through all templates */
	while ((uint8_t *) tmp <= (uint8_t *) templSet + len) {
		/* Get template ID and number of elements */
		id = ntohs(tmp->template_id) - IPFIX_MIN_RECORD_FLOWSET_ID;
		num = (ntohs(tmp->count) + ntohs(tmp->scope_field_count)) / 4;

		if (id >= templates.max) {
			templates.max += 20;
			templates.templ = realloc(templates.templ, templates.max * templates.cols * sizeof(int));

			if (templates.templ == NULL) {
				MSG_DEBUG(msg_module, "Failure to allocate templates.templ");
				return 1;
			}

			unsigned int i;
			for (i = (templates.max - 20) * templates.cols; i < templates.max * templates.cols; i++) {
				templates.templ[i] = 0;
			}
		}

		uint32_t len = templates.cols * id;
		uint32_t pos = len + 1;

		/* Input check: avoid overflow due to malicious flowset IDs */
		if (len > templates.max * templates.cols) {
			MSG_DEBUG(msg_module, "Failure due to malicious flowset ID, len=%lu", len);
			MSG_DEBUG(msg_module, "len=%lu, templates.max=%lu, templates.cols=%lu", len, templates.max, templates.cols);
			return 1;
		}

		/* Set default values for length and timestamp position */
		templates.templ[len] = 0;
		templates.templ[pos] = -1;

		/* Skip option template record header */
		tmp = (struct ipfix_options_template_record*) (((uint8_t *) tmp) + 6);

		/* Iterate through all elements */
		for (i = 0; i < num; i++) {
			/* We will misuse ipfix_template_record to get to individual elements
			   Enterprise numbers are processed as well, hopefully they will not match */

			/* We are looking for timestamps - elements with id 21 (end) and 22 (start) */
			if (ntohs(tmp->template_id) == NETFLOW_V9_END_ELEM) {
				/* We don't know which one comes first so we need to check it */
				if (templates.templ[pos] == -1) {
					templates.templ[pos] = templates.templ[len];
				}

				/* Change element ID and element length (32b -> 64b) */
				tmp->template_id = htons(FLOW_END);
				tmp->count = htons(BYTES_8);
				templates.templ[len] += BYTES_4;
			} else if (ntohs(tmp->template_id) == NETFLOW_V9_START_ELEM) {
				/* Do the same thing for element 22 */
				if (templates.templ[pos] == -1) {
					templates.templ[pos] = templates.templ[len];
				}

				tmp->template_id = htons(FLOW_START);
				tmp->count = htons(BYTES_8);
				templates.templ[len] += BYTES_4;
			} else {
				templates.templ[len] += ntohs(tmp->count);
			}

			tmp = (struct ipfix_options_template_record *) (((uint8_t *) tmp) + 4);
		}
	}

	return 0;
}

/**
 * \brief Inserts 64b timestamps into NetFlow v9 Data Set
 *
 * Finds original timestamps and replaces them by 64b IPFIX timestamps
 *
 * \param dataSet pointer to Data Set
 * \param time_header time from packet header
 * \param remaining bytes behind this data set in packet
 */
int insert_timestamp_data(struct ipfix_set_header *dataSet, uint64_t time_header, uint32_t remaining)
{
	struct ipfix_set_header *tmp;
	uint8_t *pkt;
	uint16_t id, len, num, shifted;
	int32_t first_offset, last_offset;
	int i;

	tmp = dataSet;

	/* Get used template ID and data set length */
	id = ntohs(tmp->flowset_id) - IPFIX_MIN_RECORD_FLOWSET_ID;
	len = ntohs(tmp->length) - 4;

	uint32_t lenIndex = templates.cols * id;
	uint32_t posIndex = lenIndex + 1;

	/* Input check: avoid overflow due to malicious flowset IDs */
	if (lenIndex > templates.max * templates.cols) {
		return 0;
	}

	/* Get number of data records using the same template */
	if (templates.templ[lenIndex] <= 0) {
		return 0;
	}
	num = len / templates.templ[lenIndex];
	if (num == 0) {
		return 0;
	}

	/* Increase sequence number */
	ipfix_seq_no[NF9_SEQ_NO] += num;

	/* Iterate through all data records */
	shifted = 0;
	first_offset = templates.templ[posIndex];
	last_offset = first_offset + BYTES_4;

	/* If there is nothing to insert, return */
	if (first_offset == -1) {
		return 0;
	}

	for (i = num - 1; i >= 0; i--) {
		/* Resize each timestamp in each data record to 64 bit */
		pkt = (uint8_t *) dataSet + BYTES_4 + (i * templates.templ[lenIndex]);
		uint64_t first = ntohl(*((uint32_t *) (pkt + first_offset)));
		uint64_t last  = ntohl(*((uint32_t *) (pkt + last_offset)));

		/* we need more space - 32b -> 64b timestamps => + 8 bytes
		 *
		 * everything behind timestamps in this record must be shifted: (templLen[id][0] + BYTES_4 - last_offset)
		 *
		 * all record behind this one must be shifted: (shifted * (templLen[id][0] + BYTES_8))
		 *
		 * all data/template sets behind this set must be shifted: (remaining - len)
		 */
		memmove(pkt + last_offset + BYTES_8, pkt + last_offset,
				(shifted * (templates.templ[lenIndex] + BYTES_8)) +
				(templates.templ[lenIndex] + BYTES_4 - last_offset) + (remaining - len));

		/* Set time values */
		*((uint64_t *) (pkt + first_offset)) = htobe64(time_header + first);
		*((uint64_t *) (pkt + last_offset + BYTES_4)) = htobe64(time_header + last);

		shifted++;
	}

	/* Increase set header length and packet total length */
	tmp->length = htons(len + BYTES_4 + (shifted * BYTES_8));
	return shifted;
}

/**
 * \brief Unpack Options templates enterprise numbers in Netflow v9
 *
 *  Searches all fields in Netflow v9 template that have
 *  ID bigger than 32767 (== enterprise bit is set to 1)
 *  and converts them into IPFIX enterprise element.
 *
 * \param template_set Template Set
 * \param remaining Number of bytes to the end of packet (including template set)
 * \return Number of inserted bytes
 */
int unpack_ot_enterprise_elements(struct ipfix_set_header *template_set, uint32_t remaining)
{
	struct ipfix_set_header *template_row = template_set;

	/* Get template set total length without set header length */
	uint16_t set_len = ntohs(template_row->length) - sizeof(struct ipfix_set_header);
	uint16_t added_pens = 0; /* Added private enterprise numbers */

	/* Iterate through all templates */
	while ((uint8_t *) template_row < (uint8_t *) template_set + set_len) {
		template_row++;
		remaining -= TEMPLATE_ROW_SIZE;

		/* Get number of elements */
		struct ipfix_options_template_record *tmp = (struct ipfix_options_template_record *) template_row;
		uint16_t element_count = (ntohs(tmp->count) + ntohs(tmp->scope_field_count)) / 4;

		/* Skip extra two bytes in option template record header */
		template_row = (struct ipfix_set_header *) (((uint8_t *) template_row) + BYTES_2);

		/* Iterate through all elements */
		for (uint16_t i = 0; i < element_count; ++i) {
			template_row++;
			remaining -= TEMPLATE_ROW_SIZE;

			uint16_t field_id = ntohs(template_row->flowset_id);
			/* We are only looking for elements with enterprise bit set to 1 */
			if (!(field_id & ENTERPRISE_BIT)) {
				continue;
			}

			/* Move to the enterprise number and create space for it */
			template_row++;
			memmove(((uint8_t*) template_row) + TEMPLATE_ROW_SIZE, template_row, remaining);

			set_len += TEMPLATE_ROW_SIZE;

			/* Add enterprise number */
			*((uint32_t *) template_row) = DEFAULT_ENTERPRISE_NUMBER;

			added_pens++;
		}
	}

	template_set->length = htons(ntohs(template_set->length) + added_pens * TEMPLATE_ROW_SIZE);

	return added_pens * TEMPLATE_ROW_SIZE;
}

/**
 * \brief Unpack enterprise numbers in Netflow v9
 *
 *  Searches all fields in Netflow v9 template that have
 *  ID bigger than 32767 (== enterprise bit is set to 1)
 *  and converts them into IPFIX enterprise element.
 *
 * \param template_set Template Set
 * \param remaining Number of bytes to the end of packet (indluding template set)
 * \return Number of inserted bytes
 */
int unpack_enterprise_elements(struct ipfix_set_header *template_set, uint32_t remaining)
{
	struct ipfix_set_header *template_row = template_set;

	/* Get template set total length without set header length */
	uint16_t set_len = ntohs(template_row->length) - sizeof(struct ipfix_set_header);
	uint16_t added_pens = 0; /* Added private enterprise numbers */

	/* Iterate through all templates */
	while ((uint8_t *) template_row < (uint8_t *) template_set + set_len) {
		template_row++;
		remaining -= TEMPLATE_ROW_SIZE;

		uint16_t numberOfElements = ntohs(template_row->length);

		/* Iterate through all elements */
		for (uint16_t i = 0; i < numberOfElements; ++i) {
			template_row++;
			remaining -= TEMPLATE_ROW_SIZE;

			uint16_t field_id = ntohs(template_row->flowset_id);

			/* We are only looking for elements with enterprise bit set to 1 */
			if (!(field_id & ENTERPRISE_BIT)) {
				continue;
			}

			/* Move to the enterprise number and create space for it */
			template_row++;
			memmove(((uint8_t*) template_row) + TEMPLATE_ROW_SIZE, template_row, remaining);

			set_len += TEMPLATE_ROW_SIZE;

			/* Add enterprise number */
			*((uint32_t *) template_row) = DEFAULT_ENTERPRISE_NUMBER;

			added_pens++;
		}
	}

	template_set->length = htons(ntohs(template_set->length) + added_pens * TEMPLATE_ROW_SIZE);

	return added_pens * TEMPLATE_ROW_SIZE;
}

/**
 * \brief Convert packets from Netflow v5/v9/sFlow to IPFIX
 *
 * Netflow v9 has almost the same format as IPFIX but it has different Flowset IDs
 * and more information in packet header.
 * Netflow v5 doesn't have (Option) Template Sets so they must be inserted into packet
 * with some other data that are missing (data set header etc.). Template is periodicaly
 * refreshed according to input_info.
 * sFlow format is very complicated - InMon Corp. source code is used in modified form,
 * which converts it into Netflow v5 packet. Support for sFlow is however disabled by default.
 *
 * \param[out] packet Flow information data in the form of IPFIX packet.
 * \param[in] len Length of packet
 * \param[in] max_len Amount of memory allocated for packet
 * \param[in] input_info Information structure storing data needed for refreshing templates
 * \return 0 on success
 */
int convert_packet(char **packet, ssize_t *len, uint16_t max_len, char *input_info)
{
	struct ipfix_header *header = (struct ipfix_header *) *packet;
	uint16_t flow_sample_count = 0;
	uint16_t offset = 0;
	info_list = (struct input_info_list *) input_info;

	switch (htons(header->version)) {
		/* Netflow v9 packet */
		case NETFLOW_V9_VERSION: {
			uint64_t sys_uptime = ntohl(*((uint32_t *) (((uint8_t *) header) + BYTES_4)));
			uint64_t unix_secs = ntohl(*((uint32_t *) (((uint8_t *) header) + BYTES_8)));
			uint64_t time_header = (unix_secs * 1000) - sys_uptime;

			/* Remove sysUpTime field */
			memmove(*packet + BYTES_4, *packet + BYTES_8, buff_len - BYTES_8);
			memset(*packet + buff_len - BYTES_8, 0, BYTES_4);

			*len -= BYTES_4;

			header->length = htons(IPFIX_HEADER_LENGTH);
			header->sequence_number = htonl(ipfix_seq_no[NF9_SEQ_NO]);

			struct ipfix_set_header *set_header;
			uint8_t *p = (uint8_t *) (*packet + IPFIX_HEADER_LENGTH);
			offset = IPFIX_HEADER_LENGTH;

			while (p < (uint8_t *) *packet + *len) {
				set_header = (struct ipfix_set_header *) p;

				switch (ntohs(set_header->flowset_id)) {
					case NETFLOW_V9_TEMPLATE_SET_ID:
						set_header->flowset_id = htons(IPFIX_TEMPLATE_FLOWSET_ID);
						if (ntohs(set_header->length) > 0) {
							if (insert_timestamp_template(set_header) != 0) {
								MSG_DEBUG(msg_module,"Error at V9_TEMPLATE_SET_ID");
								return CONVERSION_ERROR;
							}

							/* Check for enterprise elements */
							*len += unpack_enterprise_elements(set_header, *len - ntohs(header->length));
						}

						break;

					case NETFLOW_V9_OPT_TEMPLATE_SET_ID:
						set_header->flowset_id = htons(IPFIX_OPTION_FLOWSET_ID);
						uint16_t set_len = ntohs(set_header->length);
						if (set_len > 0) {
							if (insert_timestamp_otemplate(set_header) != 0) {
								MSG_DEBUG(msg_module,"Error at V9_OPT_TEMPLATE_SET_ID");
								return CONVERSION_ERROR;
							}

							/* Check for enterprise elements */
							*len += unpack_ot_enterprise_elements(set_header, *len - ntohs(header->length));
						}

						/* Convert 'Option Scope Length' to 'Scope Field Count'
						 * and 'Option Length' to 'Field Count'.
						 */
						struct ipfix_options_template_record *rec;
						uint16_t rec_len;
						uint16_t option_scope_len = 0, option_len = 0;
						uint8_t *otempl_p = p + sizeof(struct ipfix_set_header);

						/* Loop over all option template records; make sure that
						 * padding is ignored by checking whether record header fits
						 */
						while (otempl_p - p < set_len - (uint8_t) (sizeof(struct ipfix_options_template_record) - sizeof(template_ie))) {
							rec = (struct ipfix_options_template_record *) otempl_p;
							option_scope_len = ntohs(rec->count);
							option_len = ntohs(rec->scope_field_count);
							rec_len = option_scope_len + option_len + sizeof(struct ipfix_options_template_record) - sizeof(template_ie);

							uint8_t field_offset = 0, field_index = 0, scope_field_count = 0;
							while (field_offset < option_scope_len + option_len) {
								/* Option scope fields always come before regular fields */
								if (field_offset < option_scope_len) {
									++scope_field_count;
								}

								/* Enterprise number comes just after the IE, before the next IE */
								if (ntohs(rec->fields[field_index].ie.id) & 0x8000) {
									field_offset += sizeof(template_ie);
									++field_index;
								}

								/* Set offset to next field */
								field_offset += sizeof(template_ie);
								++field_index;
							}

							/* Perform conversion from NetFlow v9 to IPFIX */
							rec->count = htons(field_index);
							rec->scope_field_count = htons(scope_field_count);

							/* Set offset to next record */
							otempl_p += rec_len;
						}

						break;

					default: { /* Data set */
						/* Note: be careful when storing the set length in a variable and using
						 * it for determining whether padding are needed (below), as the set length
						 * is likely to be changed by 'insert_timestamp_data'.
						 */
						if (ntohs(set_header->length) > 0) {
							uint16_t shifted = insert_timestamp_data(set_header, time_header, *len - ntohs(header->length));
							*len += (shifted * 8);

							/* Add padding bytes, if necessary. Note that this is not required,
							 * but recommended.
							 */
							uint16_t set_len = ntohs(set_header->length);

							/* Sanity check: does set header length have a realistic value?
							 * We have seen cases where this code was triggered for non-NFv9
							 * PDUs, resulting in invalid operations.
							 */
							if (set_len > *len) {
								MSG_DEBUG(msg_module,"ERROR at Sanity Check, set_len: %u > len: %u", set_len, *len);
								return CONVERSION_ERROR;
							}

							if (set_len % 4 != 0) {
								uint8_t padding_len = 4 - (set_len % 4);

								/* Check whether new packet will be larger than max_len. If so,
								 * we just fail the conversion to IPFIX to avoid having to perform
								 * lots of reallocs (expensive).
								 */
								if (*len + padding_len > max_len) {
									return CONVERSION_ERROR;
								}

								memmove(p + set_len + padding_len, p + set_len, *len - offset + set_len);
								memset(p + set_len, 0, padding_len);

								*len += padding_len;
								set_header->length = htons(set_len + padding_len);
							}
						}

						break; }
				}

				header->length = htons(ntohs(header->length) + ntohs(set_header->length));

				if (ntohs(header->length) > *len) {
					/* Real length of packet is smaller than it should be */
					return CONVERSION_ERROR;
				}

				uint16_t set_len = ntohs(set_header->length);
				if (set_len == 0) {
					break;
				}

				p += set_len;
				offset += set_len;
			}

			break; }

		/* Netflow v5 packet */
		case NETFLOW_V5_VERSION: {
			uint64_t sys_uptime = ntohl(*((uint32_t *) (((uint8_t *) header) + BYTES_4)));
			uint64_t unix_secs = ntohl(*((uint32_t *) (((uint8_t *) header) + BYTES_8)));
			uint64_t unix_nsecs = ntohl(*((uint32_t *) (((uint8_t *) header) + BYTES_12)));
			uint64_t time_header = (unix_secs * 1000) + (unix_nsecs / 1000000);

			flow_sample_count = MIN(ntohs(header->length), NETFLOW_V5_MAX_RECORD_COUNT);

			/* Header modification */
			header->export_time = header->sequence_number;
			memmove(*packet + BYTES_8, *packet + IPFIX_HEADER_LENGTH, buff_len - IPFIX_HEADER_LENGTH);
			memmove(*packet + BYTES_12, *packet + BYTES_12 + BYTES_1, BYTES_1);
			header->observation_domain_id = header->observation_domain_id&(0xF000);

			/* Update real packet length because of memmove() */
			*len = *len - BYTES_8;

			/* We need to resize time element (first and last seen) from 32 bits to 64 bits */
			int i;
			uint8_t *pkt;
			uint16_t shifted = 0;
			for (i = flow_sample_count - 1; i >= 0; i--) {
				/* Resize each timestamp in each data record to 64 bits */
				pkt = (uint8_t *) (*packet + IPFIX_HEADER_LENGTH + (i * (NETFLOW_V5_DATA_SET_LEN - BYTES_4)));
				uint64_t first = ntohl(*((uint32_t *) (pkt + FIRST_OFFSET)));
				uint64_t last  = ntohl(*((uint32_t *) (pkt + LAST_OFFSET)));

				memmove(pkt + LAST_OFFSET + BYTES_8, pkt + LAST_OFFSET,
						(shifted * (NETFLOW_V5_DATA_SET_LEN + BYTES_4)) + (NETFLOW_V5_DATA_SET_LEN - LAST_OFFSET));

				/* Set time values */
				*((uint64_t *) (pkt + FIRST_OFFSET)) = htobe64(time_header - (sys_uptime - first));
				*((uint64_t *) (pkt + LAST_OFFSET + BYTES_4)) = htobe64(time_header - (sys_uptime - last));
				shifted++;
			}

			/* Set right packet length according to memmoves */
			*len += shifted * BYTES_8;

			/* Template Set insertion (if needed) and setting packet length */
			header->length = insert_template_set(packet, flow_sample_count, len);

			header->sequence_number = htonl(ipfix_seq_no[NF5_SEQ_NO]);
			if (*len >= htons(header->length)) {
				ipfix_seq_no[NF5_SEQ_NO] += flow_sample_count;
			}

			break; }

		/* sFlow packet */
		default:
#ifdef ENABLE_SFLOW
			/* Conversion from sflow to Netflow v5-like IPFIX packet */
			flow_sample_count = Process_sflow(*packet, *len);

			/* Observation domain ID is unknown */
			header->observation_domain_id = 0;

			header->export_time = htonl((uint32_t) time(NULL));

			/* Template Set insertion (if needed) and setting total packet length */
			header->length = insert_template_set(packet, flow_sample_count, len);

			header->sequence_number = htonl(ipfix_seq_no[SF_SEQ_NO]);
			if (*len >= htons(header->length)) {
				ipfix_seq_no[SF_SEQ_NO] += flow_sample_count;
			}
#else
			/* Conversion error */
			return CONVERSION_ERROR;
#endif
			break;
	}

	header->version = htons(IPFIX_VERSION);

	return 0;
}
