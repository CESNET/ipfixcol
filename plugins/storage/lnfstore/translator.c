/**
 * \file translator.c
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \author Imrich Stoffa <xstoff02@stud.fit.vutbr.cz>
 * \brief Conversion of IPFIX to LNF format (source file)
 */

/* Copyright (C) 2015 - 2017 CESNET, z.s.p.o.
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

#include <string.h>
#include <inttypes.h>
#include <ipfixcol.h>
#include "lnfstore.h"
#include "translator.h"
#include "converters.h"

/**
 * \brief Record iterator structure
 */
struct rec_iter {
	/** Pointer to the current field */
	const uint8_t *field_ptr;
	/** Size of the current field    */
	uint16_t field_size;

	/** Private Enterprise Number of the current Information Element */
	uint32_t pen;
	/** ID of the current Information Element within the PEN         */
	uint16_t ie_id;

	/** Private values of the iterator (DO NOT USE DIRECTLY!)        */
	struct private_s {
		/** Record's description (base pointer, template, etc.)      */
		const struct ipfix_record *rec_mdata;
		/** Index of the next field in the template       */
		int idx;
		/** Offset from the start of the next record      */
		uint32_t offset;
	} private_;
};

/**
 * \brief Initialize a record iterator
 *
 * \note After initialization the iterator doesn't point to any field.
 *   Therefore, to prepare the first and next following fields call
 *   rec_iter_next() function.
 * \param[out] it    Instance of the iterator
 * \param[in]  mdata Pointer to a record
 */
static void
rec_iter_init(struct rec_iter *it, const struct metadata *mdata)
{
	it->private_.offset = 0;
	it->private_.idx = 0;
	it->private_.rec_mdata = &mdata->record;
}

/**
 * \brief Destroy a record iterator
 * \param[in] it Instance of the iterator
 */
static void
rec_iter_destroy(struct rec_iter *it)
{
	// Nothing... just a placeholder for possible future changes
	(void) it;
}

/**
 * \brief Prepare the next field
 * \param[in,out] it Instance of the iterator
 * \return If the field is ready, returns 0. Otherwise (i.e. no more fields)
 *   returns a non-zero value.
 */
static int
rec_iter_next(struct rec_iter *it)
{
	const struct ipfix_template *rec_tmplt = it->private_.rec_mdata->templ;
	int idx = it->private_.idx;
	if (idx >= rec_tmplt->field_count) {
		// No more fields
		return 1;
	}

	const uint8_t *rec_start = it->private_.rec_mdata->record;
	uint16_t field_size = rec_tmplt->fields[idx].ie.length;
	uint32_t offset = it->private_.offset;

	// Update info about the next field
	it->pen = 0;
	it->ie_id = rec_tmplt->fields[idx].ie.id;
	if (it->ie_id & 0x8000) {
		// Not IANA field
		it->ie_id &= 0x7FFF;
		it->pen = rec_tmplt->fields[++idx].enterprise_number;
	}

	// Get real size of the record
	if (field_size == VAR_IE_LENGTH) {
		field_size = rec_start[offset];
		offset += 1;

		if (field_size == 255) {
			field_size = ntohs(*(uint16_t *) &rec_start[offset]);
			offset += 2;
		}
	}

	// Update user parameters
	it->field_ptr = rec_start + offset;
	it->field_size = field_size;

	// Update internals
	it->private_.idx = idx + 1;
	it->private_.offset = offset + field_size;
	return 0;
}

// Prototypes
struct translator_table_rec;
static int
translate_uint(const struct rec_iter *iter,
		const struct translator_table_rec *def, uint8_t *buffer_ptr);
static int
translate_ip(const struct rec_iter *iter,
		const struct translator_table_rec *def, uint8_t *buffer_ptr);
static int
translate_mac(const struct rec_iter *iter,
		const struct translator_table_rec *def, uint8_t *buffer_ptr);
static int
translate_tcpflags(const struct rec_iter *iter,
		const struct translator_table_rec *def, uint8_t *buffer_ptr);
static int
translate_time(const struct rec_iter *iter,
		const struct translator_table_rec *def, uint8_t *buffer_ptr);

/**
 * \typedef translator_func
 * \brief Definition of conversion function
 * \param[in]  iter       Pointer to an IPFIX field to convert
 * \param[in]  def        Definition of data conversion
 * \param[out] buffer_ptr Pointer to the conversion buffer where LNF value
 *   will be stored
 * \return On success return 0. Otherwise returns a non-zero value.
 */
typedef int (*translator_func)(const struct rec_iter *iter,
	const struct translator_table_rec *def, uint8_t *buffer_ptr);

