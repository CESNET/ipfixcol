/**
 * \file sctp_input.c
 * \author Michal Srb <michal.srb@cesnet.cz>
 * \brief SCTP input plugin for ipfixcol.
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
 * \defgroup sctpInputPlugin SCTP input plugin
 * \ingroup inputPlugins
 *
 * Implementation of SCTP input plugin for ipfixcol.
 *
 * @{
 */


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/sctp.h>
#include <sys/epoll.h>
#include <pthread.h>
#include <errno.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <sys/ioctl.h>

#include "ipfixcol.h"
#include "sflow.h"
#include "sflowtool.h"

#define DEFAULT_LISTEN_PORT_UNSECURE 4739
#define DEFAULT_LISTEN_PORT_SECURE   4740   /* listen port when used with DTLS
                                               (so never :)) */

/* maximum input/output streams per association */
#define INSTREAMS_PER_SOCKET         20
#define OSTREAMS_PER_SOCKET          20

#define DEFAULT_IPV6_LISTEN_ADDRESS  in6addr_any

#define MAX_EPOLL_EVENTS             1

#define LISTEN_BACKLOG               50

#define IPFIX_MESSAGE_TOTAL_LENGTH   65535

/* just guess, user will want to bind at most 20 addresses (per address family)
 * to listen socket. if this number is not enough the corresponding array 
 * will be reallocated */
#define DEFAULT_NUMBER_OF_ADDRESSES 20

/** Identifier to MSG_* macros */
static char *msg_module = "sctp input";


/** SCTP input plugin identification for packet conversion from netflow to ipfix format */
#define SCTP_INPUT_PLUGIN

/** Netflow v5 and v9 identifiers */
#define SET_HEADER_LEN 4

#define NETFLOW_V5_VERSION 5
#define NETFLOW_V9_VERSION 9

#define NETFLOW_V5_TEMPLATE_LEN 88
#define NETFLOW_V5_DATA_SET_LEN 48
#define NETFLOW_V5_NUM_OF_FIELDS 20

#define NETFLOW_V9_TEMPLATE_SET_ID 0
#define NETFLOW_V9_OPT_TEMPLATE_SET_ID 1

/** IPFIX Element IDs used when creating Template Set */
#define SRC_IPV4_ADDR 8
#define DST_IPV4_ADDR 12
#define NEXTHOP_IPV4_ADDR 15
#define INGRESS_INTERFACE 10
#define EGRESS_INTERFACE 14
#define PACKETS 2
#define OCTETS 1
#define FLOW_START 22
#define FLOW_END 21
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
		FLOW_START, 				 BYTES_4,\
		FLOW_END, 					 BYTES_4,\
		SRC_PORT, 					 BYTES_2,\
		DST_PORT, 					 BYTES_2,\
		PADDING, 					 BYTES_1,\
		TCP_FLAGS, 					 BYTES_1,\
		PROTO, 						 BYTES_1,\
		TOS, 						 BYTES_1,\
		SRC_AS, 					 BYTES_2,\
		DST_AS, 					 BYTES_2,\
		PADDING, 					 BYTES_1,\
		PADDING, 					 BYTES_1,\
		PADDING, 					 BYTES_2
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
 * \struct input_info_node
 * \brief node of a linked list of input_info structures
 */
struct input_info_node {
	struct input_info_network info;
	int socket;
	struct input_info_node *next;
	uint32_t last_sent;
	uint16_t packets_sent;
};

/**
 * \struct sctp_config
 * \brief plugin configuration structure
 */
struct sctp_config {
	uint16_t listen_port;                    /**< listen port (host byte 
	                                          * order) */
	uint16_t listen_socket;                  /**< listen socket */
	int epollfd;                             /**< epoll file descriptor */
	struct input_info_node *input_info_list; /**< linked list of input_info
	                                          * structures */
	pthread_mutex_t input_info_list_mutex;   /**< mutex for 
	                                          * 'input_info_list' list */
	pthread_t listen_thread;                 /**< id of the thread that 
	                                          * listens for new associations */
};


/**
 * \brief Listen for incoming associations
 *
 * \param[in] data  plugin config structure
 * \return this function should never return
 */
