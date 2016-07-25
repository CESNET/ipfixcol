/**
 * \file udp_input.c
 * \author Petr Velan <petr.velan@cesnet.cz>
 * \author Jan Wrona <wrona@cesnet.cz>
 * \brief IPFIX Collector UDP Input Plugin
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

/**
 * \defgroup udpInput UDP input plugin for ipfixcol with support for template replication over CPG
 * \ingroup inputPLugins
 *
 * This is implementation of the input plugin API for UDP network input.
 * Input parameters are passed in xml format
 *
 * @{
 */

#include <inttypes.h>
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

#include <corosync/cpg.h> //closed process group
#include <sys/select.h> //monitor multiple file descriptors

/* API version constant */
IPFIXCOL_API_VERSION;

/* input buffer length */
#define BUFF_LEN 10000

/* default port for udp collector */
#define DEFAULT_PORT "4739"

/** Identifier to MSG_* macros */
static char *msg_module = "UDP-CPG input";

/** UDP input plugin identification for packet conversion from NetFlow to IPFIX */
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
struct plugin_conf {
	int socket; /**< listening socket */
	struct input_info_network info; /**< infromation structure passed to collector */
	struct input_info_list *info_list; /**< list of infromation structures passed to collector */
	cpg_handle_t cpg_handle; /**< CPG handle context */
	struct cpg_name cpg_group_name; /**< CPG group name */
};

/**
 * \struct cpg_context
 * \brief User-defined context for CPG callbacks
 */
struct cpg_context {
	struct sockaddr_in6 *address; /**< either struct sockaddr_in or struct sockaddr_in6 */
	char *packet; /**< packet buffer */
	ssize_t *msg_len; /**< packet length */
	int have_packet; /**< packet received flag */
};


/**
 * \brief CPG data deliver callback
 *
 * Incoming IPFIX packet is filtered for template and option template sets, data
 * sets are omitted. Newly created packet is passed back in CPG user context.
 *
 * \param[in] handle CPG context.
 * \param[in] group_name Group name.
 * \param[in] nodeid Remote node ID.
 * \param[in] pid Remote node process ID.
 * \param[in] msg Message data.
 * \param[in] msg_len Message data length.
 */
static void cpg_deliver_callback(cpg_handle_t handle, const struct cpg_name *group_name, uint32_t nodeid,
		uint32_t pid, void *msg, size_t msg_len)
{
	(void)group_name;
	(void)pid;
	cs_error_t ret;
	unsigned int local_nodeid;
	struct cpg_context *context;
	struct ipfix_header *ipfix_header;
	uint16_t ipfix_len;
	char *p;
	uint16_t packet_len = 0;

	/* Ignore messages sent by local node. */
	cpg_local_get(handle, &local_nodeid);
	if (local_nodeid == nodeid) {
		MSG_DEBUG(msg_module, "CPG ignoring local node message");
		return;
	}

	MSG_INFO(msg_module, "CPG remote node message received (%zu bytes)", msg_len);

	/* Get user context. */
	ret = cpg_context_get(handle, (void **)&context);
	if (ret != CS_OK) {
		MSG_WARNING(msg_module, "CPG context get failed");
		return;
	}

	/* Copy sockaddr from the begining of the buffer. */
	memcpy(context->address, msg, sizeof (*context->address));
	msg += sizeof (*context->address);
	msg_len -= sizeof (*context->address);

	/* Copy IPFIX header. */
	memcpy(context->packet, msg, IPFIX_HEADER_LENGTH);
	packet_len += IPFIX_HEADER_LENGTH;

	p = msg + IPFIX_HEADER_LENGTH;
	ipfix_len = ntohs(((struct ipfix_header *)msg)->length);
	/* Loop through all the sets. */
	while (p < (char *)msg + ipfix_len) {
		struct ipfix_set_header *set_header = (struct ipfix_set_header *)p;
		uint16_t set_header_len = ntohs(set_header->length);
		uint16_t flowset_id = ntohs(set_header->flowset_id);

		/* Copy templates and options, skip data. */
		if (flowset_id == IPFIX_TEMPLATE_FLOWSET_ID || flowset_id == IPFIX_OPTION_FLOWSET_ID) {
			memcpy(context->packet + packet_len, p, set_header_len);
			packet_len += set_header_len;
		}

		p += set_header_len;
	}

	ipfix_header = (struct ipfix_header *)context->packet;
	ipfix_header->length = htons(packet_len); //correct length in IPFIX header
	*context->msg_len = packet_len;
	context->have_packet = 1;
}

