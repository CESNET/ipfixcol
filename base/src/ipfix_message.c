/**
 * \file ipfix_message.c
 * \author Michal Srb <michal.srb@cesnet.cz>
 * \brief Auxiliary functions for working with IPFIX messages
 *
 * Copyright (C) 2012 CESNET, z.s.p.o.
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ipfixcol/ipfix_message.h>
#include <ipfixcol/verbose.h>

/** Identifier to MSG_* macros */
static char *msg_module = "ipfix_message";

/* Field offsets */
struct offset_field {
	uint16_t offset_index;
	uint16_t id;
	uint16_t length;
};

/* IPFIX ID <-> OFFSET ID mapping */
static struct offset_field offsets[] = {
	/* OFFSET ID, IPFIX_ID, LENGTH */
	{ OF_OCTETS,	1,	8  },
	{ OF_PACKETS,	2,	8  },
	{ OF_PROTOCOL,	4,	1  },
	{ OF_SRCPORT,	7,	2  },
	{ OF_DSTPORT,	11,	2  },
	{ OF_SRCIPV4,	8,	4  },
	{ OF_DSTIPV4,	12,	4  },
	{ OF_SRCIPV6,	27,	16 },
	{ OF_DSTIPV6,	28,	16 }
};

/* some auxiliary functions for extracting data of exact length */
#define read8(ptr) (*((uint8_t *) (ptr)))
#define read16(ptr) (*((uint16_t *) (ptr)))
#define read32(ptr) (*((uint32_t *) (ptr)))
#define read64(ptr) (*((uint64_t *) (ptr)))


/**
 * \brief Create ipfix_message structure from data in memory
 *
 * \param[in] msg memory containing IPFIX message
 * \param[in] len length of the IPFIX message
 * \param[in] input_info information structure about input
 * \param[in] source_status Status of source (new, opened, closed)
 * \return ipfix_message structure on success, NULL otherwise
 */
struct ipfix_message *message_create_from_mem(void *msg, int len, struct input_info* input_info, int source_status)
{
	struct ipfix_message *message;
	uint32_t odid;
	uint16_t pktlen;

	message = (struct ipfix_message*) calloc (1, sizeof(struct ipfix_message));
	if (!message) {
		MSG_ERROR(msg_module, "Unable to allocate memory (%s:%d)", __FILE__, __LINE__);
		return NULL;
	}

	message->pkt_header = (struct ipfix_header*) msg;
	message->input_info = input_info;
	message->source_status = source_status;

	odid = ntohl(message->pkt_header->observation_domain_id);
	MSG_DEBUG(msg_module, "[%u] Processing data", odid);

	/* check IPFIX version */
	if (message->pkt_header->version != htons(IPFIX_VERSION)) {
		MSG_WARNING(msg_module, "[%u] Unexpected IPFIX version detected (%X); skipping message...", odid,
				message->pkt_header->version);
		free(message);
		return NULL;
	}

	pktlen = ntohs(message->pkt_header->length);

	/* check whether message is not shorter than header says */
	if ((uint16_t) len < pktlen) {
		MSG_WARNING(msg_module, "[%u] Malformed IPFIX message detected (bad length); skipping message...", odid);
		free (message);
		return NULL;
	}

	/* process IPFIX msg and fill up the ipfix_message structure */
    uint8_t *p = msg + IPFIX_HEADER_LENGTH;
    int t_set_count = 0;
    int ot_set_count = 0;
    int d_set_count = 0;
    struct ipfix_set_header *set_header;
    while (p < (uint8_t*) msg + pktlen) {
        set_header = (struct ipfix_set_header*) p;
        if ((uint8_t *) p + ntohs(set_header->length) > (uint8_t *) msg + pktlen) {
        	MSG_WARNING(msg_module, "[%u] Malformed IPFIX message detected (bad length); skipping message...", odid);
        	free(message);
        	return NULL;
        }
        switch (ntohs(set_header->flowset_id)) {
            case IPFIX_TEMPLATE_FLOWSET_ID:
                message->templ_set[t_set_count++] = (struct ipfix_template_set *) set_header;
                break;
            case IPFIX_OPTION_FLOWSET_ID:
                 message->opt_templ_set[ot_set_count++] = (struct ipfix_options_template_set *) set_header;
                break;
            default:
                if (ntohs(set_header->flowset_id) < IPFIX_MIN_RECORD_FLOWSET_ID) {
                	MSG_WARNING(msg_module, "[%u] Unknown Set ID %d", odid, ntohs(set_header->flowset_id));
                } else {
                    message->data_couple[d_set_count++].data_set = (struct ipfix_data_set*) set_header;
                }
                break;
        }

        /* if length is wrong and pointer does not move, stop processing the message */
        if (ntohs(set_header->length) == 0) {
        	break;
        }

        p += ntohs(set_header->length);
    }

