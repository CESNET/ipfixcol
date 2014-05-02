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
#include "ipfix_message.h"

/** Identifier to MSG_* macros */
static char *msg_module = "ipfix_message";


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
 * \return ipfix_message structure on success, NULL otherwise
 */
struct ipfix_message *message_create_from_mem(void *msg, int len, struct input_info* input_info)
{
	struct ipfix_message *message;

	message = (struct ipfix_message*) calloc (1, sizeof(struct ipfix_message));
	if (!message) {
		MSG_ERROR(msg_module, "Unable to allocate memory (%s:%d)", __FILE__, __LINE__);
		return NULL;
	}

	message->pkt_header = (struct ipfix_header*) msg;
	message->input_info = input_info;
	MSG_DEBUG(msg_module, "Processing data for Observation domain ID %d.",
			ntohl(message->pkt_header->observation_domain_id));

	/* check IPFIX version */
	if (message->pkt_header->version != htons(IPFIX_VERSION)) {
		MSG_WARNING(msg_module, "Unexpected IPFIX version detected (%X), skipping msg.",
				message->pkt_header->version);
		free(message);
		return NULL;
	}

	/* check whether message is not shorter than header says */
	if ((uint16_t) len < ntohs(message->pkt_header->length)) {
		MSG_WARNING(msg_module, "Malformed IPFIX message detected (bad length), skipping msg.");
		free (message);
		return NULL;
	}

