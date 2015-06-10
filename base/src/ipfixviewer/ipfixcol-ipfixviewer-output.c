/**
 * \file ipfixcol-ipfixviewer-output.c
 * \author Michal Srb <michal.srb@cesnet.cz>
 * \brief Plugin for displaying IPFIX data stored in IPFIX file format.
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <arpa/inet.h>
#include <endian.h>
#include <inttypes.h>
#include <time.h>
#include <string.h>

#include "ipfixcol.h"

/**
 * \defgroup ipfixviewer IPFIXviewer plugin
 * \ingroup storagePlugins
 *
 * This plugin actually does not store anything anywhere.
 * It just prints out IPFIX data from messages.
 *
 * @{
 */

/* API version constant */
IPFIXCOL_API_VERSION;

/** Identifier to MSG_* macros */
static char *msg_module = "ipfixviewer";

struct viewer_config {
	/* structure so far empty */
	int empty;
};

/* some auxiliary functions for extracting data of exact length */
#define read8(ptr) (*((uint8_t *) (ptr)))
#define read16(ptr) (*((uint16_t *) (ptr)))
#define read32(ptr) (*((uint32_t *) (ptr)))
#define read64(ptr) (*((uint64_t *) (ptr)))

/**
 * \brief Print IPFIX message header
 *
 * \param[in] hdr message header structure
 * \return nothing
 */
static void print_header(struct ipfix_header *hdr)
{
	time_t time = (time_t) ntohl(hdr->export_time);
	char *str_time = ctime(&time);
	str_time[strlen(str_time) - 1] = '\0';

	printf("--------------------------------------------------------------------------------\n");
	printf("IPFIX Message Header:\n");
	printf("\tVersion: %u\n", ntohs(hdr->version));
	printf("\tLength: %u\n", ntohs(hdr->length));
	printf("\tExport Time: %u (%s)\n", ntohl(hdr->export_time), str_time);
	printf("\tSequence Number: %u\n", ntohl(hdr->sequence_number));
	printf("\tObservation Domain ID: %u\n", ntohl(hdr->observation_domain_id));
}

/**
 * \brief Print set header
 *
 * \param[in] set_header set header structure
 * \return nothing
 */
static void print_set_header(struct ipfix_template_set *template_set)
{
	struct ipfix_set_header *set_header = (struct ipfix_set_header *) template_set;

	printf("Set Header:\n");
	printf("\tSet ID: %u", ntohs(set_header->flowset_id));

	switch (ntohs(set_header->flowset_id)) {
	case (2):
		printf(" (Template Set)\n");
		break;
	case (3):
		printf(" (Options Template Set)\n");
		break;
	default:
		if (ntohs(set_header->flowset_id) >= IPFIX_MIN_RECORD_FLOWSET_ID) {
			printf(" (Data Set)\n");
		} else {
			printf(" (Unknown ID)\n");
		}
		break;
	}

	printf("\tLength: %u\n", ntohs(set_header->length));
}

/**
 * \brief Print template record
 *
 * \param[in] rec template record structure
 * \return length of the template in bytes (including header).
 */
static uint16_t print_template_record(struct ipfix_template_record *rec)
{
	uint16_t count = 0;
	uint16_t offset = 0;
	uint16_t index;

	printf("Template Record Header:\n");
	printf("\tTemplate ID: %u\n", ntohs(rec->template_id));
	printf("\tField Count: %u\n", ntohs(rec->count));
	offset += 4;    /* template record header */

	printf("Fields:\n");

	index = count;

	/* print fields */
	while (count != ntohs(rec->count)) {
		printf("\tIE ID: %u\t", ntohs(rec->fields[index].ie.id) & 0x7fff);
		if (ntohs(rec->fields[index].ie.length) != VAR_IE_LENGTH) {
			printf("\tField Length: %u", ntohs(rec->fields[index].ie.length));
		} else {
			printf("\tField Length: variable");
		}
		offset += 4;

		/* check for enterprise number bit */
		if (ntohs(rec->fields[index].ie.id) >> 15) {
			/* Enterprise number follows */
			++index;
			printf(" (PEN:%u)", ntohl(rec->fields[index].enterprise_number));
			offset += 4;
		}

		printf("\n");

		++index;
		++count;
	}

	return offset;
}

/**
 * \brief Print options template record
 *
 * \param[in] rec options template record structure
 * \return length of the template in bytes (including header)
 */