    return message;
}

/**
 * \brief Copy IPFIX message
 *
 * \param[in] msg original IPFIX message
 * \return copy of original IPFIX message on success, NULL otherwise
 */
struct ipfix_message *message_create_clone(struct ipfix_message *msg)
{
	struct ipfix_message *message;
	uint8_t *packet;

	if (!msg) {
		MSG_DEBUG(msg_module, "Trying to clone IPFIX message, but argument is NULL");
		return NULL;
	}

	packet = (uint8_t *) malloc(ntohs(msg->pkt_header->length));
	if (!packet) {
		MSG_ERROR(msg_module, "Unable to allocate memory (%s:%d)", __FILE__, __LINE__);
		return NULL;
	}
	memset(packet, 0, sizeof(ntohs(msg->pkt_header->length)));

	message = message_create_from_mem(packet, msg->pkt_header->length, msg->input_info, msg->source_status);
	if (!message) {
		MSG_DEBUG(msg_module, "Unable to clone IPFIX message");
		free(packet);
		return NULL;
	}

	return message;
}

/**
 * \brief Get data from IPFIX message.
 *
 * \param[out] dest where to copy data from message
 * \param[in] source from where to copy data from message
 * \param[in] len length of the data
 * \return 0 on success, negative value otherwise
 */
int message_get_data(uint8_t **dest, uint8_t *source, int len)
{

	*dest = (uint8_t *) malloc(sizeof(uint8_t) * len);
	if (!*dest) {
		MSG_ERROR(msg_module, "Unable to allocate memory (%s:%d)", __FILE__, __LINE__);
		return -1;
	}

	memcpy(*dest, source, len);

	return 0;
}

/**
 * \brief Set data to IPFIX message.
 *
 * \param[out] dest where to write data
 * \param[in] source actual data
 * \param[in] len length of the data
 * \return 0 on success, negative value otherwise
 */
int message_set_data(uint8_t *dest, uint8_t *source, int len)
{
	memcpy(dest, source, len);

	return 0;
}

/**
 * \brief Get offset where next data record starts
 *
 * \param[in] data_record data record
 * \param[in] tmplt template for data record
 * \return offset of next data record in data set
 */
uint16_t get_next_data_record_offset(uint8_t *data_record, struct ipfix_template *tmplt)
{
	if (!tmplt) {
		/* we don't have template for this data set */
		return 0;
	}

	uint16_t count = 0;
	uint16_t offset = 0;
	uint16_t index;
	uint16_t length;
	uint16_t id;

	index = count;

	/* go over all fields */
	while (count != tmplt->field_count) {
		id = tmplt->fields[index].ie.id;
		length = tmplt->fields[index].ie.length;

		if (id >> 15) {
			/* Enterprise Number */
			++index;
		}

		if (length != VAR_IE_LENGTH) {
			offset += length;
		} else {
			/* variable length */
			length = read8(data_record+offset);
			offset += 1;

			if (length == 255) {
				length = ntohs(read16(data_record+offset));
				offset += 2;
			}

			offset += length;
		}

		++index;
		++count;
	}

	return offset;
}

/**
 * \brief Get pointers to start of the Data Records in specific Data Set
 *
 * \param[in] data_set data set to work with
 * \param[in] tmplt template for data set
 * \return array of pointers to start of the Data Records in Data Set
 */
