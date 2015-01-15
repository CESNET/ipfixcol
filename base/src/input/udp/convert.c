/**
 * \file convert.c
 * \author Michal Kozubik <kozubik.michal@gmail.com>
 * \brief Packet conversion from Netflow v5/v9 or sFlow to IPFIX format.
 *
 * Copyright (C) 2011 CESNET, z.s.p.o.
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
#include <string.h>
#include <time.h>

#include "convert.h"
#include "sflow.h"
#include "sflowtool.h"
#include "inttypes.h"


/** Netflow v5 and v9 identifiers */
#define SET_HEADER_LEN 4

#define NETFLOW_V5_VERSION 5
#define NETFLOW_V9_VERSION 9

#define NETFLOW_V5_TEMPLATE_LEN 76
#define NETFLOW_V5_DATA_SET_LEN 52
#define NETFLOW_V5_NUM_OF_FIELDS 17

#define NETFLOW_V9_TEMPLATE_SET_ID 0
#define NETFLOW_V9_OPT_TEMPLATE_SET_ID 1

#define NETFLOW_V9_END_ELEM 21
#define NETFLOW_V9_START_ELEM 22

/* Offsets of timestamps in netflow v5 data record */
#define FIRST_OFFSET 24
#define LAST_OFFSET 28

/** IPFIX Element IDs used when creating Template Set */
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

/** Defines for numbers of bytes */
#define BYTES_1 1
#define BYTES_2 2
#define BYTES_4 4
#define BYTES_8 8
#define BYTES_12 12

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
		IPFIX_MIN_RECORD_FLOWSET_ID, NETFLOW_V5_DATA_SET_LEN + SET_HEADER_LEN
};

/* Sequence numbers for NFv5,9 and sFlow */
static uint32_t seqNo[3] = {0,0,0};
#define NF5_SEQ_N 0
#define NF9_SEQ_N 1
#define SF_SEQ_N  2

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
	for (i = 0; i < NETFLOW_V5_TEMPLATE_LEN/2; i++) {
		netflow_v5_template[i] = htons(netflow_v5_template[i]);
	}
	netflow_v5_data_header[0] = htons(netflow_v5_data_header[0]);
	netflow_v5_data_header[1] = htons(netflow_v5_data_header[1]);
}