/**
 * \brief Conversion record
 */
struct translator_table_rec
{
	/** Identification of the IPFIX Information Element              */
	struct ipfix_s {
		/** Private Enterprise Number of the Information Element     */
		uint32_t pen;
		/** ID of the Information Element within the PEN             */
		uint16_t ie;
	} ipfix;

	/** Identification of the corresponding LNF Field */
	struct lnf_s {
		/** Field identification                      */
		int id;
		/** Internal size of the Field                */
		int size;
		/** Internal type of the Field                */
		int type;
	} lnf;

	/** Conversion function */
	translator_func func;
};

/**
 * \brief Global translator table
 * \warning Size of each LNF field is always 0 because a user must create its
 *   own instance of the table and fill the correct size from LNF using
 *   lnf_fld_info API function.
 */
static const struct translator_table_rec translator_table_global[] = {
	{{0,   1}, {LNF_FLD_DOCTETS,     0, 0}, translate_uint},
	{{0,   2}, {LNF_FLD_DPKTS,       0, 0}, translate_uint},
	{{0,   3}, {LNF_FLD_AGGR_FLOWS,  0, 0}, translate_uint},
	{{0,   4}, {LNF_FLD_PROT,        0, 0}, translate_uint},
	{{0,   5}, {LNF_FLD_TOS,         0, 0}, translate_uint},
	{{0,   6}, {LNF_FLD_TCP_FLAGS,   0, 0}, translate_tcpflags},
	{{0,   7}, {LNF_FLD_SRCPORT,     0, 0}, translate_uint},
	{{0,   8}, {LNF_FLD_SRCADDR,     0, 0}, translate_ip},
	{{0,   9}, {LNF_FLD_SRC_MASK,    0, 0}, translate_uint},
	{{0,  10}, {LNF_FLD_INPUT,       0, 0}, translate_uint},
	{{0,  11}, {LNF_FLD_DSTPORT,     0, 0}, translate_uint},
	{{0,  12}, {LNF_FLD_DSTADDR,     0, 0}, translate_ip},
	{{0,  13}, {LNF_FLD_DST_MASK,    0, 0}, translate_uint},
	{{0,  14}, {LNF_FLD_OUTPUT,      0, 0}, translate_uint},
	{{0,  15}, {LNF_FLD_IP_NEXTHOP,  0, 0}, translate_ip},
	{{0,  16}, {LNF_FLD_SRCAS,       0, 0}, translate_uint},
	{{0,  17}, {LNF_FLD_DSTAS,       0, 0}, translate_uint},
	{{0,  18}, {LNF_FLD_BGP_NEXTHOP, 0, 0}, translate_ip},
	/* These elements are not supported, because of implementation complexity.
	 * However, these elements are not very common in IPFIX flows and in case
	 * of Netflow flows, these elements are converted in the preprocessor to
	 * timestamp in flowStartMilliseconds/flowEndMilliseconds.
	{{0,  21}, {LNF_FLD_LAST,        0, 0}, translate_time},
	{{0,  22}, {LNF_FLD_FIRST,       0, 0}, translate_time},
	*/
	{{0,  23}, {LNF_FLD_OUT_BYTES,   0, 0}, translate_uint},
	{{0,  24}, {LNF_FLD_OUT_PKTS,    0, 0}, translate_uint},
	{{0,  27}, {LNF_FLD_SRCADDR,     0, 0}, translate_ip},
	{{0,  28}, {LNF_FLD_DSTADDR,     0, 0}, translate_ip},
	{{0,  29}, {LNF_FLD_SRC_MASK,    0, 0}, translate_uint},
	{{0,  30}, {LNF_FLD_DST_MASK,    0, 0}, translate_uint},
	/* LNF_FLD_ specific id missing, DSTPORT overlaps
	{{0,  32}, {LNF_FLD_DSTPORT,     0, 0}, translate_uint },
	*/
	{{0,  38}, {LNF_FLD_ENGINE_TYPE, 0, 0}, translate_uint},
	{{0,  39}, {LNF_FLD_ENGINE_ID,   0, 0}, translate_uint},
	{{0,  55}, {LNF_FLD_DST_TOS,     0, 0}, translate_uint},
	{{0,  56}, {LNF_FLD_IN_SRC_MAC,  0, 0}, translate_mac},
	{{0,  57}, {LNF_FLD_OUT_DST_MAC, 0, 0}, translate_mac},
	{{0,  58}, {LNF_FLD_SRC_VLAN,    0, 0}, translate_uint},
	{{0,  59}, {LNF_FLD_DST_VLAN,    0, 0}, translate_uint},
	{{0,  61}, {LNF_FLD_DIR,         0, 0}, translate_uint},
	{{0,  62}, {LNF_FLD_IP_NEXTHOP,  0, 0}, translate_ip},
	{{0,  63}, {LNF_FLD_BGP_NEXTHOP, 0, 0}, translate_ip},
	/* Not implemented
	{{0,  70}, { LNF_FLD_MPLS_LABEL, 0, 0}, translate_mpls}, //this refers to base of stack
	{{0,  71}, { LNF_FLD_MPLS_LABEL, 0, 0}, translate_mpls},
	{{0,  72}, { LNF_FLD_MPLS_LABEL, 0, 0}, translate_mpls},
	{{0,  73}, { LNF_FLD_MPLS_LABEL, 0, 0}, translate_mpls},
	{{0,  74}, { LNF_FLD_MPLS_LABEL, 0, 0}, translate_mpls},
	{{0,  75}, { LNF_FLD_MPLS_LABEL, 0, 0}, translate_mpls},
	{{0,  76}, { LNF_FLD_MPLS_LABEL, 0, 0}, translate_mpls},
	{{0,  77}, { LNF_FLD_MPLS_LABEL, 0, 0}, translate_mpls},
	{{0,  78}, { LNF_FLD_MPLS_LABEL, 0, 0}, translate_mpls},
	{{0,  79}, { LNF_FLD_MPLS_LABEL, 0, 0}, translate_mpls},
	*/
	{{0,  80}, {LNF_FLD_OUT_SRC_MAC, 0, 0}, translate_mac},
	{{0,  81}, {LNF_FLD_IN_DST_MAC,  0, 0}, translate_mac},
	{{0,  89}, {LNF_FLD_FWD_STATUS,  0, 0}, translate_uint},
	{{0, 128}, {LNF_FLD_BGPNEXTADJACENTAS, 0, 0}, translate_uint},
	{{0, 129}, {LNF_FLD_BGPPREVADJACENTAS, 0, 0}, translate_uint},
	{{0, 130}, {LNF_FLD_IP_ROUTER,   0, 0}, translate_ip},
	{{0, 131}, {LNF_FLD_IP_ROUTER,   0, 0}, translate_ip},
	{{0, 148}, {LNF_FLD_CONN_ID,     0, 0}, translate_uint},
	{{0, 150}, {LNF_FLD_FIRST,       0, 0}, translate_time},
	{{0, 151}, {LNF_FLD_LAST,        0, 0}, translate_time},
	{{0, 152}, {LNF_FLD_FIRST,       0, 0}, translate_time},
	{{0, 153}, {LNF_FLD_LAST,        0, 0}, translate_time},
	{{0, 154}, {LNF_FLD_FIRST,       0, 0}, translate_time},
	{{0, 155}, {LNF_FLD_LAST,        0, 0}, translate_time},
	{{0, 156}, {LNF_FLD_FIRST,       0, 0}, translate_time},
	{{0, 157}, {LNF_FLD_LAST,        0, 0}, translate_time},
	{{0, 176}, {LNF_FLD_ICMP_TYPE,   0, 0}, translate_uint},
	{{0, 177}, {LNF_FLD_ICMP_CODE,   0, 0}, translate_uint},
	{{0, 178}, {LNF_FLD_ICMP_TYPE,   0, 0}, translate_uint},
	{{0, 179}, {LNF_FLD_ICMP_CODE,   0, 0}, translate_uint},
	{{0, 225}, {LNF_FLD_XLATE_SRC_IP,   0, 0}, translate_ip},
	{{0, 226}, {LNF_FLD_XLATE_DST_IP,   0, 0}, translate_ip},
	{{0, 227}, {LNF_FLD_XLATE_SRC_PORT, 0, 0}, translate_uint},
	{{0, 228}, {LNF_FLD_XLATE_DST_PORT, 0, 0}, translate_uint},
	{{0, 230}, {LNF_FLD_EVENT_FLAG,     0, 0}, translate_uint}, //not sure
	{{0, 233}, {LNF_FLD_FW_XEVENT,      0, 0}, translate_uint},
	{{0, 234}, {LNF_FLD_INGRESS_VRFID,  0, 0}, translate_uint},
	{{0, 235}, {LNF_FLD_EGRESS_VRFID,   0, 0}, translate_uint},
	{{0, 258}, {LNF_FLD_RECEIVED,       0, 0}, translate_time},
	{{0, 281}, {LNF_FLD_XLATE_SRC_IP,   0, 0}, translate_ip},
	{{0, 282}, {LNF_FLD_XLATE_DST_IP,   0, 0}, translate_ip}
};