void *listen_worker(void *data) {
	struct sctp_config *conf;
	int conn_socket;
	socklen_t addrlen;
	struct sockaddr_storage addr;
	struct sockaddr *addr_ptr;
	int ret;
	char printable_ip[INET6_ADDRSTRLEN];
	struct sockaddr_in6 *src_addr6;
	struct input_info_node *node;
	struct input_info_node *temp_node;
	struct epoll_event ev;
	int on = 1;                 /* ioctl() "turn on" flag */
	
	conf = data;

	addrlen = sizeof(addr);
	addr_ptr = (struct sockaddr *) &addr;

	while (1) {
		/* wait for new SCTP association */
		conn_socket = accept(conf->listen_socket, addr_ptr, &addrlen);
		if (conn_socket == -1) {
			if ((errno == ECONNABORTED) || (errno == EINTR) 
			    || (errno == EINVAL)) {
				/* listen socket probably closed, user wants 
				 * to quit ipfixcol */
				pthread_testcancel();
			}

			MSG_ERROR(msg_module, "accept() - %s", strerror(errno));
			continue;
		}

		/* make new socket non-blocking */
		ioctl(conn_socket, FIONBIO, (char *)&on);

		/* input_info - fill out information about input */
		node = (struct input_info_node *) malloc(sizeof(*node));
		if (!node) {
			MSG_ERROR(msg_module, "Not enough memory (%s:%d)",
			                 __FILE__, __LINE__);
			goto err_assoc;
		}

		src_addr6 = (struct sockaddr_in6 *) &addr;

		node->info.type = SOURCE_TYPE_SCTP;
		node->info.l3_proto = AF_INET6;
		/* source address */
		memcpy(&(node->info.src_addr.ipv6), src_addr6, 
		                        sizeof(node->info.src_addr.ipv6));
		node->info.src_port = ntohs(src_addr6->sin6_port);
		node->info.dst_port = ntohs(((struct sockaddr_in6*) addr_ptr)->sin6_port);
		node->socket = conn_socket;


		/* add input_info to the list */
		pthread_mutex_lock(&(conf->input_info_list_mutex));
		node->next = conf->input_info_list;
		node->last_sent = 0;
		node->packets_sent = 0;
		conf->input_info_list = node;
		pthread_mutex_unlock(&(conf->input_info_list_mutex));

		/* add new association to the event poll */
		memset(&ev, 0, sizeof(ev));
		ev.events = EPOLLIN;
		ev.data.fd = conn_socket;
		ret = epoll_ctl(conf->epollfd, EPOLL_CTL_ADD, conn_socket, &ev);
		if (ret == -1) {
			MSG_ERROR(msg_module, "epoll_ctl() error");
			goto err_assoc;
		}

		inet_ntop(AF_INET6, &(src_addr6->sin6_addr), printable_ip, 
		                             INET6_ADDRSTRLEN);
		MSG_NOTICE(msg_module, "New SCTP association from %s",
		                             printable_ip);

		continue;

		/* error occurs, handle it and continue listening */
err_assoc:
		close(conn_socket);

		/* remove input_info from the list */
		pthread_mutex_lock(&(conf->input_info_list_mutex));

		temp_node = conf->input_info_list->next;
		free(conf->input_info_list);
		conf->input_info_list = temp_node;

		pthread_mutex_unlock(&(conf->input_info_list_mutex));

		continue;
	}
}


/**
 * \brief Plugin initialization
 *
 * \param[in] params  XML based configuration for the plugin
 * \param[out] config  plugin config structure
 * \return 0 on success, negative value otherwise
 */