uint8_t **get_data_records(struct ipfix_data_set *data_set, struct ipfix_template *tmplt)
{
	uint8_t *data_record;
	uint32_t offset;
	uint16_t min_record_length;
	uint8_t **records;
	uint16_t records_index = 0;

	/* TODO - make it clever */
	records = (uint8_t **) malloc(MSG_MAX_DATA_COUPLES * sizeof(uint8_t *));
	if (records == NULL) {
		MSG_ERROR(msg_module, "Unable to allocate memory (%s:%d)", __FILE__, __LINE__);
		return (uint8_t **) NULL;
	}
	memset(records, 0, MSG_MAX_DATA_COUPLES * sizeof(uint8_t *));

	min_record_length = tmplt->data_length;
	offset = 0;

	offset += 4;  /* size of the header */

	if (min_record_length & 0x8000) {
		/* oops, record contains fields with variable length */
		min_record_length = min_record_length & 0x7fff; /* size of the fields, variable fields excluded  */
	}

	while ((int) ntohs(data_set->header.length) - (int) offset - (int) min_record_length >= 0) {
		data_record = (((uint8_t *) data_set) + offset);
		records[records_index] = data_record;
		offset += get_next_data_record_offset(data_record, tmplt);
	}

	return records;
}


/**
 * \brief Create empty IPFIX message
 *
 * \return new instance of ipfix_message structure.
 */
struct ipfix_message *message_create_empty()
{
	struct ipfix_message *message;
	struct ipfix_header *header;

	message = (struct ipfix_message *) malloc(sizeof(*message));
	if (!message) {
		MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)", __FILE__, __LINE__);
		return NULL;
	}
	memset(message, 0, sizeof(*message));

	header = (struct ipfix_header *) malloc(sizeof(*header));
	if (!header) {
		MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)", __FILE__, __LINE__);
		free(message);
		return NULL;
	}
	memset(header, 0, sizeof(*header));

	message->pkt_header = header;

	return message;
}


/**
 * \brief Dispose IPFIX message
 *
 * \param[in] msg IPFIX message to dispose
 * \return 0 on success, negative value otherwise
 */
int message_free(struct ipfix_message *msg)
{
	if (!msg) {
		MSG_DEBUG(msg_module, "Trying to free NULL pointer");
		return -1;
	}

	free(msg->pkt_header);
	free(msg);

	/* note we do not want to free input_info structure, it is input plugin's job */

	return 0;
}

/*
 * ---------------------------------------------------------------------------
 * ---------------------------------------------------------------------------
 * ---------------------------------------------------------------------------
 */

/**
 * \brief Get field with given id
 * \param[in] fields template (record) fields
 * \param[in] cnt number of fields
 * \param[in] enterprise enterprise field number
 * \param[in] id  field id
 * \param[out] data_offset offset data record specified by this template
 * \param[in] netw Flag indicating network byte order (template record)
 * \return pointer to row in fields
 */
struct ipfix_template_row *fields_get_field(uint8_t *fields, int cnt, uint32_t enterprise, uint16_t id, int *data_offset, int netw)
{
	int i;

	if (data_offset) {
		*data_offset = 0;
	}

	struct ipfix_template_row *row = (struct ipfix_template_row *) fields;

	for (i = 0; i < cnt; ++i, ++row) {
		uint16_t rid = (netw) ? ntohs(row->id) : row->id;
		uint16_t len = (netw) ? ntohs(row->length) : row->length;
		uint32_t ren = 0;
		
		/* Get field ID and enterprise number */
		if (rid >> 15) {
			rid = rid & 0x7FFF;
			++row;
			ren = (netw) ? ntohl(*((uint32_t *) row)) : *((uint32_t *) row);
		}
		
		/* Check informations */
		if (rid == id && ren == enterprise) {
			if (ren != 0) {
				--row;
			}
			return row;
		}

		/* increase data offset */
		if (data_offset) {
			*data_offset += len;
		}
	}

	return NULL;
}

/**
 * \brief Get template record field
 */
struct ipfix_template_row *template_record_get_field(struct ipfix_template_record *rec, uint32_t enterprise, uint16_t id, int *data_offset)
{
	return fields_get_field((uint8_t *) rec->fields, ntohs(rec->count), enterprise, id, data_offset, 1);
}

/**
 * \brief Get template record field
 */
struct ipfix_template_row *template_get_field(struct ipfix_template *templ, uint32_t enterprise, uint16_t id, int *data_offset)
{
	return fields_get_field((uint8_t *) templ->fields, templ->field_count, enterprise, id, data_offset, 0);
}

/**
 * \brief Get offset of data record's field
 *
 * \param[in] data_record Data record
 * \param[in] template Data record's template
 * \param[in] enterprise Enterprise number
 * \param[in] id Field ID
 * \param[out] data_length Field length
 * \return Field offset
 */