/**
 * \brief Convert an unsigned integer
 * \details \copydetails ::translator_func
 */
static int
translate_uint(const struct rec_iter *iter,
	const struct translator_table_rec *def, uint8_t *buffer_ptr)
{
	// Get a value of IPFIX field
	uint64_t value;
	if (ipx_get_uint_be(iter->field_ptr, iter->field_size, &value)
		!= IPX_CONVERT_OK) {
		// Failed
		return 1;
	}

	// Store the value to the buffer
	if (ipx_set_uint_lnf(buffer_ptr, def->lnf.type, value)
		== IPX_CONVERT_ERR_ARG) {
		// Failed
		return 1;
	}

	return 0;
}

/**
 * \brief Convert an IP address
 * \details \copydetails ::translator_func
 */
static int
translate_ip(const struct rec_iter *iter,
	const struct translator_table_rec *def, uint8_t *buffer_ptr)
{
	(void) def;

	switch (iter->field_size) {
	case 4: // IPv4
		memset(buffer_ptr, 0x0, sizeof(lnf_ip_t));
		((lnf_ip_t *) buffer_ptr)->data[3] = *(uint32_t *) iter->field_ptr;
		break;
	case 16: // IPv6
		memcpy(buffer_ptr, iter->field_ptr, iter->field_size);
		break;
	default:
		// Invalid size of the field
		return 1;
	}

	return 0;
}