	/* process IPFIX msg and fill up the ipfix_message structure */
    uint8_t *p = msg + IPFIX_HEADER_LENGTH;
    int t_set_count = 0;
    int ot_set_count = 0;
    int d_set_count = 0;
    struct ipfix_set_header *set_header;
    while (p < (uint8_t*) msg + ntohs(message->pkt_header->length)) {
        set_header = (struct ipfix_set_header*) p;
        switch (ntohs(set_header->flowset_id)) {
            case IPFIX_TEMPLATE_FLOWSET_ID:
                message->templ_set[t_set_count++] = (struct ipfix_template_set *) set_header;
                break;
            case IPFIX_OPTION_FLOWSET_ID:
                 message->opt_templ_set[ot_set_count++] = (struct ipfix_options_template_set *) set_header;
                break;
            default:
                if (ntohs(set_header->flowset_id) < IPFIX_MIN_RECORD_FLOWSET_ID) {
                	MSG_WARNING(msg_module, "Unknown Set ID %d", ntohs(set_header->flowset_id));
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
		MSG_DEBUG(msg_module, "Trying to make copy of the IPFIX message, but argument is NULL");
		return NULL;
	}

	packet = (uint8_t *) malloc(ntohs(msg->pkt_header->length));
	if (!packet) {
		MSG_ERROR(msg_module, "Unable to allocate memory (%s:%d)", __FILE__, __LINE__);
		return NULL;
	}
	memset(packet, 0, sizeof(ntohs(msg->pkt_header->length)));

	message = message_create_from_mem(packet, msg->pkt_header->length, msg->input_info);
	if (!message) {
		MSG_DEBUG(msg_module, "Unable to create copy of the IPFIX message");
		free(packet);
		return NULL;
	}

	return message;
}

/**
 * \brief Set corresponding templates for data records in IPFIX message.
 *
 * \param[in] msg IPFIX message
 * \param[in] tm template manager with corresponding templates
 * \return 0 on success, negative value otherwise
 */
int message_set_templates(struct ipfix_message *msg, struct ipfix_template_mgr *tm, uint32_t src_id)
{
	int ret = 0;
	int i;
	struct ipfix_template *template;
    uint16_t min_data_length;
    uint16_t data_length;
    uint16_t length = 0;
    uint16_t offset = 0;
    uint8_t var_len = 0;
    uint32_t records_count = 0;
    uint16_t count;

    struct ipfix_template_key *key = tm_key_create(ntohl(msg->pkt_header->observation_domain_id), src_id, 0);

	if (!msg || !tm) {
		return -1;
	}

    for (i=0; msg->data_couple[i].data_set != NULL && i<1023; i++) {
    	tm_key_change_template_id(key, ntohs(msg->data_couple[i].data_set->header.flowset_id));
		msg->data_couple[i].data_template = tm_get_template(tm, key);
		if (msg->data_couple[i].data_template == NULL) {
			MSG_WARNING(msg_module, "Template with ID %i not found!", ntohs(msg->data_couple[i].data_set->header.flowset_id));
		} else {
			/* compute sequence number */
			if (msg->data_couple[i].data_template->data_length & 0x80000000) {
				/*
				 * there is a Information Element with variable length. we have to
				 * compute number of the Data Records in current Set
				 */

				/* template for current Data Set */
				template = msg->data_couple[i].data_template;
				/* every Data Record has to be at least this long */
				min_data_length = (uint16_t) msg->data_couple[i].data_template->data_length;

				data_length = ntohs(msg->data_couple[i].data_set->header.length) - sizeof(struct ipfix_set_header);


				length = 0;   /* position in Data Record */
				while (data_length - length >= min_data_length) {
					offset = 0;
					for (count = 0; count < template->field_count; count++) {
						if (template->fields[offset].ie.length == 0xffff) {
							/* this element has variable length. read first byte from actual data record to determine
							 * real length of the field */
							var_len = msg->data_couple[i].data_set->records[length];
							if (var_len < 255) {
								/* field length is var_len */
								length += var_len;
								length += 1; /* first 1 byte contains information about field length */
							} else {
								/* field length is more than 255, actual length is stored in next two bytes */
								length += ntohs(*((uint16_t *) (msg->data_couple[i].data_set->records + length + 1)));
								length += 3; /* first 3 bytes contain information about field length */
							}
						} else {
							/* length from template */
							length += template->fields[offset].ie.length;
						}

						if (template->fields[offset].ie.id & 0x8000) {
							/* do not forget on Enterprise Number */
							offset += 1;
						}
						offset += 1;
					 }

					 /* data record found */
					 records_count += 1;
				 }
			 } else {
				 /* no elements with variable length */
				 records_count += ntohs(msg->data_couple[i].data_set->header.length) / msg->data_couple[i].data_template->data_length;
			 }
		 }
     }

//    tm->sequence_number += records_count;
//    msg->data_records_counter = records_count;
//    msg->current_sequence_number = ntohl(msg->pkt_header->sequence_number);


	return ret;
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
 * \param[in] template template for data record
 * \return offset of next data record in data set
 */
uint16_t get_next_data_record_offset(uint8_t *data_record, struct ipfix_template *template)
{
	if (!template) {
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
	while (count != template->field_count) {
		id = template->fields[index].ie.id;
		length = template->fields[index].ie.length;

		if (id >> 15) {
			/* Enterprise Number */
			++index;
		}

		if (length != 65535) {
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
 * \param[in] template template for data set
 * \return array of pointers to start of the Data Records in Data Set
 */
uint8_t **get_data_records(struct ipfix_data_set *data_set, struct ipfix_template *template)
{
	uint8_t *data_record;
	uint32_t offset;
	uint16_t min_record_length;
	uint8_t **records;
	uint16_t records_index = 0;

	/* TODO - make it clever */
	records = (uint8_t **) malloc(1023 * sizeof(uint8_t *));
	if (records == NULL) {
		MSG_ERROR(msg_module, "Unable to allocate memory (%s:%d)", __FILE__, __LINE__);
		return (uint8_t **) NULL;
	}
	memset(records, 0, 1023 * sizeof(uint8_t *));

	min_record_length = template->data_length;
	offset = 0;

	offset += 4;  /* size of the header */

	if (min_record_length & 0x8000) {
		/* oops, record contains fields with variable length */
		min_record_length = min_record_length & 0x7fff; /* size of the fields, variable fields excluded  */
	}

	while ((int) ntohs(data_set->header.length) - (int) offset - (int) min_record_length >= 0) {
		data_record = (((uint8_t *) data_set) + offset);
		records[records_index] = data_record;
		offset += get_next_data_record_offset(data_record, template);
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