int convert_init(int in_plugin, int len) {
	/* Initialize templates structure */
	templates.max = 30;
	templates.cols = 2;

	/* Allocate memory */
	templates.templ = malloc(templates.max * templates.cols * sizeof(int));

	if (templates.templ == NULL) {
		return 1;
	}

	/* Initialize allocated memory */
	int i;
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

int templates_realloc() {
	/* More templates are needed, realloc memory */
	templates.max += 20;

	templates.templ = realloc(templates.templ, templates.max * templates.cols * sizeof(int));

	if (templates.templ == NULL) {
		return 1;
	}

	/* Initialize allocated memory */
	int i;
	for (i = (templates.max - 20) * templates.cols; i < templates.max * templates.cols; i++) {
			templates.templ[i] = 0;
	}

	return 0;
}

void convert_close() {
	/* Free allocated memory */
	free(templates.templ);
}

/**
 * \brief Inserts Template Set into packet and updates input_info
 *
 * Also sets total length of packet
 *
 * \param[out] packet Flow information data in the form of IPFIX packet.
 * \param[in] input_info Structure with informations needed for inserting Template Set
 * \param[in] numOfFlowSamples Number of flow samples in sFlow datagram
 * \return Total length of packet
 */
uint16_t insert_template_set(char **packet, int numOfFlowSamples, ssize_t *len)
{
	/* Template Set insertion if needed */
	/* Check conf->info_list->info.template_life_packet and template_life_time */
	struct ipfix_header *header = (struct ipfix_header *) *packet;

	/* Remove last 4 bytes (padding) of each data record in packet */
	int i;
	for (i = numOfFlowSamples - 1; i > 0; i--) {
		uint32_t pos = IPFIX_HEADER_LENGTH + (i * (NETFLOW_V5_DATA_SET_LEN + BYTES_4));
		memmove(*packet + pos - BYTES_4, *packet + pos, (numOfFlowSamples - i) * NETFLOW_V5_DATA_SET_LEN);
	}

	/* Insert Data Set header */
	if (numOfFlowSamples > 0) {
		netflow_v5_data_header[1] = htons(NETFLOW_V5_DATA_SET_LEN * numOfFlowSamples + SET_HEADER_LEN);
		memmove(*packet + IPFIX_HEADER_LENGTH + BYTES_4, *packet + IPFIX_HEADER_LENGTH, buff_len - IPFIX_HEADER_LENGTH - BYTES_4);
		memcpy(*packet + IPFIX_HEADER_LENGTH, netflow_v5_data_header, BYTES_4);
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
				return htons(IPFIX_HEADER_LENGTH + NETFLOW_V5_TEMPLATE_LEN + (NETFLOW_V5_DATA_SET_LEN * numOfFlowSamples));
			} else {
				return htons(IPFIX_HEADER_LENGTH + (NETFLOW_V5_DATA_SET_LEN * numOfFlowSamples));
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
				if (numOfFlowSamples > 0) {
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

			return htons(IPFIX_HEADER_LENGTH + NETFLOW_V5_TEMPLATE_LEN + (NETFLOW_V5_DATA_SET_LEN * numOfFlowSamples));
		} else {
			return htons(IPFIX_HEADER_LENGTH + (NETFLOW_V5_DATA_SET_LEN * numOfFlowSamples));
		}
	} else if (inserted == 0) {
		inserted = 1;

		memmove(*packet + IPFIX_HEADER_LENGTH + NETFLOW_V5_TEMPLATE_LEN, *packet + IPFIX_HEADER_LENGTH, buff_len - NETFLOW_V5_TEMPLATE_LEN - IPFIX_HEADER_LENGTH);
		memcpy(*packet + IPFIX_HEADER_LENGTH, netflow_v5_template, NETFLOW_V5_TEMPLATE_LEN);
		*len += NETFLOW_V5_TEMPLATE_LEN;

		return htons(IPFIX_HEADER_LENGTH + NETFLOW_V5_TEMPLATE_LEN + (NETFLOW_V5_DATA_SET_LEN * numOfFlowSamples));
	} else {
		return htons(IPFIX_HEADER_LENGTH + (NETFLOW_V5_DATA_SET_LEN * numOfFlowSamples));
	}
}

/* \brief Inserts 64b timestamps into netflow v9 template
 *
 * Finds original timestamps (e.id 21 and 22) and replaces them with 152 and 153
 *
 * \param templSet pointer to Template Set
 */
int insert_timestamp_template(struct ipfix_set_header *templSet)
{
	struct ipfix_set_header *tmp;
	uint16_t len, num, i, id;

	tmp = templSet;

	/* Get template set total length without set header length */
	len = ntohs(tmp->length) - 4;

	/* Iterate through all templates */
	while ((uint8_t *) tmp < (uint8_t *) templSet + len) {
		tmp++;

		/* Get template ID and number of elements */
	    id = ntohs(tmp->flowset_id) - IPFIX_MIN_RECORD_FLOWSET_ID;
	    num = ntohs(tmp->length);

	    if (id >= templates.max) {
	    	templates.max += 20;
	    	templates.templ = realloc(templates.templ, templates.max * templates.cols * sizeof(int));

	    	if (templates.templ == NULL) {
	    		return 1;
	    	}
	    	int i;
	    	for (i = (templates.max - 20) * templates.cols; i < templates.max * templates.cols; i++) {
	    			templates.templ[i] = 0;
	    	}
	    }

	    int len = templates.cols * id;
	    int pos = len + 1;

	    /* Set default values for length and timestamp position */
	    templates.templ[len] = 0;
	    templates.templ[pos] = -1;

	    /* Iterate through all elements */
	    for (i = 0; i < num; i++) {
	        tmp++;

	        /* We are looking for timestamps - elements with id 21 (end) and 22 (start) */
	        if (ntohs(tmp->flowset_id) == NETFLOW_V9_END_ELEM) {
	        	/* We don't know which one comes first so we need to check it */
	        	if (templates.templ[pos] == -1) {
	        		templates.templ[pos] = templates.templ[len];
	        	}

	        	/* Change element ID and element length (32b -> 64b) */
	        	tmp->flowset_id = htons(FLOW_END);
	        	tmp->length = htons(BYTES_8);
	        	templates.templ[len] += BYTES_4;
	        } else if (ntohs(tmp->flowset_id) == NETFLOW_V9_START_ELEM) {
	        	/* Do the same thing for element 22 */
	        	if (templates.templ[pos] == -1) {
					templates.templ[pos] = templates.templ[len];
				}

	        	tmp->flowset_id = htons(FLOW_START);
	        	tmp->length = htons(BYTES_8);
	        	templates.templ[len] += BYTES_4;
	        } else {
	        	templates.templ[len] += ntohs(tmp->length);
	        }
	    }
	}
	return 0;
}

/* \brief Inserts 64b timestamps into netflow v9 Data Set
 *
 * Finds original timestamps and replaces them by 64b ipfix timestamps
 *
 * \param dataSet pointer to Data Set
 * \param time_header time from packet header
 * \param remaining bytes behind this data set in packet
 */
int insert_timestamp_data(struct ipfix_set_header *dataSet, uint64_t time_header, uint32_t remaining)
{
	struct ipfix_set_header *tmp;
	uint8_t *pkt;
	uint16_t id, len, num, shifted, first_offset, last_offset;
	int i;

	tmp = dataSet;

	/* Get used template id and data set length */
	id = ntohs(tmp->flowset_id) - IPFIX_MIN_RECORD_FLOWSET_ID;
	len = ntohs(tmp->length) - 4;

	int lenIndex = templates.cols * id;
	int posIndex = lenIndex + 1;

	/* Get number of data records using the same template */
	if (templates.templ[lenIndex] <= 0) {
		return 0;
	}
	num = len / templates.templ[lenIndex];
	if (num == 0) {
		return 0;
	}

	/* Increase sequence number */
	seqNo[NF9_SEQ_N] += num;

	/* Iterate through all data records */
	shifted = 0;
	first_offset = templates.templ[posIndex];
	last_offset = first_offset + 4;

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
				(templates.templ[lenIndex] + BYTES_4 - last_offset) +	(remaining - len));

		/* Set time values */
		*((uint64_t *)(pkt + first_offset)) = htobe64(time_header + first);
		*((uint64_t *)(pkt + last_offset + BYTES_4)) = htobe64(time_header + last);

		shifted++;
	}

	/* Increase set header length and packet total length*/
	tmp->length = htons(len + 4 + (shifted * BYTES_8));
	return shifted;
}