int input_init(char *params, void **config)
{
	struct sctp_config *conf = NULL;
	xmlDocPtr doc;
	xmlNodePtr cur;
	xmlChar *listen_port_str = NULL;
	struct sctp_initmsg initmsg;
	struct sctp_event_subscribe sctp_events;
	int epollfd;
	int ret;
	char *ip_str = NULL;
	struct sockaddr_in *sockaddr = NULL;
	struct sockaddr_in6 *sockaddr6 = NULL;
	struct sockaddr_in **sockaddr_listen_old = NULL;
	int ip_family;
	char *dot = NULL;
	uint8_t port_set = 0;
	int i;

	struct sockaddr_in6 **sockaddr6_listen;  /* list of sockaddr6 structures
	                                          * for use with sctp_bindx() */
	uint8_t sockaddr6_listen_counter = 0;    /* counter of the sockaddr6 
	                                          * structures */
	uint8_t sockaddr6_listen_max;            /* maximum sockaddr6 structures
	                                          * in array */

	struct sockaddr_in **sockaddr_listen;    /* list of sockaddr structures
	                                          * for use with sctp_bindx() */
	uint8_t sockaddr_listen_counter = 0;     /* counter of the sockaddr 
	                                          * structures */
	uint8_t sockaddr_listen_max;             /* maximum sockaddr structures 
	                                          * in array */


	if (params == NULL) {
		MSG_ERROR(msg_module, "SCTP input plugin: No configuration data");
		return -1;
	}

	/* allocate memory for config structure */
	conf = (struct sctp_config *) malloc(sizeof(*conf));
	if (!conf) {
		MSG_ERROR(msg_module, "Not enough memory (%s:%d)",
		                         __FILE__, __LINE__);
		return -1;
	}
	memset(conf, 0, sizeof(*conf));

	/* array for IPv6 listen addresses. this array will later be used with 
	 * sctp_bindx() (e.i. multi-homing support) */
	sockaddr6_listen = (struct sockaddr_in6 **) 
	      malloc(DEFAULT_NUMBER_OF_ADDRESSES * sizeof(*(sockaddr6_listen)));
	if (sockaddr6_listen == NULL) {
		MSG_ERROR(msg_module, "Not enough memory (%s:%d)",
		                        __FILE__, __LINE__);
		goto err_sockaddr6;
	}
	memset(sockaddr6_listen, 0, 
	     sizeof(DEFAULT_NUMBER_OF_ADDRESSES * sizeof(*(sockaddr6_listen))));
	sockaddr6_listen_max = DEFAULT_NUMBER_OF_ADDRESSES;

	/* array for IPv4 listen addresses. this array will later be used with 
	 * sctp_bindx() (e.i. multi-homing support) */
	sockaddr_listen = (struct sockaddr_in **) 
	       malloc(DEFAULT_NUMBER_OF_ADDRESSES * sizeof(*(sockaddr_listen)));
	if (sockaddr_listen == NULL) {
		MSG_ERROR(msg_module, "Not enough memory (%s:%d)",
		                        __FILE__, __LINE__);
		goto err_sockaddr;
	}
	memset(sockaddr_listen, 0, 
	      sizeof(DEFAULT_NUMBER_OF_ADDRESSES * sizeof(*(sockaddr_listen))));
	sockaddr_listen_max = DEFAULT_NUMBER_OF_ADDRESSES;

	/* try to parse XML configuration */
	doc = xmlReadMemory(params, strlen(params), "nobase.xml", NULL, 0);
	if (doc == NULL) {
		MSG_ERROR(msg_module, "SCTP input plugin configuration not "
		                        "parsed successfully");
		goto err_xml;
	}

	cur = xmlDocGetRootElement(doc);
	if (cur == NULL) {
		MSG_ERROR(msg_module, "Empty configuration");
		goto err_xml;
	}

	if (xmlStrcmp(cur->name, (const xmlChar *) "sctpCollector")) {
		MSG_ERROR(msg_module, "SCTP input plugin: Bad configuration "
		                        "(root node != sctpCollector)");
		goto err_xml;
	}

	/* here we are, this is the place where actual plugin configuration 
	 * starts */
	cur = cur->xmlChildrenNode;

	while (cur != NULL) {
		if ((!xmlStrcmp(cur->name, (const xmlChar *) "localIPAddress"))) {

			/* user wants to listen on this IP address */
			ip_str = (char *) xmlNodeListGetString(doc, cur->children, 1);

			if (ip_str == NULL) {
				cur = cur->next;
				continue;
			}
			
			/* try to determine address family */
			dot = strchr(ip_str, '.');
			ip_family = (dot) ? AF_INET : AF_INET6;

			sockaddr = NULL;
			sockaddr6 = NULL;
			/* prepare new sockaddr structure for further processing
			 * with sctp_bindx() */
			switch (ip_family) {

			/* given address is IPv6 address */
			case (AF_INET):
				/* add new IPv4 address */
				sockaddr = (struct sockaddr_in *) malloc(sizeof(*sockaddr));
				if (!sockaddr) {
					MSG_ERROR(msg_module, "Not enough "
					   "memory (%s:%d)", __FILE__, __LINE__);
					goto err_sockaddr_case;
				}
				memset(sockaddr, 0, sizeof(*sockaddr));

				/* fill address family and IP address */
				sockaddr->sin_family = AF_INET;
				ret = inet_pton(AF_INET, ip_str, &(sockaddr->sin_addr));
				if (ret != 1) {
					/* invalid address */
					MSG_ERROR(msg_module, "SCTP init: "
					  "%s is not valid IP address", ip_str);
					goto err_sockaddr_case;
				}
				/* note we don't know yet what the desired port
				 * is, so we will fill it later */

				if (sockaddr_listen_counter >= sockaddr_listen_max) {
					/* oops, we need to realloc array that 
					 * holds these addresses */
					sockaddr_listen_old =  sockaddr_listen;
					sockaddr_listen = realloc(sockaddr_listen, sockaddr_listen_max * 2);

					if (sockaddr_listen == NULL) {
						/* realloc fails, discard this 
						 * address and continue */
						sockaddr_listen = sockaddr_listen_old;
						MSG_ERROR(msg_module, "Realloc fails (%s:%d)", __FILE__, __LINE__);
						MSG_ERROR(msg_module, "Address %s cannot be added "
						                          "- system error", ip_str);
						goto err_sockaddr_case;
					}

					sockaddr6_listen_max *= 2;
				}

				/* everything is ok, add this new address */
				sockaddr_listen[sockaddr_listen_counter] = sockaddr;
				sockaddr_listen_counter += 1;
				
				MSG_NOTICE(msg_module, "SCTP listen address: %s", ip_str);

				break;

err_sockaddr_case:
				/* error handling */
				if (sockaddr != NULL) {
					free(sockaddr);
				}
				xmlFree(ip_str);
				break;
			/* end of case(AF_INET) */

			/* given address is IPv6 address */
			case (AF_INET6):
				/* add new IPv6 address */
				sockaddr6 = (struct sockaddr_in6 *) malloc(sizeof(*sockaddr6));
				if (!sockaddr6) {
					MSG_ERROR(msg_module, "Not enough memory (%s:%d)", __FILE__, __LINE__);
					goto err_sockaddr6_case;
				}
				memset(sockaddr6, 0, sizeof(*sockaddr6));

				/* fill address family and IP address */
				sockaddr6->sin6_family = AF_INET6;
				ret = inet_pton(AF_INET6, ip_str, &(sockaddr6->sin6_addr));
				if (ret != 1) {
					/* invalid address */
					MSG_ERROR(msg_module, "SCTP init: %s is not valid IP "
					                          "address", ip_str);
					goto err_sockaddr6_case;
				}
				/* note we don't know yet what the desired port
				 * is, so we will fill it later */

				if (sockaddr6_listen_counter >= sockaddr6_listen_max) {
					/* oops, we need to realloc array that 
					 * holds these addresses */
					sockaddr_listen_old = (struct sockaddr_in **) sockaddr6_listen;
					sockaddr6_listen = realloc(sockaddr6_listen, sockaddr6_listen_max * 2);
					
					if (sockaddr6_listen == NULL) {
						/* realloc fails, discard this 
						 * address and continue */
						sockaddr6_listen = (struct sockaddr_in6 **) sockaddr_listen_old;
						MSG_ERROR(msg_module, "Realloc fails (%s:%d)", __FILE__, __LINE__);
						MSG_ERROR(msg_module, "Address %s cannot be added "
						                          "- system error", ip_str);
						goto err_sockaddr6_case;
					}
					
					sockaddr6_listen_max *= 2;
				}

				/* everything is ok, add this new address */
				sockaddr6_listen[sockaddr6_listen_counter] = sockaddr6;
				sockaddr6_listen_counter += 1;
				
				MSG_NOTICE(msg_module, "SCTP listen address: %s", ip_str);

				break;

err_sockaddr6_case:
				/* error handling */
				if (sockaddr6 != NULL) {
					free(sockaddr6);
				}
				xmlFree(ip_str);
				break;
			/* end of case(AF_INET6) */
			
			default:
				/* unknown address family, we should never 
				 * reach this point */
				MSG_ERROR(msg_module, "Unknown address family, this should never happen (%s, %d)", __FILE__, __LINE__);
				break;
			} /* switch */


			xmlFree(ip_str);
		}


		if ((!xmlStrcmp(cur->name, (const xmlChar *) "localPort"))) {
			/* user specified (another) listen port */
			if (port_set != 0) {
				/* damn, port is specified multiple times. this 
				 * might be a bug in configuration */
				MSG_WARNING(msg_module, "SCTP input plugin: Warning, listen port is specified multiple times in configuration file");
			}
			listen_port_str = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);

			conf->listen_port = atoi((char *) listen_port_str);
			MSG_NOTICE(msg_module, "SCTP listen port: %s", (char *) listen_port_str);
			port_set = 1;

			xmlFree(listen_port_str);
		}

		cur = cur->next;
	}


	/* use default address if user doesn't specified any */
	if ((sockaddr6_listen_counter == 0) && (sockaddr_listen_counter == 0)) {
		sockaddr6 = (struct sockaddr_in6 *) calloc(1, sizeof(*sockaddr6));
		if (!sockaddr6) {
			MSG_ERROR(msg_module, "Not enough memory (%s:%d)",
			                        __FILE__, __LINE__);
			goto err;
		}

		sockaddr6->sin6_family = AF_INET6;
		/* listen on any IPv6 address */
		memcpy(&(sockaddr6->sin6_addr), &(DEFAULT_IPV6_LISTEN_ADDRESS), 
		                           sizeof(DEFAULT_IPV6_LISTEN_ADDRESS));
		sockaddr6_listen[0] = sockaddr6;
		sockaddr6_listen_counter += 1;
	}

	/* use default listen port if not specified otherwise */
	if (conf->listen_port == 0) {
		conf->listen_port = DEFAULT_LISTEN_PORT_UNSECURE;
	}

	/* same port for every IPv4 address */
	for (i = 0; i < sockaddr_listen_counter; i++) {
		sockaddr_listen[i]->sin_port = htons(conf->listen_port);
	}

	/* same port for every IPv6 address */
	for (i = 0; i < sockaddr6_listen_counter; i++) {
		sockaddr6_listen[i]->sin6_port = htons(conf->listen_port);
	}


	/* get socket. SOCK_STREAM creates one-to-one style socket. in opposite
	 * SOCK_SEQPACKET would create one-to-many style socket. for our 
	 * purpose, one-to-one is just fine */
	conf->listen_socket = socket(AF_INET6, SOCK_STREAM, IPPROTO_SCTP);
	if (conf->listen_socket == (uint16_t) -1) {
		MSG_ERROR(msg_module, "socket() - %s", strerror(errno));
		goto err;
	}

	/* bind IPv6 listen addresses */
	for (i = 0; i < sockaddr6_listen_counter; i++) {
		ret = sctp_bindx(conf->listen_socket, 
		                 (struct sockaddr *) sockaddr6_listen[i], 1, SCTP_BINDX_ADD_ADDR);
		if (ret == -1) {
			MSG_ERROR(msg_module, "sctp_bindx() - %s", strerror(errno));
		}
	}

	/* bind IPv4 listen addresses */
	for (i = 0; i < sockaddr_listen_counter; i++) {
		ret = sctp_bindx(conf->listen_socket, (struct sockaddr *) sockaddr_listen[i], 1, SCTP_BINDX_ADD_ADDR);
		if (ret == -1) {
			MSG_ERROR(msg_module, "sctp_bindx() - %s", strerror(errno));
		}
	}

	/* configure streams */
	memset(&initmsg, 0, sizeof(initmsg));
	initmsg.sinit_num_ostreams = OSTREAMS_PER_SOCKET;
	initmsg.sinit_max_instreams = INSTREAMS_PER_SOCKET;

	ret = setsockopt(conf->listen_socket, IPPROTO_SCTP, SCTP_INITMSG, &initmsg, sizeof(initmsg));
	if (ret == -1) {
		MSG_ERROR(msg_module, "setsockopt(initmsg) - %s", strerror(errno));
		goto err;
	}

	/* subscribe to some SCTP events */
	memset(&sctp_events, 0, sizeof(sctp_events));
	/* this allows us to determine the stream number on which message arrived */
	sctp_events.sctp_data_io_event = 1; 
	/* now we know, that specific association was closed */
	sctp_events.sctp_shutdown_event = 1; 

	ret = setsockopt(conf->listen_socket, IPPROTO_SCTP, SCTP_EVENTS, &sctp_events, sizeof(sctp_events));
	if (ret == -1) {
		MSG_ERROR(msg_module, "setsockopt(event subscription) - %s", strerror(errno));
		goto err;
	}

	/* enable incoming associations */
	ret = listen(conf->listen_socket, LISTEN_BACKLOG);
	if (ret == -1) {
		MSG_ERROR(msg_module, "listen() - %s", strerror(errno));
		goto err;
	}

	/* set up epoll. parameter is ignored anyway, so it means nothing */
	epollfd = epoll_create(10);
	if (epollfd == -1) {
		MSG_ERROR(msg_module, "epoll_create() - %s", strerror(errno));
		goto err_listen;
	}

	MSG_NOTICE(msg_module, "SCTP input plugin listening for incoming "
	                          "associations");

	conf->epollfd = epollfd;

	/* create listen worker */
	ret = pthread_create(&(conf->listen_thread), NULL, listen_worker, conf);
	if (ret != 0) {
		MSG_ERROR(msg_module, "Unable to create listen_worker thread");
		goto err_listen;
	}

	/* do some cleanup */

	/* we don't need this xml anymore */
	xmlFreeDoc(doc);
	xmlCleanupParser();

	for (i = 0; i < sockaddr_listen_counter; i++) {
		free(sockaddr_listen[i]);
	}
	free(sockaddr_listen);

	for (i = 0; i < sockaddr6_listen_counter; i++) {
		free(sockaddr6_listen[i]);
	}
	free(sockaddr6_listen);


	*config = conf;

	return 0;