/**
 * \brief Check IPFIX message for template or option template set
 *
 * If there is at least one template or option template set, return 1, otherwise
 * return 0.
 *
 * \param[in] packet IPFIX packet data.
 * \return 0 when no template or option template set in packet, 1 otherwise
 */
static int cpg_have_template_or_option(char *packet)
{
	struct ipfix_header *ipfix_header = (struct ipfix_header *)packet;
	uint16_t ipfix_len = ntohs(ipfix_header->length);
	char *p = packet + IPFIX_HEADER_LENGTH;

	/* Loop through all the sets. */
	while (p < packet + ipfix_len) {
		struct ipfix_set_header *set_header = (struct ipfix_set_header *)p;
		uint16_t set_header_length = ntohs(set_header->length);
		uint16_t flowset_id = ntohs(set_header->flowset_id);

		if (flowset_id == IPFIX_TEMPLATE_FLOWSET_ID || flowset_id == IPFIX_OPTION_FLOWSET_ID) {
			return 1; //packet with at least one template or option
		}

		p += set_header_length;
	}

	return 0; //no template or option in packet
}

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
	struct addrinfo *addrinfo = NULL, hints;
	struct plugin_conf *conf = NULL;
	char *port = NULL, *address = NULL;
	int ai_family = AF_INET6; /* IPv6 is default */
	char dst_addr[INET6_ADDRSTRLEN];
	int ret, ipv6_only = 0, retval = 0;

	/* 1 when using default port - don't free memory */
	int default_port = 0;

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
		MSG_ERROR(msg_module, "Cannot parse configuration");
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
	if (!xmlStrEqual(root_element->name, BAD_CAST "udp-cpgCollector")) {
		MSG_ERROR(msg_module, "Expecting udp-cpgCollector root element, got %s", root_element->name);
		retval = 1;
		goto out;
	}

	/* go over all elements */
	for (cur_node = root_element->children; cur_node; cur_node = cur_node->next) {
		if (cur_node->type == XML_ELEMENT_NODE
				&& cur_node->children != NULL
				&& cur_node->children->content != NULL) {
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
				if (conf->info.template_life_time) {
					free(conf->info.template_life_time);
				}
				conf->info.template_life_time = tmp_val;
			} else if (xmlStrEqual(cur_node->name, BAD_CAST "optionsTemplateLifeTime")) {
				if (conf->info.options_template_life_time) {
					free(conf->info.options_template_life_time);
				}
				conf->info.options_template_life_time = tmp_val;
			} else if (xmlStrEqual(cur_node->name, BAD_CAST "templateLifePacket")) {
				if (conf->info.template_life_packet) {
					free(conf->info.template_life_packet);
				}
				conf->info.template_life_packet = tmp_val;
			} else if (xmlStrEqual(cur_node->name, BAD_CAST "optionsTemplateLifePacket")) {
				if (conf->info.options_template_life_packet) {
					free(conf->info.options_template_life_packet);
				}
				conf->info.options_template_life_packet = tmp_val;
			} else if (xmlStrEqual(cur_node->name, BAD_CAST "CPGName")) {
				strncpy(conf->cpg_group_name.value, tmp_val, CPG_MAX_NAME_LENGTH - 1);
				conf->cpg_group_name.length = strlen(conf->cpg_group_name.value);
			} else { /* unknown parameter, ignore */
				free(tmp_val);
			}
		}
	}

	/* set default port if none given */
	if (port == NULL) {
		port = DEFAULT_PORT;
		default_port = 1;
	}

	/* specify parameters of the connection */
	memset(&hints, 0, sizeof(struct addrinfo));
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

	/* Retry with IPv4 when the implementation does not support the specified address family */
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
		MSG_WARNING(msg_module, "Cannot turn off socket option IPV6_V6ONLY; plugin may not accept IPv4 connections...");
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

		/* copy destination IPv6 address */
		int i;
		for (i = 0; i < 4; i++) {
			conf->info.dst_addr.ipv6.s6_addr32[i] =
				((struct sockaddr_in6*) addrinfo->ai_addr)->sin6_addr.s6_addr32[i];
		}

		inet_ntop(AF_INET6, &conf->info.dst_addr.ipv6, dst_addr, INET6_ADDRSTRLEN);
	}

	if (convert_init(UDP_PLUGIN, BUFF_LEN) != 0) {
		MSG_ERROR(msg_module, "Failed to initialize templates");
		retval = 1;
		goto out;
	}

	/* print info */
	MSG_INFO(msg_module, "Input plugin listening on %s, port %s", dst_addr, port);

	/* and pass it to the collector */
	*config = (void*) conf;

	if (conf->cpg_group_name.length > 0) {
		/* Initialize if CPGName was defined in configuration XML. */
		cs_error_t cpg_ret;
		cpg_model_v1_data_t cpg_model_data = { CPG_MODEL_V1, cpg_deliver_callback };

		cpg_ret = cpg_model_initialize(&conf->cpg_handle, CPG_MODEL_V1, (cpg_model_data_t *)&cpg_model_data, NULL);
		if (cpg_ret != CS_OK) {
			MSG_ERROR(msg_module, "CPG model initialization failed");
			retval = 1;
			goto out;
		}

		/* Join group. */
		cpg_ret = cpg_join(conf->cpg_handle, &conf->cpg_group_name);
		if (cpg_ret != CS_OK) {
			MSG_ERROR(msg_module, "CPG join failed");
			retval = 1;
			goto out;
		}

		MSG_INFO(msg_module, "CPG joined \"%s\"", conf->cpg_group_name.value);
	} else {
		/* User didn't specify CPGName, no group will be joined. */
		MSG_INFO(msg_module, "No CPG joined");
	}

	/* normal exit, all OK */
	MSG_INFO(msg_module, "Plugin initialization completed successfully");