/**
 * \brief Converts packet from Netflow v5/v9 or sFlow format to IPFIX format
 *
 * Netflow v9 has almost the same format as ipfix but it has different Flowset IDs
 * and more informations in packet header.
 * Netflow v5 doesn't have (Option) Template Sets so they must be inserted into packet
 * with some other data that are missing (data set header etc.). Template is periodicaly
 * refreshed according to input_info.
 * sFlow format is very complicated - InMon Corp. source code is used (modified)
 * which converts it into Netflow v5 packet.
 *
 * \param[out] packet Flow information data in the form of IPFIX packet.
 * \param[in] len Length of packet
 * \param[in] input_info Information structure storing data needed for refreshing templates
 */
void convert_packet(char **packet, ssize_t *len, char *input_info)
{
	struct ipfix_header *header = (struct ipfix_header *) *packet;
	uint16_t numOfFlowSamples = 0;
	info_list = (struct input_info_list *) input_info;
	switch (htons(header->version)) {
		/* Netflow v9 packet */
		case NETFLOW_V9_VERSION: {
			uint64_t sysUp = ntohl(*((uint32_t *) (((uint8_t *)header) + 4)));
			uint64_t unSec = ntohl(*((uint32_t *) (((uint8_t *)header) + 8)));

			uint64_t time_header = (unSec * 1000) - sysUp;

			memmove(*packet + BYTES_4, *packet + BYTES_8, buff_len - BYTES_8);
			memset(*packet + buff_len - BYTES_8, 0, BYTES_4);

			*len -= BYTES_4;

			header->length = htons(IPFIX_HEADER_LENGTH);
			header->sequence_number = htonl(seqNo[NF9_SEQ_N]);

			uint8_t *p = (uint8_t *) (*packet + IPFIX_HEADER_LENGTH);
			struct ipfix_set_header *set_header;

			while (p < (uint8_t*) *packet + *len) {
				set_header = (struct ipfix_set_header*) p;

				switch (ntohs(set_header->flowset_id)) {
					case NETFLOW_V9_TEMPLATE_SET_ID:
						set_header->flowset_id = htons(IPFIX_TEMPLATE_FLOWSET_ID);
						if (ntohs(set_header->length) > 0) {
							if (insert_timestamp_template(set_header) != 0) {
								return;
							}
						}
						break;
					case NETFLOW_V9_OPT_TEMPLATE_SET_ID:
						set_header->flowset_id = htons(IPFIX_OPTION_FLOWSET_ID);
						break;
					default:
						if (ntohs(set_header->length) > 0) {
							uint16_t shifted;
							shifted = insert_timestamp_data(set_header, time_header, *len - ntohs(header->length));
							*len += (shifted * 8);
						}
						break;
				}

				header->length = htons(ntohs(header->length)+ntohs(set_header->length));

				if (ntohs(header->length) > *len) {
					/* Real length of packet is smaller than it should be */
					return;
				}

				if (ntohs(set_header->length) == 0) {
					break;
				}
				p += ntohs(set_header->length);
			}

			break; }

		/* Netflow v5 packet */
		case NETFLOW_V5_VERSION: {
			uint64_t sysUp = ntohl(*((uint32_t *) (((uint8_t *)header) + 4)));
			uint64_t unSec = ntohl(*((uint32_t *) (((uint8_t *)header) + 8)));
			uint64_t unNsec = ntohl(*((uint32_t *) (((uint8_t *)header) + 12)));

			uint64_t time_header = (unSec * 1000) + unNsec/1000000;

			numOfFlowSamples = ntohs(header->length);
			/* Header modification */
			header->export_time = header->sequence_number;
			memmove(*packet + BYTES_8, *packet + IPFIX_HEADER_LENGTH, buff_len - IPFIX_HEADER_LENGTH);
			memmove(*packet + BYTES_12, *packet + BYTES_12 + BYTES_1, BYTES_1);
			header->observation_domain_id = header->observation_domain_id&(0xF000);

			/* Update real packet length because of memmove() */
			*len = *len - BYTES_8;

			/* We need to resize time element (first and last seen) fron 32 bit to 64 bit */
			int i;
			uint8_t *pkt;
			uint16_t shifted = 0;
			for (i = numOfFlowSamples - 1; i >= 0; i--) {
				/* Resize each timestamp in each data record to 64 bit */
				pkt = (uint8_t *) (*packet + IPFIX_HEADER_LENGTH + (i * (NETFLOW_V5_DATA_SET_LEN - BYTES_4)));
				uint64_t first = ntohl(*((uint32_t *) (pkt + FIRST_OFFSET)));
				uint64_t last  = ntohl(*((uint32_t *) (pkt + LAST_OFFSET)));

				memmove(pkt + LAST_OFFSET + BYTES_8, pkt + LAST_OFFSET,
						(shifted * (NETFLOW_V5_DATA_SET_LEN + BYTES_4)) + (NETFLOW_V5_DATA_SET_LEN - LAST_OFFSET));

				/* Set time values */
				*((uint64_t *)(pkt + FIRST_OFFSET)) = htobe64(time_header - (sysUp - first));
				*((uint64_t *)(pkt + LAST_OFFSET + BYTES_4)) = htobe64(time_header - (sysUp - last));
				shifted++;
			}

			/* Set right packet length according to memmoves */
			*len += shifted * BYTES_8;

			/* Template Set insertion (if needed) and setting packet length */
			header->length = insert_template_set(packet, numOfFlowSamples, len);

			header->sequence_number = htonl(seqNo[NF5_SEQ_N]);
			if (*len >= htons(header->length)) {
				seqNo[NF5_SEQ_N] += numOfFlowSamples;
			}

			break; }

		/* SFLOW packet (converted to Netflow v5 like packet */
		default:
			/* Conversion from sflow to Netflow v5 like IPFIX packet */
			numOfFlowSamples = Process_sflow(*packet, *len);
			if (numOfFlowSamples < 0) {
				/* Make header->length bigger than packet lenght so error will occur and packet will be skipped */
				header->length = *len + 1;
				return;
			}

			/* Observation domain ID is unknown */
			header->observation_domain_id = 0; // ??

			header->export_time = htonl((uint32_t) time(NULL));

			/* Template Set insertion (if needed) and setting total packet length */
			header->length = insert_template_set(packet, numOfFlowSamples, len);

			header->sequence_number = htonl(seqNo[SF_SEQ_N]);
			if (*len >= htons(header->length)) {
				seqNo[SF_SEQ_N] += numOfFlowSamples;
			}
			break;
	}
	header->version = htons(IPFIX_VERSION);
}