int data_record_field_offset(uint8_t *data_record, struct ipfix_template *template, uint32_t enterprise, uint16_t id, int *data_length)
{
	int ieid;
	int count, offset = 0, index, length, prevoffset;
	struct ipfix_template_row *row = NULL;

	if (!(template->data_length & 0x80000000)) {
		/* Data record with no variable length field */
		row = template_get_field(template, enterprise, id, &offset);
		if (!row) {
			return -1;
		}

		if (data_length) {
			*data_length = row->length;
		}

		return offset;
	}

	/* Data record with variable length field(s) */
	for (count = index = 0; count < template->field_count; count++, index++) {
		length = template->fields[index].ie.length;
		ieid = template->fields[index].ie.id;

		if (ieid >> 15) {
			/* Enterprise Number */
			ieid &= 0x7FFF;
			++index;
		}
		prevoffset = offset;

		switch (length) {
		case (1):
		case (2):
		case (4):
		case (8):
			offset += length;
			break;
		default:
			if (length != VAR_IE_LENGTH) {
				offset += length;
			} else {
				/* variable length */
				length = *((uint8_t *) (data_record+offset));
				offset += 1;

				if (length == 255) {
					length = ntohs(*((uint16_t *) (data_record+offset)));
					offset += 2;
				}

				prevoffset = offset;
				offset += length;
			}
			break;
		}

		/* Field found */
		if (id == ieid) {
			if (data_length) {
				*data_length = length;
			}
			return prevoffset;
		}

	}

	/* Field not found */
	return -1;
}

/**
 * \brief Get data from record
 */
uint8_t *data_record_get_field(uint8_t *record, struct ipfix_template *templ, uint32_t enterprise, uint16_t id, int *data_length)
{
	int offset_id = OF_COUNT;

	if (enterprise == 0) {
		/* Check whether we have offset field for this ID */
		for (offset_id = 0; offset_id < OF_COUNT; ++offset_id) {
			if (id == offsets[offset_id].id) {
				break;
			}
		}

		/* If offset is already known, use it */
		if (offset_id != OF_COUNT && templ->offsets[offset_id] > 0) {
			if (data_length) {
				*data_length = offsets[offset_id].length;
			}

			return (uint8_t *) record + templ->offsets[offset_id];
		}
	}

	int offset = data_record_field_offset(record, templ, enterprise, id, data_length);

	if (offset < 0) {
		return NULL;
	}

	/* Store offset for future lookup */
	if (offset_id != OF_COUNT) {
		templ->offsets[offset_id] = offset;
	}

	return (uint8_t *) record + offset;
}

/**
 * \brief Set field value
 */
void data_record_set_field(uint8_t *record, struct ipfix_template *templ, uint32_t enterprise, uint16_t id, uint8_t *value)
{
	int data_length;
	int offset = data_record_field_offset(record, templ, enterprise, id, &data_length);

	if (offset >= 0) {
		memcpy(record + offset, value, data_length);
	}
}

/**
 * \brief Count data record length
 */
uint16_t data_record_length(uint8_t *data_record, struct ipfix_template *template)
{
	uint16_t count, offset = 0, index, length;

	if (!(template->data_length & 0x80000000)) {
		return template->data_length;
	}

	for (count = index = 0; count < template->field_count; count++, index++) {
		length = template->fields[index].ie.length;

		if (template->fields[index].ie.id >> 15) {
			/* Enterprise Number */
			++index;
		}

		switch (length) {
		case (1):
		case (2):
		case (4):
		case (8):
			offset += length;
			break;
		default:
			if (length != VAR_IE_LENGTH) {
				offset += length;
			} else {
				/* variable length */
				length = *((uint8_t *) (data_record+offset));
				offset += 1;

				if (length == 255) {
					length = ntohs(*((uint16_t *) (data_record+offset)));
					offset += 2;
				}

				offset += length;
			}
			break;
		}
	}

	return offset;
}

/**
 * \brief Go through all data records and call processing function for each
 */
int data_set_process_records(struct ipfix_data_set *data_set, struct ipfix_template *templ, dset_callback_f processor, void *proc_data)
{
	uint16_t setlen = ntohs(data_set->header.length), rec_len, count = 0;
	uint8_t *ptr = data_set->records;

	uint16_t min_record_length = templ->data_length;
	uint32_t offset = 4;

	if (min_record_length & 0x80000000) {
		/* record contains fields with variable length */
		min_record_length = min_record_length & 0x7fff; /* size of the fields, variable fields excluded  */
	}

	while ((int) setlen - (int) offset - (int) min_record_length >= 0) {
		ptr = (((uint8_t *) data_set) + offset);
		rec_len = data_record_length(ptr, templ);
		if (processor) {
			processor(ptr, rec_len, templ, proc_data);
		}
		count++;
		offset += rec_len;
	}

	return count;
}