static uint16_t print_options_template_record(struct ipfix_options_template_record *rec)
{
	uint16_t count = 0;       /* number of fields processed */
	uint16_t offset = 0;      /* offset in 'rec' structure (bytes) */
	uint16_t index;           /* index in rec->fields[] */

	printf("Options Template Record Header:\n");
	printf("\tTemplate ID: %u\n", ntohs(rec->template_id));
	printf("\tField Count: %u\n", ntohs(rec->count));
	printf("\tScope Field Count: %u\n", ntohs(rec->scope_field_count));
	offset += 6;              /* size of the Opt. Template Record Header */

	printf("Fields:\n");

	index = count;

	while (count != ntohs(rec->count)) {
		printf("\tIE ID: %u\t", ntohs(rec->fields[index].ie.id) & 0x7fff);
		if (ntohs(rec->fields[index].ie.length) != VAR_IE_LENGTH) {
			printf("\tField Length: %u", ntohs(rec->fields[index].ie.length));
		} else {
			/* field has variable length */
			printf("\tField Length: variable");
		}
		offset += 4;

		/* check for enterprise number bit */
		if (ntohs(rec->fields[index].ie.id) >> 15) {
			/* Enterprise number follows */
			++index;
			printf(" (PEN:%u)", ntohl(rec->fields[index].enterprise_number));
			offset += 4;
		}

		printf("\n");

		++index;
		++count;
	}

	return offset;
}

/**
 * \brief Print all template sets in IPFIX message
 *
 * \param[in] ipfix_msg IPFIX message
 * \return 0 on success, -1 otherwise
 */
static int print_template_sets(const struct ipfix_message *ipfix_msg)
{
	uint16_t template_index = 0;
	struct ipfix_template_set *template_set;
	struct ipfix_template_record *template_record;
	uint32_t offset;
	int padding;

	if (!ipfix_msg) {
		return -1;
	}

	/* pick up first template set in the message */
	template_set = ipfix_msg->templ_set[template_index];

	while (template_set) {
		printf("\n\n");
		offset = 0;

		/* print template set header */
		print_set_header(template_set);
		offset += 4;  /* size of the set header */

		while ((int) ntohs(template_set->header.length) - (int) offset >= 8) {
			template_record = (struct ipfix_template_record *) (((uint8_t *) template_set) + offset);
			/* print template record */
			offset += print_template_record(template_record);
		}

		/* compute possible padding */
		padding = (int) ntohs(template_set->header.length) - (int) offset;
		if (padding > 0) {
			printf("Padding: %d\n", padding);
		}

		/* process next set */
		template_set = ipfix_msg->templ_set[++template_index];
	}

	return 0;
}

/**
 * \brief Print all options template sets in IPFIX message
 *
 * \param[in] ipfix_msg IPFIX message
 * \return 0 on success, -1 otherwise
 */
static int print_options_template_sets(const struct ipfix_message *ipfix_msg)
{
	uint16_t template_index = 0;
	struct ipfix_options_template_set *template_set;
	struct ipfix_options_template_record *options_template_record;
	uint32_t offset;
	int padding;

	if (!ipfix_msg) {
		return -1;
	}

	template_set = ipfix_msg->opt_templ_set[template_index];

	while (template_set) {
		printf("\n\n");
		offset = 0;

		/* print options template set header */
		print_set_header((struct ipfix_template_set *) template_set);
		offset += 4;  /* size of the header */

		while ((int) ntohs(template_set->header.length) - (int) offset >= 12) {
			options_template_record = (struct ipfix_options_template_record *) (((uint8_t *) template_set) + offset);
			/* print options template record */
			offset += print_options_template_record(options_template_record);
		}

		/* compute possible padding */
		padding = (int) ntohs(template_set->header.length) - (int) offset;
		if (padding > 0) {
			printf("Padding: %d\n", padding);
		}

		/* process next set */
		template_set = ipfix_msg->opt_templ_set[++template_index];
	}

	return 0;
}

/**
 * \brief Print data record
 *
 * \param[in] data_record IPFIX data record
 * \param[in] template corresponding template
 * \return length of the data record
 */
static uint16_t print_data_record(uint8_t *data_record, struct ipfix_template *template)
{
	int i;

	if (!template) {
		/* we don't have template for this data set */
		fprintf(stderr, "ERROR: no template for this data set\n");
		return 0;
	}

	uint16_t count = 0;
	uint16_t offset = 0;
	uint16_t index;
	uint16_t length;
	uint16_t id;

	index = count;

	/* print all fields */
	while (count != template->field_count) {
		id = template->fields[index].ie.id;
		printf("\tIE ID: %u", id & 0x7fff);

		length = template->fields[index].ie.length;

		if (id >> 15) {
			/* Enterprise Number */
			printf(" (PEN:%u)\t", template->fields[++index].enterprise_number);
		} else {
			printf("\t\t");
		}

		switch (length) {
		case (1):
			printf("Value: %#x\n", read8(data_record+offset));
			offset += 1;
			break;
		case (2):
			printf("Value: %#x\n", read16(data_record+offset));
			offset += 2;
			break;
		case (4):
			printf("Value: %#x\n", read32(data_record+offset));
			offset += 4;
			break;
		case (8):
			printf("Value: %#lx\n", read64(data_record+offset));
			offset += 8;
			break;
		default:
			if (length != VAR_IE_LENGTH) {
				printf("Value: 0x");

				for (i = 0; i < length; i++) {
					printf("%02x", (data_record+offset)[i]);
				}
				printf("\n");

				offset += length;
			} else {
				/* variable length */
				length = read8(data_record+offset);
				offset += 1;

				if (length == 255) {
					length = ntohs(read16(data_record+offset));
					offset += 2;
				}

				printf("Value: 0x");

				for (i = 0; i < length; i++) {
					printf("%02x", (data_record+offset)[i]);
				}
				printf("\n");

				offset += length;
			}
			break;
		}

		++index;
		++count;
	}

	return offset;
}