/**
 * \brief Convert TCP flags
 * \note TCP flags can be also stored in 16bit in IPFIX, but LNF file supports
 *   only 8 bits flags. Therefore, we need to truncate the field if necessary.
 * \details \copydetails ::translator_func
 */
static int
translate_tcpflags(const struct rec_iter *iter,
	const struct translator_table_rec *def, uint8_t *buffer_ptr)
{
	(void) def;

	switch (iter->field_size) {
	case 1:
		*buffer_ptr = *iter->field_ptr;
		break;
	case 2: {
		uint16_t new_value = ntohs(*(uint16_t *) iter->field_ptr);
		*buffer_ptr = (uint8_t) new_value; // Preserve only bottom 8 bites
		}
		break;
	default:
		// Invalid size of the field
		return 1;
	}

	return 0;
}

/**
 * \brief Convert a MAC address
 * \note We have to keep the address in network byte order. Therefore, we
 *   cannot use a converter for unsigned int
 * \details \copydetails ::translator_func
 */
static int
translate_mac(const struct rec_iter *iter,
	const struct translator_table_rec *def, uint8_t *buffer_ptr)
{
	if (iter->field_size != 6 || def->lnf.size != 6) {
		return 1;
	}

	memcpy(buffer_ptr, iter->field_ptr, 6U);
	return 0;
}

/**
 * \brief Convert a timestamp
 * \details \copydetails ::translator_func
 */
static int
translate_time(const struct rec_iter *iter,
	const struct translator_table_rec *def, uint8_t *buffer_ptr)
{
	if (iter->pen != 0) {
		// Non-standard field are not supported right now
		return 1;
	}

	// Determine data type of a timestamp
	enum ipx_element_type type;
	switch (iter->ie_id) {
	case 150: // flowStartSeconds
	case 151: // flowEndSeconds
		type = IPX_ET_DATE_TIME_SECONDS;
		break;
	case 152: // flowStartMilliseconds
	case 153: // flowEndMilliseconds
		type = IPX_ET_DATE_TIME_MILLISECONDS;
		break;
	case 154: // flowStartMicroseconds
	case 155: // flowEndMicroseconds
		type = IPX_ET_DATE_TIME_MICROSECONDS;
		break;
	case 156: // flowStartNanoseconds
	case 157: // flowEndNanoseconds
		type = IPX_ET_DATE_TIME_NANOSECONDS;
		break;
	case 258: // collectionTimeMilliseconds
		type = IPX_ET_DATE_TIME_MILLISECONDS;
		break;
	default:
		// Other fields are not supported
		return 1;
	}

	// Get the timestamp in milliseconds
	uint64_t value;
	if (ipx_get_datetime_lp_be(iter->field_ptr, iter->field_size, type, &value)
		!= IPX_CONVERT_OK) {
		// Failed
		return 1;
	}

	// Store the value
	if (ipx_set_uint_lnf(buffer_ptr, def->lnf.type, value) != IPX_CONVERT_OK) {
		// Failed (note: truncation is doesn't make sense)
		return 1;
	}

	return 0;
}