/**
 * \brief Go through all (options) template records and call processing function for each
 */
int template_set_process_records(struct ipfix_template_set *tset, int type, tset_callback_f processor, void *proc_data)
{
	int max_len, rec_len, count = 0;
	uint8_t *ptr = (uint8_t*) &tset->first_record;

	while (ptr < (uint8_t*) tset + ntohs(tset->header.length)) {
		max_len = ((uint8_t *) tset + ntohs(tset->header.length)) - ptr;
		rec_len = tm_template_record_length((struct ipfix_template_record *) ptr, max_len, type, NULL);
		if (rec_len == 0) {
			break;
		}
		if (processor) {
			processor(ptr, rec_len, proc_data);
		}
		count++;
		ptr += rec_len;
	}

	return count;
}

/**
 * \brief Set field value for each data record in set
 */
void data_set_set_field(struct ipfix_data_set *data_set, struct ipfix_template *templ, uint32_t enterprise, uint16_t id, uint8_t *value)
{
	int field_offset;
	uint16_t setlen = ntohs(data_set->header.length);
	uint8_t *ptr = data_set->records;
	struct ipfix_template_row *row = template_get_field(templ, enterprise, id, &field_offset);

	if (!row) {
		return;
	}

	uint16_t min_record_length = templ->data_length;
	uint32_t offset = 4;

	if (min_record_length & 0x8000) {
		/* record contains fields with variable length */
		min_record_length = min_record_length & 0x7fff; /* size of the fields, variable fields excluded  */
	}

	while ((int) setlen - (int) offset - (int) min_record_length >= 0) {
		ptr = (((uint8_t *) data_set) + offset);
		memcpy(ptr + field_offset, value, row->length);
		offset += data_record_length(ptr, templ);
	}

}

int data_set_records_count(struct ipfix_data_set *data_set, struct ipfix_template *template)
{
	if (template->data_length & 0x80000000) {
		/* damn... there is a Information Element with variable length. we have to
		 * compute number of the Data Records in current Set by hand */
		/* Get number of records */
		return data_set_process_records(data_set, template, NULL, NULL);
	} else {
		/* no elements with variable length */
		return ntohs(data_set->header.length) / template->data_length;
	}
}

void message_free_metadata(struct ipfix_message *msg)
{
	for (uint16_t i = 0; i < msg->data_records_count; ++i) {
		/* Free profiles */
		if (msg->metadata[i].channels) {
			free(msg->metadata[i].channels);
		}
	}
	
	/* Free metadata structure */
	free(msg->metadata);
}

struct metadata *message_copy_metadata(struct ipfix_message *src)
{
	struct metadata *metadata = calloc(src->data_records_count, sizeof(struct metadata));
	if (!metadata) {
		MSG_ERROR(msg_module, "Not enough memory (%s:%d)", __FILE__, __LINE__);
		return NULL;
	}

	for (uint16_t i = 0; i < src->data_records_count; ++i) {
		metadata[i].srcAS = src->metadata[i].srcAS;
		metadata[i].dstAS = src->metadata[i].dstAS;
		memcpy(metadata[i].srcName, src->metadata[i].srcName, 32);
		memcpy(metadata[i].dstName, src->metadata[i].dstName, 32);
		metadata[i].srcCountry = src->metadata[i].srcCountry;
		metadata[i].dstCountry = src->metadata[i].dstCountry;
		metadata[i].record = src->metadata[i].record;

		if (src->metadata[i].channels == NULL) {
			continue;
		}

		uint16_t channels = 0;
		while (src->metadata[i].channels[channels]) {
			channels++;
		}

		metadata[i].channels = calloc(channels + 1, sizeof(void*));
		if (!metadata) {
			MSG_ERROR(msg_module, "Not enough memory (%s:%d)", __FILE__, __LINE__);
			free(metadata);
			return NULL;
		}

		for (uint16_t index = 0; index < channels; ++index) {
			metadata[i].channels[index] = src->metadata[i].channels[index];
		}
	}

	return metadata;
}


























