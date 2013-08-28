/**
 * \file udp_input.c
 * \author Petr Velan <petr.velan@cesnet.cz>
 * \brief IPFIX Collector UDP Input Plugin
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

/**
 * \defgroup udpInput UDP input plugin for ipfixcol
 * \ingroup inputPLugins
 *
 * This is implementation of the input plugin API for UDP network input.
 * Input parameters are passed in xml format
 *
 * @{
 */

#include <stdint.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <time.h>

#include <ipfixcol.h>
#include "sflow.h"
#include "sflowtool.h"

/* input buffer length */
#define BUFF_LEN 10000
/* default port for udp collector */
#define DEFAULT_PORT "4739"

/** Identifier to MSG_* macros */
static char *msg_module = "UDP input";

/** UDP input plugin identification for packet conversion from netflow to ipfix format */
#define UDP_INPUT_PLUGIN

/** Netflow v5 and v9 identifiers */
#define SET_HEADER_LEN 4

#define NETFLOW_V5_VERSION 5
#define NETFLOW_V9_VERSION 9

#define NETFLOW_V5_TEMPLATE_LEN 76
#define NETFLOW_V5_DATA_SET_LEN 52
#define NETFLOW_V5_NUM_OF_FIELDS 17

#define NETFLOW_V9_TEMPLATE_SET_ID 0
#define NETFLOW_V9_OPT_TEMPLATE_SET_ID 1

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

static uint8_t modified = 0;
static uint8_t inserted = 0;

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

/**
 * \struct plugin_conf
 * \brief  Plugin configuration structure passed by the collector
 */
struct plugin_conf
{
	int socket; /**< listening socket */
	struct input_info_network info; /**< infromation structure passed
									  * to collector */
	struct input_info_list *info_list; /**< list of infromation structures
		 	 	 	 	 	 	 	 	 	* passed to collector */
};

/**
 * \brief Input plugin initializtion function
 *
 * \param[in]  params XML with input parameters
 * \param[out] config  Sets source and destination IP, destination port.
 * \return 0 on success, nonzero else.
 */
