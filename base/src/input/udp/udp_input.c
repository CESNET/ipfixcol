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
#include "convert.h"

/* input buffer length */
#define BUFF_LEN 10000
/* default port for udp collector */
#define DEFAULT_PORT "4739"

/** Identifier to MSG_* macros */
static char *msg_module = "UDP input";

/** UDP input plugin identification for packet conversion from netflow to ipfix format */
#define UDP_INPUT_PLUGIN

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
	struct input_info_network info; /**< infromation structure passed to collector */
	struct input_info_list *info_list; /**< list of infromation structures passed to collector */
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

	/* parse params */
	xmlDoc *doc = NULL;
	xmlNode *root_element = NULL;
	xmlNode *cur_node = NULL;

	/* allocate plugin_conf structure */
	conf = calloc(1, sizeof(struct plugin_conf));
	if (conf == NULL) {
		MSG_ERROR(msg_module, "Cannot allocate memory for config structure: %s", strerror(errno));
		retval = 1;
		goto out;
	}

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
			int tmp_val_len = strlen((char *) cur_node->children->content) + 1;
			char *tmp_val = malloc(sizeof(char) * tmp_val_len);
			if (tmp_val == NULL) {
				MSG_ERROR(msg_module, "Cannot allocate memory: %s", strerror(errno));
				retval = 1;
				goto out;
			}

			/* this is not a preferred cast, but we really want to use plain chars here */
			strncpy_safe(tmp_val, (char *) cur_node->children->content, tmp_val_len);

			if (xmlStrEqual(cur_node->name, BAD_CAST "localPort")) { /* set local port */
				if (port) {
					free(port);
				}
				port = tmp_val;
			} else if (xmlStrEqual(cur_node->name, BAD_CAST "localIPAddress")) { /* set local address */
				if (address) {
					free(address);
				}
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
	/* Retry with IPv4 when the implementation does not support the specified address family. */
	if (conf->socket == -1 && errno == EAFNOSUPPORT && addrinfo->ai_family == AF_INET6) {
		addrinfo->ai_family = AF_INET;
		conf->socket = socket(addrinfo->ai_family, addrinfo->ai_socktype, addrinfo->ai_protocol);
	}
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

	if (convert_init(UDP_PLUGIN, BUFF_LEN) != 0) {
		MSG_ERROR(msg_module, "Error when initializing templates!");
		retval = 1;
		goto out;
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
 * \brief Pass input data from the input plugin into the ipfixcol core.
 *
 * IP addresses are passed as returned by recvfrom and getsockname,
 * ports are in host byte order
 *
 * \param[in] config  plugin_conf structure
 * \param[out] info   Information structure describing the source of the data.
 * \param[out] packet Flow information data in the form of IPFIX packet.
 * \param[out] source_status Status of source (new, opened, closed)
 * \return the length of packet on success, INPUT_CLOSE when some connection
 *  closed, INPUT_ERROR on error.
 */
int get_packet(void *config, struct input_info **info, char **packet, int *source_status)
{
	/* get socket */
	int sock = ((struct plugin_conf*) config)->socket;
	ssize_t length = 0;
	socklen_t addr_length = sizeof(struct sockaddr_in6);
	struct sockaddr_in6 address;
	struct plugin_conf *conf = config;
	struct input_info_list *info_list;

	/* allocate memory for packet, if needed */
	if (!*packet) {
		*packet = malloc(BUFF_LEN * sizeof(char));
		if (!*packet) {
			MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)", __FILE__, __LINE__);
		}
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
		MSG_ERROR(msg_module, "Packet header is incomplete; skipping message...");
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
		info_list = calloc(1, sizeof(struct input_info_list));
		memcpy(&info_list->info, &conf->info, sizeof(struct input_info_network));

		info_list->info.status = SOURCE_STATUS_NEW;
		info_list->info.odid = ntohl(((struct ipfix_header *) *packet)->observation_domain_id);

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
	} else {
		info_list->info.status = SOURCE_STATUS_OPENED;
	}

	/* Set source status */
	*source_status = info_list->info.status;

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
	convert_close();

	MSG_NOTICE(msg_module, "All allocated resources have been freed");

	return 0;
}
/**@}*/