// Size of conversion buffer
#define REC_BUFF_SIZE (65535)
// Size of translator table
#define TRANSLATOR_TABLE_SIZE \
	(sizeof(translator_table_global) / sizeof(translator_table_global[0]))

struct translator_s {
	/** Private conversion table */
	struct translator_table_rec table[TRANSLATOR_TABLE_SIZE];
	/** Record conversion buffer */
	uint8_t rec_buffer[REC_BUFF_SIZE];
};

/**
 * \brief Compare conversion definitions
 * \note Only definitions of IPFIX Information Elements are used for comparison
 *   i.e. other values can be undefined.
 * \param[in] p1 Left definition
 * \param[in] p2 Right definition
 * \return The same as memcmp i.e. < 0, == 0, > 0
 */
static int
transtator_cmp(const void *p1, const void *p2)
{
	const struct translator_table_rec *elem1, *elem2;
	elem1 = (const struct translator_table_rec *) p1;
	elem2 = (const struct translator_table_rec *) p2;

	uint64_t elem1_val = ((uint64_t) elem1->ipfix.pen) << 16 | elem1->ipfix.ie;
	uint64_t elem2_val = ((uint64_t) elem2->ipfix.pen) << 16 | elem2->ipfix.ie;

	if (elem1_val == elem2_val) {
		return 0;
	} else {
		return (elem1_val < elem2_val) ? (-1) : 1;
	}
}

translator_t *
translator_init()
{
	translator_t *instance = calloc(1, sizeof(*instance));
	if (!instance) {
		return NULL;
	}

	// Copy conversion table and sort it (just for sure)
	const size_t table_size = sizeof(translator_table_global);
	memcpy(instance->table, translator_table_global, table_size);

	const size_t table_elem_size = sizeof(instance->table[0]);
	qsort(instance->table, TRANSLATOR_TABLE_SIZE, table_elem_size,
		transtator_cmp);

	// Update information about LNF fields
	for (size_t i = 0; i < TRANSLATOR_TABLE_SIZE; ++i) {
		struct translator_table_rec *rec = &instance->table[i];

		int size = 0;
		int type = LNF_NONE;

		// Get the size and type of the LNF field
		int size_ret;
		int type_ret;

		size_ret = lnf_fld_info(rec->lnf.id, LNF_FLD_INFO_SIZE, &size, sizeof(size));
		type_ret = lnf_fld_info(rec->lnf.id, LNF_FLD_INFO_TYPE, &type, sizeof(type));
		if (size_ret == LNF_OK && type_ret == LNF_OK) {
			rec->lnf.size = size;
			rec->lnf.type = type;
			continue;
		}

		// Failed
		MSG_ERROR(msg_module, "lnf_fld_info(): Failed to get a size/type of "
			"a LNF element (id: %d)", rec->lnf.id);
		free(instance);
		return NULL;
	}

	return instance;
}

void
translator_destroy(translator_t *trans)
{
	free(trans);
}

int
translator_translate(translator_t *trans, const struct metadata *mdata,
	lnf_rec_t *rec)
{
	lnf_rec_clear(rec);

	// Initialize a record iterator
	struct rec_iter it;
	rec_iter_init(&it, mdata);

	// Try to convert all IPFIX fields
	struct translator_table_rec key;
	struct translator_table_rec *def;
	int converted_fields = 0;

	const size_t table_rec_cnt = TRANSLATOR_TABLE_SIZE;
	const size_t table_rec_size = sizeof(trans->table[0]);
	uint8_t * const buffer_ptr = trans->rec_buffer;

	while (rec_iter_next(&it) == 0) {
		// Find a conversion function
		key.ipfix.ie = it.ie_id;
		key.ipfix.pen = it.pen;

		def = bsearch(&key, trans->table, table_rec_cnt, table_rec_size,
			transtator_cmp);
		if (!def) {
			// Conversion definition not found
			continue;
		}

		if (def->func(&it, def, buffer_ptr) != 0) {
			// Conversion function failed
			MSG_WARNING(msg_module, "Failed to converter a IPFIX IE field "
				"(ID: %" PRIu16 ", PEN: %" PRIu32 ") to LNF field.",
				it.ie_id, it.pen);
			continue;
		}

		if (lnf_rec_fset(rec, def->lnf.id, buffer_ptr) != LNF_OK) {
			// Setter failed
			MSG_WARNING(msg_module, "Failed to store a IPFIX IE field "
				"(ID: %" PRIu16 ", PEN: %" PRIu32 ") to a LNF record.",
				it.ie_id, it.pen);
			continue;
		}
		converted_fields++;
	}

	// Cleanup
	rec_iter_destroy(&it);
	return converted_fields;
}