int input_init(char *params, void **config)
{
	/* necessary structures */
	struct addrinfo *addrinfo=NULL, hints;
	struct plugin_conf *conf=NULL;
	char *port = NULL, *address = NULL;
	int ai_family = AF_INET6; /* IPv6 is default */
	char dst_addr[INET6_ADDRSTRLEN];
	int ret, ipv6_only = 0, retval = 0;
	/* 1 when using default port - don't free memory */
	int def_port = 0;

	/* allocate plugin_conf structure */
	conf = calloc(1, sizeof(struct plugin_conf));
	if (conf == NULL) {
		MSG_ERROR(msg_module, "Cannot allocate memory for config structure: %s", strerror(errno));
		retval = 1;
		goto out;
	}

	/* parse params */
	xmlDoc *doc = NULL;
	xmlNode *root_element = NULL;
	xmlNode *cur_node = NULL;

	/* parse xml string */
	doc = xmlParseDoc(BAD_CAST params);
	if (doc == NULL) {
		printf("%s", params);
		MSG_ERROR(msg_module, "Cannot parse config xml");
		retval = 1;
		goto out;
	}
	/* get the root element node */
	root_element = xmlDocGetRootElement(doc);
	if (root_element == NULL) {
		MSG_ERROR(msg_module, "Cannot get document root element");
		retval = 1;
		goto out;
	}

	/* check that we have the right config xml, BAD_CAST is (xmlChar *) cast defined by libxml */
	if (!xmlStrEqual(root_element->name, BAD_CAST "udpCollector")) {
		MSG_ERROR(msg_module, "Expecting udpCollector root element, got %s", root_element->name);
		retval = 1;
		goto out;
	}

	/* go over all elements */
	for (cur_node = root_element->children; cur_node; cur_node = cur_node->next) {
		if (cur_node->type == XML_ELEMENT_NODE && cur_node->children != NULL) {
			/* copy value to memory - don't forget the terminating zero */
			char *tmp_val = malloc(sizeof(char)*strlen((char *)cur_node->children->content)+1);
			/* this is not a preferred cast, but we really want to use plain chars here */
			strcpy(tmp_val, (char *)cur_node->children->content);
			if (tmp_val == NULL) {
				MSG_ERROR(msg_module, "Cannot allocate memory: %s", strerror(errno));
				retval = 1;
				goto out;
			}

			if (xmlStrEqual(cur_node->name, BAD_CAST "localPort")) { /* set local port */
				port = tmp_val;
			} else if (xmlStrEqual(cur_node->name, BAD_CAST "localIPAddress")) { /* set local address */
				address = tmp_val;
			/* save following configuration to input_info */
			} else if (xmlStrEqual(cur_node->name, BAD_CAST "templateLifeTime")) {
				conf->info.template_life_time = tmp_val;
			} else if (xmlStrEqual(cur_node->name, BAD_CAST "optionsTemplateLifeTime")) {
				conf->info.options_template_life_time = tmp_val;
			} else if (xmlStrEqual(cur_node->name, BAD_CAST "templateLifePacket")) {
				conf->info.template_life_packet = tmp_val;
			} else if (xmlStrEqual(cur_node->name, BAD_CAST "optionsTemplateLifePacket")) {
				conf->info.options_template_life_packet = tmp_val;
			} else { /* unknown parameter, ignore */
				free(tmp_val);
			}
		}
	}

	/* set default port if none given */
	if (port == NULL) {
		port = DEFAULT_PORT;
		def_port = 1;
	}

	/* specify parameters of the connection */
	memset (&hints, 0, sizeof(struct addrinfo));
	hints.ai_socktype = SOCK_DGRAM; /* UDP */
	hints.ai_family = ai_family; /* both IPv4 and IPv6*/
	hints.ai_flags = AI_V4MAPPED; /* we want to accept mapped addresses */
	if (address == NULL) {
		hints.ai_flags |= AI_PASSIVE; /* no address given, listen on all local addresses */
	}

	/* get server address */
	if ((ret = getaddrinfo(address, port, &hints, &addrinfo)) != 0) {
		MSG_ERROR(msg_module, "getaddrinfo failed: %s", gai_strerror(ret));
		retval = 1;
		goto out;
	}

	/* create socket */
	conf->socket = socket(addrinfo->ai_family, addrinfo->ai_socktype, addrinfo->ai_protocol);
	if (conf->socket == -1) {
		MSG_ERROR(msg_module, "Cannot create socket: %s", strerror(errno));
		retval = 1;
		goto out;
	}

	/* allow IPv4 connections on IPv6 */
	if ((addrinfo->ai_family == AF_INET6) &&
		(setsockopt(conf->socket, IPPROTO_IPV6, IPV6_V6ONLY, &ipv6_only, sizeof(ipv6_only)) == -1)) {
		MSG_WARNING(msg_module, "Cannot turn off socket option IPV6_V6ONLY. Plugin might not accept IPv4 connections");
	}

	/* bind socket to address */
	if (bind(conf->socket, addrinfo->ai_addr, addrinfo->ai_addrlen) != 0) {
		MSG_ERROR(msg_module, "Cannot bind socket: %s", strerror(errno));
		retval = 1;
		goto out;
	}

	/* fill in general information */
	conf->info.type = SOURCE_TYPE_UDP;
	conf->info.dst_port = atoi(port);
	if (addrinfo->ai_family == AF_INET) { /* IPv4 */
		conf->info.l3_proto = 4;

		/* copy dst IPv4 address */
		conf->info.dst_addr.ipv4.s_addr =
			((struct sockaddr_in*) addrinfo->ai_addr)->sin_addr.s_addr;

		inet_ntop(AF_INET, &conf->info.dst_addr.ipv4, dst_addr, INET6_ADDRSTRLEN);
	} else { /* IPv6 */
		conf->info.l3_proto = 6;

		/* copy dst IPv6 address */
		int i;
		for (i=0; i<4;i++) {
			conf->info.dst_addr.ipv6.s6_addr32[i] =
				((struct sockaddr_in6*) addrinfo->ai_addr)->sin6_addr.s6_addr32[i];
		}

		inet_ntop(AF_INET6, &conf->info.dst_addr.ipv6, dst_addr, INET6_ADDRSTRLEN);
	}
	/* print info */
	MSG_NOTICE(msg_module, "UDP input plugin listening on address %s, port %s", dst_addr, port);

	/* and pass it to the collector */
	*config = (void*) conf;

	/* normal exit, all OK */
	MSG_NOTICE(msg_module, "Plugin initialization completed successfully");

out:
	if (def_port == 0 && port != NULL) { /* free when memory was actually allocated*/
		free(port);
	}

	if (address != NULL) {
		free(address);
	}

	if (addrinfo != NULL) {
		freeaddrinfo(addrinfo);
	}

	/* free the xml document */
	if (doc != NULL) {
		xmlFreeDoc(doc);
	}

	/* free the global variables that may have been allocated by the xml parser */
	xmlCleanupParser();

	/* free input_info when error occured */
	if (retval != 0 && conf != NULL) {
		if (conf->info.template_life_time != NULL) {
			free (conf->info.template_life_time);
		}
		if (conf->info.options_template_life_time != NULL) {
			free (conf->info.options_template_life_time);
		}
		if (conf->info.template_life_packet != NULL) {
			free (conf->info.template_life_packet);
		}
		if (conf->info.options_template_life_packet != NULL) {
			free (conf->info.options_template_life_packet);
		}
		free(conf);

	}

	return retval;
}