/**
 * \brief Print all data sets in IPFIX message
 *
 * \param[in] ipfix_msg IPFIX message
 * \return 0 on success, -1 otherwise
 */
static int print_data_sets(const struct ipfix_message *ipfix_msg)
{
	uint16_t data_index = 0;
	struct ipfix_data_set *data_set;
	uint8_t *data_record;
	struct ipfix_template *template;
	uint16_t counter = 1;
	uint32_t offset;
	uint16_t min_record_length;
	int padding;

	data_set = ipfix_msg->data_couple[data_index].data_set;

	while (data_set) {
		printf("\n\n");
		template = ipfix_msg->data_couple[data_index].data_template;
		if (!template) {
			/* Data set without template, skip it */
			data_set = ipfix_msg->data_couple[++data_index].data_set;
			continue;
		}
		min_record_length = template->data_length;
		offset = 0;
		counter = 1;

		print_set_header((struct ipfix_template_set *) data_set);
		offset += 4;  /* size of the header */

		if (min_record_length & 0x8000) {
			/* oops, record contains fields with variable length */
			min_record_length = min_record_length & 0x7fff; /* size of the fields, variable fields excluded  */
		}

		while ((int) ntohs(data_set->header.length) - (int) offset - (int) min_record_length >= 0) {
			data_record = (((uint8_t *) data_set) + offset);
			/* print data record */
			printf("Data Record (#%u):\t\t(network byte order)\n", counter++);
			offset += print_data_record(data_record, template);
		}

		/* compute possible padding */
		padding = (int) ntohs(data_set->header.length) - (int) offset;
		if (padding > 0) {
			printf("Padding: %d\n", padding);
		}

		/* process next set */
		data_set = ipfix_msg->data_couple[++data_index].data_set;
	}

	return 0;
}

/**
 * \brief Storage plugin initialization.
 *
 * Initialize IPFIX storage plugin. This function allocates, fills and
 * returns config structure.
 *
 * \param[in] params parameters for this storage plugin
 * \param[out] config the plugin specific configuration structure
 * \return 0 on success, negative value otherwise
 */
int storage_init(char *params, void **config)
{
	(void) params;
	struct viewer_config *conf;

	conf = (struct viewer_config *) calloc(1, sizeof(*conf));
	if (!conf) {
		MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)", __FILE__, __LINE__);
		return -1;
	}

	*config = conf;
	return 0;
}

/**
 * \brief Show IPFIX data
 *
 * The plugin actually doesn't store anything anywhere.
 * It just prints out IPFIX data from messages.
 *
 * \param[in] config the plugin specific configuration structure
 * \param[in] ipfix_msg IPFIX message
 * \param[in] template_mgr Template manager
 * \return 0 on success, negative value otherwise
 */
int store_packet(void *config, const struct ipfix_message *ipfix_msg,
		const struct ipfix_template_mgr *template_mgr)
{
	struct viewer_config *conf;
	(void) template_mgr;

	if (config == NULL || ipfix_msg == NULL) {
		return -1;
	}

	conf = (struct viewer_config *) config;
	conf->empty = 1; /* just to suppress compiler's warning */

	/* print header */
	print_header(ipfix_msg->pkt_header);

	print_template_sets(ipfix_msg);
	print_options_template_sets(ipfix_msg);
	print_data_sets(ipfix_msg);

	return 0;
}

/**
 * \brief This function does nothing in ipfixviewer plugin
 *
 * Just flush all buffers.
 *
 * \param[in] config the plugin specific configuration structure
 * \return 0 on success, negative value otherwise
 */
int store_now(const void *config)
{
	/* nothing to do */
	(void) config;
	return 0;
}

/**
 * \brief Remove storage plugin.
 *
 * This function is called when we don't want to use this storage plugin
 * anymore. All it does is that it cleans up after the storage plugin.
 *
 * \param[in] config the plugin specific configuration structure
 * \return 0 on success, negative value otherwise
 */
int storage_close(void **config)
{
	struct viewer_config *conf = (struct viewer_config *) *config;
	free(conf);
	return 0;
}

/**@}*/