out:
	if (default_port == 0 && port != NULL) { /* free when memory was actually allocated */
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
	ssize_t len = 0;
	uint16_t max_msg_len = BUFF_LEN * sizeof(char);
	socklen_t addr_len = sizeof(struct sockaddr_in6);
	struct sockaddr_in6 address;
	struct plugin_conf *conf = config;
	struct input_info_list *info_list;

	/* allocate memory for packet, if needed */
	if (!*packet) {
		*packet = malloc(max_msg_len);
		if (!*packet) {
			MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)", __FILE__, __LINE__);
			return INPUT_ERROR;
		}
	}

	cs_error_t cpg_ret;
	int retval;
	int cpg_fd = 0; //this is not stdin
	fd_set readfds;
	struct cpg_context cpg_context = {&address, *packet, &len, 0 };

	if (conf->cpg_group_name.length > 0) {
		/* Set user context. */
		cpg_ret = cpg_context_set(conf->cpg_handle, &cpg_context);
		if (cpg_ret != CS_OK) {
			MSG_ERROR(msg_module, "CPG context set failed");
			return INPUT_ERROR;
		}

		/* Get file descriptor for polling. */
		cpg_ret = cpg_fd_get(conf->cpg_handle, &cpg_fd);
		if (cpg_ret != CS_OK || cpg_fd == 0) {
			MSG_ERROR(msg_module, "CPG get fd failed");
			return INPUT_ERROR;
		}
	}

	/* cpg_dispatch() may return without actual packet data. */
	while (!cpg_context.have_packet) {
		FD_ZERO(&readfds); //clear FD set
		if (cpg_fd) {
			FD_SET(cpg_fd, &readfds); //add CPG FD if it exists
		}
		FD_SET(sock, &readfds); //add UDP socket

		/* Watch indefinitely for data to become available for reading in one of FDs. */
		retval = select(cpg_fd > sock ? cpg_fd + 1 : sock + 1, &readfds, NULL, NULL, NULL);
		if (retval <= 0) {
			if (errno == EINTR) { //signal interruption
				return INPUT_INTR;
			}

			MSG_ERROR(msg_module, "select(): %s", strerror(errno));
			return INPUT_ERROR;
		}

		if (cpg_fd && FD_ISSET(cpg_fd, &readfds)) { //CPG FD exists and data available on CPG FD
			cpg_ret = cpg_dispatch(conf->cpg_handle, CS_DISPATCH_ALL);
			if (cpg_ret != CS_OK) {
				MSG_WARNING(msg_module, "CPG dispatch failed");
			}
		} else if (FD_ISSET(sock, &readfds)) { //data available on UDP socket
			cpg_context.have_packet = 1;

			/* receive packet */
			len = recvfrom(sock, *packet, BUFF_LEN, 0, (struct sockaddr*) &address, &addr_len);
			if (len == -1) {
				if (errno == EINTR) {
					return INPUT_INTR;
				}

				MSG_ERROR(msg_module, "Failed to receive packet: %s", strerror(errno));
				return INPUT_ERROR;
			}

			if (len < IPFIX_HEADER_LENGTH) {
				MSG_WARNING(msg_module, "Packet header is incomplete; skipping message...");
				return INPUT_INTR;
			}

			/* Try to convert packet from Netflow v5/v9/sflow to IPFIX */
			if (htons(((struct ipfix_header *) (*packet))->version) != IPFIX_VERSION) {
				if (convert_packet(packet, &len, max_msg_len, (char *) conf->info_list) != 0) {
					MSG_WARNING(msg_module, "Message conversion error; skipping message...");
					return INPUT_INTR;
				}
			}

			/* Check if lengths are the same */
			if (len < htons(((struct ipfix_header *) *packet)->length)) {
				return INPUT_INTR;
			} else if (len > htons(((struct ipfix_header *) *packet)->length)) {
				len = htons(((struct ipfix_header *) *packet)->length);
			}

			/* Check if there are any template or option template sets.
			 * If there is at least one, send packet and sockaddr to all groups.
			 * Data sets are ommited on receiving side.
			 */
			if (conf->cpg_group_name.length > 0 && cpg_have_template_or_option(*packet)) {
				struct iovec iovec[2];

				iovec[0].iov_base = &address;
				iovec[0].iov_len = sizeof (address);
				iovec[1].iov_base = *packet;
				iovec[1].iov_len = len;

				cpg_ret = cpg_mcast_joined(conf->cpg_handle, CPG_TYPE_AGREED, iovec, 2);
				if (cpg_ret != CS_OK) {
					MSG_WARNING(msg_module, "CPG mcast failed");
				}

				MSG_INFO(msg_module, "CPG message sent (%zu bytes)", iovec[0].iov_len + iovec[1].iov_len);
			}
		}
	}

	/* Loop over input_info_list */
	for (info_list = conf->info_list; info_list != NULL; info_list = info_list->next) {
		/* Ports must match */
		if (info_list->info.src_port == ntohs(((struct sockaddr_in*) &address)->sin_port)) {
			/* ODIDs must match */
			if (info_list->info.odid == ntohl(((struct ipfix_header *) *packet)->observation_domain_id)) {
				/* Compare addresses, dependent on IP protocol version*/
				if (info_list->info.l3_proto == 4) {
					if (info_list->info.src_addr.ipv4.s_addr == ((struct sockaddr_in*) &address)->sin_addr.s_addr) {
						break;
					}
				} else {
					if (info_list->info.src_addr.ipv6.s6_addr32[0] == address.sin6_addr.s6_addr32[0]
							&& info_list->info.src_addr.ipv6.s6_addr32[1] == address.sin6_addr.s6_addr32[1]
							&& info_list->info.src_addr.ipv6.s6_addr32[2] == address.sin6_addr.s6_addr32[2]
							&& info_list->info.src_addr.ipv6.s6_addr32[3] == address.sin6_addr.s6_addr32[3]) {
						break;
					}
				}
			}
		}
	}

	/* check whether we found the input_info */
	if (info_list == NULL) {
		MSG_INFO(msg_module, "New UDP exporter connected (unique adress, port, ODID)");

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
			for (i = 0; i < 4; i++) {
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

	return len;
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

	if (conf->cpg_group_name.length > 0) {
		cs_error_t cpg_ret;

		/* Leave group. */
		cpg_ret = cpg_leave(conf->cpg_handle, &conf->cpg_group_name);
		if (cpg_ret != CS_OK) {
			MSG_ERROR(msg_module, "CPG leave group failed");
		}

		MSG_INFO(msg_module, "CPG left group \"%s\"", conf->cpg_group_name.value);

		/* Finalize. */
		cpg_ret = cpg_finalize(conf->cpg_handle);
		if (cpg_ret != CS_OK) {
			MSG_ERROR(msg_module, "CPG finalize failed");
		}
	}

	/* free allocated structures */
	free(*config);
	convert_close();

	MSG_INFO(msg_module, "All allocated resources have been freed");

	return 0;
}
/**@}*/