/* error handling */
err_listen:
	close(conf->listen_socket);

err:
err_xml:
	free(sockaddr_listen);

err_sockaddr:
	free(sockaddr6_listen);
err_sockaddr6:
	free(conf);

	return -1;
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
	struct input_info_node *info_list = (struct input_info_node *) input_info;
	uint16_t buff_len = IPFIX_MESSAGE_TOTAL_LENGTH;
#else
	struct input_info_list *info_list = (struct input_info_list *) input_info;
	uint16_t buff_len = BUFF_LEN;
#endif
	/* Insert Data Set header */
	netflow_v5_data_header[1] = htons(NETFLOW_V5_DATA_SET_LEN * numOfFlowSamples + SET_HEADER_LEN);
	memmove(*packet + IPFIX_HEADER_LENGTH + BYTES_4, *packet + IPFIX_HEADER_LENGTH, buff_len - IPFIX_HEADER_LENGTH - BYTES_4);
	memcpy(*packet + IPFIX_HEADER_LENGTH, netflow_v5_data_header, BYTES_4);

#ifdef UDP_INPUT_PLUGIN
	uint32_t last = 0;
	if (info_list != NULL) {
		if ((info_list->info.template_life_packet != NULL) && (info_list->info.template_life_time != NULL)) {
			if (info_list->packets_sent == strtol(info_list->info.template_life_packet, NULL, 10)) {
				last = ntohl(header->export_time);
			} else {
				last = info_list->last_sent + strtol(info_list->info.template_life_time, NULL, 10);
				if (numOfFlowSamples > 0) {
					info_list->packets_sent++;
				}
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
			numOfFlowSamples = ntohs(header->length);

			/* Header modification */
			header->export_time = header->sequence_number;

			memmove(*packet + BYTES_8, *packet + IPFIX_HEADER_LENGTH, buff_len - IPFIX_HEADER_LENGTH);
			memmove(*packet + BYTES_12, *packet + BYTES_12 + BYTES_1, BYTES_1);

			header->observation_domain_id = header->observation_domain_id&(0xF000);

			/* Update real packet length because of memmove() */
			*len = *len - BYTES_8;

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
			if ((numOfFlowSamples = Process_sflow(*packet, *len)) < 0) {
				header->length = *len -1;
				return;
			}

			/* Observation domain ID is unknown */
			header->observation_domain_id = 0; // ??

			/* Template Set insertion (if needed) and setting total packet length */
			header->length = insertTemplateSet(packet,(char *) info_list, numOfFlowSamples, len);
			seqNo[SF_SEQ_N] += numOfFlowSamples;
			header->sequence_number = htonl(seqNo[SF_SEQ_N]);
			break;
	}
	header->version = htons(IPFIX_VERSION);
}

/**
 * \brief Receive data from opened associations
 *
 * \param[in] config  plugin config structure
 * \param[out] input_info  information about input
 * \param[out] packet  IPFIX message
 * \return message length on success, error code otherwise
 */
int get_packet(void *config, struct input_info** info, char **packet)
{	
	struct sctp_config *conf;
	int socket;
	struct sctp_sndrcvinfo sinfo;
	int flags;
	int msg_length;
	int nfds;
	uint8_t packet_allocated_here = 0;
	struct epoll_event events[MAX_EPOLL_EVENTS];
	int i = 0;    /* always interrested only in first IO event */
	struct input_info_node *info_node;
	uint16_t notification_type;
	int ret;
	conf = config;


	memset(&events, 0, MAX_EPOLL_EVENTS * sizeof(events));


wait_for_data:
	/* wait for IPFIX messages (note, level triggered epoll) */
	nfds = epoll_wait(conf->epollfd, events, MAX_EPOLL_EVENTS, -1);

	if (nfds == -1) {
		if (errno == EINTR) {
			/* user wants to terminate the collector */
			ret = INPUT_INTR;
			goto out;
		} else {
			/* serious error occurs */
			MSG_ERROR(msg_module, "epoll_wait() - %s", strerror(errno));
			ret = INPUT_ERROR;
			goto out;
		}
	} else if (nfds == 0) {
		MSG_ERROR(msg_module, "epoll_wait() wakes up, but no "
		                  "descriptors are ready- %s", strerror(errno));
		goto wait_for_data;
	}

	/* event on socket */
	socket = events[i].data.fd;

	info_node = conf->input_info_list;

	/* search for corresponding input_info */
	while (info_node) {
		if (info_node->socket == socket) {
			/* got it */
			*info = (struct input_info *) &(info_node->info);
			break;
		}
		info_node = info_node->next;
	}
	if (info_node == NULL) {
		/* no such input_info (!?) */
		MSG_ERROR(msg_module, "Something is horribly wrong. "
		                          "Missing input_info for SCTP "
		                          "association. This should never "
		                          "happen");
		ret = INPUT_ERROR;
		goto out;
	}

	/* allocate memory for message, if needed */
	if (*packet == NULL) {
		/* TODO - we can check how big the message is from its header */
		*packet = (char *) malloc(IPFIX_MESSAGE_TOTAL_LENGTH);
		if (*packet == NULL) {
			MSG_ERROR(msg_module, "Not enough memory (%s:%d)",
			                          __FILE__, __LINE__);
			ret = INPUT_ERROR;
			goto out;
		}
		packet_allocated_here = 1;
	}

	/* new message */
	flags = 0;
	msg_length = sctp_recvmsg(socket, *packet, IPFIX_MESSAGE_TOTAL_LENGTH, 
	                                            NULL, NULL, &sinfo, &flags);
	if (msg_length == -1) {
		MSG_ERROR(msg_module, "sctp_recvmsg()");
		ret = INPUT_ERROR;
		goto out;
	}

	if (msg_length < IPFIX_HEADER_LENGTH) {
		MSG_ERROR(msg_module, "Packet header is incomplete, skipping");
		return INPUT_INTR;
	}

	/* Convert packet from Netflow v5/v9/sflow to IPFIX format */
	if (htons(((struct ipfix_header *)(*packet))->version) != IPFIX_VERSION) {
		convert_packet(packet, (ssize_t *) &msg_length, (char *) conf->input_info_list);
	}

	/* Check if lengths are the same */
	if (msg_length < htons(((struct ipfix_header *)*packet)->length)) {
		MSG_ERROR(msg_module, "Packet is incomplete, skipping");
		return INPUT_INTR;
	} else if (msg_length > htons(((struct ipfix_header *)*packet)->length)) {
		MSG_WARNING(msg_module, "Received more data than packet length, setting right value");
		msg_length = htons(((struct ipfix_header *)*packet)->length);
	}

#if 0
	fprintf(stderr, "********************************************\n");
	fprintf(stderr, "sinfo_ssn: %u\n", sinfo.sinfo_ssn);
	fprintf(stderr, "sinfo_flags: %u\n", sinfo.sinfo_flags);
	fprintf(stderr, "sinfo_ppid: %u\n", sinfo.sinfo_ppid);
	fprintf(stderr, "sinfo_context: %u\n", sinfo.sinfo_context);
	fprintf(stderr, "sinfo_timetolive: %u\n", sinfo.sinfo_timetolive);
	fprintf(stderr, "sinfo_tsn: %u\n", sinfo.sinfo_tsn);
	fprintf(stderr, "sinfo_cumtsn: %u\n", sinfo.sinfo_cumtsn);
	fprintf(stderr, "sinfo_assoc_id: %u\n", sinfo.sinfo_assoc_id);
	fprintf(stderr, "********************************************\n");
#endif

	/* check whether event happened on SCTP stack */
	if (flags & MSG_NOTIFICATION) {
		/* these are not IPFIX data, but event notification from SCTP 
		 * stack */
		notification_type = *((uint16_t *) *packet);   /* damn bugs! */

		if (notification_type == SCTP_SHUTDOWN_EVENT) {
			MSG_NOTICE(msg_module, "SCTP input plugin: Exporter disconnected");

			/* remove disconnected socket from event poll */
			ret = epoll_ctl(conf->epollfd, EPOLL_CTL_DEL, socket, NULL);
			if (ret == -1) {
				/* damn... what can i do */
				MSG_ERROR(msg_module, "epoll_ctl(...,"
				"EPOLL_CTL_DEL,...) error (%s:%d)", __FILE__,
				__LINE__);
			}

			/* no more data from this exporter */
			/* \todo free input info structure now (in near future) */
			return INPUT_CLOSED;
		} else {
			MSG_WARNING(msg_module, "Unsupported SCTP event "
			                          "occured");
			goto wait_for_data;
		}

		/* SCTP event processed */
		/* we can't return yet. user expects some data */
		goto wait_for_data;
	} else if (!(flags & MSG_EOR)) {
		MSG_WARNING(msg_module, "SCTP input plugin: message is too "
		                          "long");
	}


	return msg_length;

/* error handling */
out:
	if (packet_allocated_here && *packet) {
		free(*packet);
		*packet = NULL;
	}

	return ret;
}


/**
 * \brief Plugin destructor
 *
 * \param[in] config  plugin config structure
 * \return 0 on success, negative value otherwise
 */
int input_close(void **config)
{
	struct input_info_node *node;
	struct input_info_node *next_node;
	struct sctp_config *conf;
	int ret;

	conf = *config;
	
	node = conf->input_info_list;

	/* close listen socket */
	close(conf->listen_socket);

	/* cancel listen thread and wait for it */
	pthread_cancel(conf->listen_thread);
	pthread_join(conf->listen_thread, NULL);


	/* close all client sockets and free input_info structures */
	while (node) {
		next_node = node->next;

		/* graceful association shutdown */
		ret = close(node->socket);
		if (ret == -1) {
			MSG_ERROR(msg_module, "Error while closing "
			                          "association");
		}
		
		/* free input_info structure for this association */
		free(node);

		node = next_node;
	}

	free(conf);


	return 0;
}

/**@}*/


#ifdef SCTP_INPUT_PLUGIN_SELF_DEBUG
/* self-DEBUG section. everything here can be safely ignored or deleted */
/* it's here mainly because of valgrind */

static char
*xml_configuration1 = "<sctpCollector>\n"
                         "<name>Listening port 4739</name>\n"
                         "<localPort>100</localPort>\n"
                         "<localPort>4739</localPort>\n"
                         "<localIPAddress>127.0.0.1</localIPAddress>\n"
                         "<localIPAddress>::1</localIPAddress>\n"
                      "</sctpCollector>";

#define P(msg) fprintf(stderr, "DEBUG: %s\n", (msg));

int main(int argc, char **argv)
{
	void *conf = NULL;
	int ret;
	int msg_length;
	char *packet = NULL;
	struct input_info *info = NULL;

	P("input_init()");
	input_init(xml_configuration1, &conf);
	P("input_init() X");

	if (!conf) {
		P("config is NULL");
		exit(EXIT_FAILURE);
	}

	msg_length = get_packet(conf, &info, &packet);
	if (msg_length <= 0) {
		P("get_packet()");
	}

	ret = input_close(&conf);
	if (ret != 0) {
		P("input_close()");
	}

	return 0;
}
#undef P

#endif