/**
 * \brief Convers static arrays from host to network byte order
 *
 * Also sets "modified" flag
 */
static inline void modify() {
	modified = 1;
	int i;
	for (i = 0; i < NETFLOW_V5_TEMPLATE_LEN/2; i++) {
		netflow_v5_template[i] = htons(netflow_v5_template[i]);
	}
	netflow_v5_data_header[0] = htons(netflow_v5_data_header[0]);
	netflow_v5_data_header[1] = htons(netflow_v5_data_header[1]);
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
inline uint16_t insertTemplateSet(char **packet, char *input_info, int numOfFlowSamples, ssize_t *len) {
	/* Template Set insertion if needed */
	/* Check conf->info_list->info.template_life_packet and template_life_time */
	struct ipfix_header *header = (struct ipfix_header *) *packet;
#ifdef SCTP_INPUT_PLUGIN
	uint16_t buff_len = IPFIX_MESSAGE_TOTAL_LENGTH;
#else
	struct input_info_list *info_list = (struct input_info_list *) input_info;
	uint16_t buff_len = BUFF_LEN;
#endif

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
#ifdef UDP_INPUT_PLUGIN
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
#else
	if (inserted == 0) {
		inserted = 1;
#endif

		memmove(*packet + IPFIX_HEADER_LENGTH + NETFLOW_V5_TEMPLATE_LEN, *packet + IPFIX_HEADER_LENGTH, buff_len - NETFLOW_V5_TEMPLATE_LEN - IPFIX_HEADER_LENGTH);
		memcpy(*packet + IPFIX_HEADER_LENGTH, netflow_v5_template, NETFLOW_V5_TEMPLATE_LEN);
		*len += NETFLOW_V5_TEMPLATE_LEN;

		return htons(IPFIX_HEADER_LENGTH + NETFLOW_V5_TEMPLATE_LEN + (NETFLOW_V5_DATA_SET_LEN * numOfFlowSamples));
	} else {
		return htons(IPFIX_HEADER_LENGTH + (NETFLOW_V5_DATA_SET_LEN * numOfFlowSamples));
	}
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
void convert_packet(char **packet, ssize_t *len, char *input_info) {
	struct ipfix_header *header = (struct ipfix_header *) *packet;
#ifdef SCTP_INPUT_PLUGIN
	struct input_info_node *info_list = (struct input_info_node *) input_info;
	uint16_t buff_len = IPFIX_MESSAGE_TOTAL_LENGTH;
#else
	struct input_info_list *info_list = (struct input_info_list *) input_info;
	uint16_t buff_len = BUFF_LEN;
#endif
	int numOfFlowSamples = 0;
	switch (htons(header->version)) {
		/* Netflow v9 packet */
		case NETFLOW_V9_VERSION:
			memmove(*packet + BYTES_4, *packet + BYTES_8, buff_len - BYTES_8);
			memset(*packet + buff_len - BYTES_8, 0, BYTES_4);
			*len -= BYTES_4;
			header->length = htons(IPFIX_HEADER_LENGTH);
			uint8_t *p = (uint8_t *) (*packet + IPFIX_HEADER_LENGTH);
			struct ipfix_set_header *set_header;
			while (p < (uint8_t*) *packet + *len) {
				set_header = (struct ipfix_set_header*) p;

				/* check if recieved packet is big enought */
				header->length = htons(ntohs(header->length)+ntohs(set_header->length));
				if (ntohs(header->length) > *len) {
					/* Real length of packet is smaller than it should be */
					MSG_DEBUG(msg_module, "Incomplete packet received");
					return;
				}

				switch (ntohs(set_header->flowset_id)) {
					case NETFLOW_V9_TEMPLATE_SET_ID:
						set_header->flowset_id = htons(IPFIX_TEMPLATE_FLOWSET_ID);
						break;
					case NETFLOW_V9_OPT_TEMPLATE_SET_ID:
						set_header->flowset_id = htons(IPFIX_OPTION_FLOWSET_ID);
						break;
					default:
						break;
				}
				if (ntohs(set_header->length) == 0) {
					break;
				}
				p += ntohs(set_header->length);
			}

			break;

		/* Netflow v5 packet */
		case NETFLOW_V5_VERSION:
			if (modified == 0) {
				modify();
			}
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
			header->length = insertTemplateSet(packet,(char *) info_list, numOfFlowSamples, len);

			header->sequence_number = htonl(seqNo[NF5_SEQ_N]);
			if (*len >= htons(header->length)) {
				seqNo[NF5_SEQ_N] += numOfFlowSamples;
			}

			break;

		/* SFLOW packet (converted to Netflow v5 like packet */
		default:
			if (modified == 0) {
				modify();
			}
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
			header->length = insertTemplateSet(packet,(char *) info_list, numOfFlowSamples, len);

			header->sequence_number = htonl(seqNo[SF_SEQ_N]);
			if (*len >= htons(header->length)) {
				seqNo[SF_SEQ_N] += numOfFlowSamples;
			}
			break;
	}
	header->version = htons(IPFIX_VERSION);
}

/**
 * \brief Pass input data from the input plugin into the ipfixcol core.
 *
 * IP addresses are passed as returned by recvfrom and getsockname,
 * ports are in host byte order
 *
 * \param[in] config  plugin_conf structure
 * \param[out] info   Information structure describing the source of the data.
 * \param[out] packet Flow information data in the form of IPFIX packet.
 * \return the length of packet on success, INPUT_CLOSE when some connection
 *  closed, INPUT_ERROR on error.
 */
int get_packet(void *config, struct input_info **info, char **packet)
{
	/* get socket */
	int sock = ((struct plugin_conf*) config)->socket;
	ssize_t length = 0;
	socklen_t addr_length = sizeof(struct sockaddr_in6);
	struct sockaddr_in6 address;
	struct plugin_conf *conf = config;
	struct input_info_list *info_list;

	/* allocate memory for packet, if needed */
	if (*packet == NULL) {
		*packet = malloc(BUFF_LEN*sizeof(char));
	}
	/* receive packet */
	length = recvfrom(sock, *packet, BUFF_LEN, 0, (struct sockaddr*) &address, &addr_length);
	if (length == -1) {
		if (errno == EINTR) {
			return INPUT_INTR;
		}
		MSG_ERROR(msg_module, "Failed to receive packet: %s", strerror(errno));
		return INPUT_ERROR;
	}

	if (length < IPFIX_HEADER_LENGTH) {
		MSG_ERROR(msg_module, "Packet header is incomplete, skipping");
		return INPUT_INTR;
	}

	/* Convert packet from Netflow v5/v9/sflow to IPFIX format */
	if (htons(((struct ipfix_header *)(*packet))->version) != IPFIX_VERSION) {
		convert_packet(packet, &length, (char *) conf->info_list);
	}

	/* Check if lengths are the same */
	if (length < htons(((struct ipfix_header *)*packet)->length)) {
		MSG_DEBUG(msg_module, "length = %d, header->length = %d", length, htons(((struct ipfix_header *)*packet)->length));
		return INPUT_INTR;
	} else if (length > htons(((struct ipfix_header *)*packet)->length)) {
		length = htons(((struct ipfix_header *)*packet)->length);
	}

	/* go through input_info_list */
	for (info_list = conf->info_list; info_list != NULL; info_list = info_list->next) {
		/* ports must match */
		if (info_list->info.src_port == ntohs(((struct sockaddr_in*) &address)->sin_port)) {
			/* compare addresses, dependent on IP protocol version*/
			if (info_list->info.l3_proto == 4) {
				if (info_list->info.src_addr.ipv4.s_addr == ((struct sockaddr_in*) &address)->sin_addr.s_addr) {
					break;
				}
			} else {
				if (info_list->info.src_addr.ipv6.s6_addr32[0] == address.sin6_addr.s6_addr32[0] &&
						info_list->info.src_addr.ipv6.s6_addr32[1] == address.sin6_addr.s6_addr32[1] &&
						info_list->info.src_addr.ipv6.s6_addr32[2] == address.sin6_addr.s6_addr32[2] &&
						info_list->info.src_addr.ipv6.s6_addr32[3] == address.sin6_addr.s6_addr32[3]) {
					break;
				}
			}
		}
	}
	/* check whether we found the input_info */
	if (info_list == NULL) {
		MSG_NOTICE(msg_module, "New UDP exporter connected (unique port and address)");
		/* create new input_info */
		info_list = malloc(sizeof(struct input_info_list));
		memcpy(&info_list->info, &conf->info, sizeof(struct input_info_network));

		/* copy address and port */
		if (address.sin6_family == AF_INET) {
			/* copy src IPv4 address */
			info_list->info.src_addr.ipv4.s_addr =
					((struct sockaddr_in*) &address)->sin_addr.s_addr;

			/* copy port */
			info_list->info.src_port = ntohs(((struct sockaddr_in*)  &address)->sin_port);
		} else {
			/* copy src IPv6 address */
			int i;
			for (i=0; i<4; i++) {
				info_list->info.src_addr.ipv6.s6_addr32[i] = address.sin6_addr.s6_addr32[i];
			}

			/* copy port */
			info_list->info.src_port = ntohs(address.sin6_port);
		}

		/* add to list */
		info_list->next = conf->info_list;
		info_list->last_sent = ((struct ipfix_header *)(*packet))->export_time;
		info_list->packets_sent = 1;
		conf->info_list = info_list;
	}
	/* pass info to the collector */
	*info = (struct input_info*) info_list;

	return length;
}

/**
 * \brief Input plugin "destructor".
 *
 * \param[in,out] config  plugin_info structure
 * \return 0 on success and config is changed to NULL, nonzero else.
 */
int input_close(void **config)
{
	int ret;
	struct plugin_conf *conf = (struct plugin_conf*) *config;
	struct input_info_list *info_list;

	/* close socket */
	int sock = ((struct plugin_conf*) *config)->socket;
	if ((ret = close(sock)) == -1) {
		MSG_ERROR(msg_module, "Cannot close socket: %s", strerror(errno));
	}

	/* free input_info list */
	while (conf->info_list) {
		info_list = conf->info_list->next;
		free(conf->info_list);
		conf->info_list = info_list;
	}

	/* free configuration strings */
	if (conf->info.template_life_time != NULL) {
		free(conf->info.template_life_time);
	}
	if (conf->info.template_life_packet != NULL) {
		free(conf->info.template_life_packet);
	}
	if (conf->info.options_template_life_time != NULL) {
		free(conf->info.options_template_life_time);
	}
	if (conf->info.options_template_life_packet != NULL) {
		free(conf->info.options_template_life_packet);
	}

	/* free allocated structures */
	free(*config);

	MSG_NOTICE(msg_module, "All allocated resources have been freed");

	return 0;
}
/**@}*/
