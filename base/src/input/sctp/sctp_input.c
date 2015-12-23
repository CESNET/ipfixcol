/**
 * \file sctp_input.c
 * \author Michal Srb <michal.srb@cesnet.cz>
 * \brief SCTP input plugin for ipfixcol.
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
#include <time.h>

#include "ipfixcol.h"
#include "convert.h"

/* API version constant */
IPFIXCOL_API_VERSION;

#define DEFAULT_LISTEN_PORT        4739
#define DEFAULT_LISTEN_PORT_DLTS   4740   /* listen port when used with DTLS */

/* maximum input/output streams per association */
#define INSTREAMS_PER_SOCKET         20
#define OSTREAMS_PER_SOCKET          20

#define DEFAULT_IPV6_LISTEN_ADDRESS  in6addr_any
#define MAX_EPOLL_EVENTS             1
#define LISTEN_BACKLOG               50

/* just guess, user will want to bind at most 20 addresses (per address family)
 * to listen socket. if this number is not enough the corresponding array 
 * will be reallocated */
#define DEFAULT_NUMBER_OF_ADDRESSES 20

/** Identifier to MSG_* macros */
static char *msg_module = "SCTP input";

/** SCTP input plugin identification for packet conversion from netflow to ipfix format */
#define SCTP_INPUT_PLUGIN

/**
 * \struct input_info_node
 * \brief node of a linked list of input_info structures
 */
struct input_info_node {
	struct input_info_network info;
	int socket;
	struct input_info_node *next;
};

/**
 * \struct sctp_config
 * \brief plugin configuration structure
 */
struct sctp_config {
	uint16_t listen_port;                       /**< listen port (host byte order) */
	int listen_socket;                          /**< listen socket */
	int epollfd;                                /**< epoll file descriptor */
	struct input_info_node *input_info_list;    /**< linked list of input_info structures */
	pthread_mutex_t input_info_list_mutex;      /**< mutex for 'input_info_list' list */
	pthread_t listen_thread;                    /**< id of the thread that listens for new associations */
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
		node = (struct input_info_node *) calloc(1, sizeof(*node));
		if (!node) {
			MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)", __FILE__, __LINE__);
			goto err_assoc;
		}

		src_addr6 = (struct sockaddr_in6 *) &addr;

		node->info.type = SOURCE_TYPE_SCTP;
		node->info.l3_proto = AF_INET6;
		/* source address */
		memcpy(&(node->info.src_addr.ipv6), src_addr6, sizeof(node->info.src_addr.ipv6));
		node->info.src_port = ntohs(src_addr6->sin6_port);
		node->info.dst_port = ntohs(((struct sockaddr_in6*) addr_ptr)->sin6_port);
		node->socket = conn_socket;
		node->info.status = SOURCE_STATUS_NEW;

		/* add input_info to the list */
		pthread_mutex_lock(&(conf->input_info_list_mutex));
		node->next = conf->input_info_list;
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

		inet_ntop(AF_INET6, &(src_addr6->sin6_addr), printable_ip, INET6_ADDRSTRLEN);
		MSG_INFO(msg_module, "New SCTP association from %s", printable_ip);

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
	char dst_addr[INET6_ADDRSTRLEN];
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

	struct sockaddr_in6 **sockaddr6_listen;  /* list of sockaddr6 structures for use with sctp_bindx() */
	uint8_t sockaddr6_listen_counter = 0;    /* counter of the sockaddr6 structures */
	uint8_t sockaddr6_listen_max;            /* maximum sockaddr6 structures in array */

	struct sockaddr_in **sockaddr_listen;    /* list of sockaddr structures for use with sctp_bindx() */
	uint8_t sockaddr_listen_counter = 0;     /* counter of the sockaddr structures */
	uint8_t sockaddr_listen_max;             /* maximum sockaddr structures in array */

	if (params == NULL) {
		MSG_ERROR(msg_module, "No configuration data");
		return -1;
	}

	/* allocate memory for config structure */
	conf = (struct sctp_config *) malloc(sizeof(*conf));
	if (!conf) {
		MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)", __FILE__, __LINE__);
		return -1;
	}
	memset(conf, 0, sizeof(*conf));

	/* array for IPv6 listen addresses. this array will later be used with 
	 * sctp_bindx() for multi-homing support */
	sockaddr6_listen = (struct sockaddr_in6 **) 
	malloc(DEFAULT_NUMBER_OF_ADDRESSES * sizeof(*(sockaddr6_listen)));
	if (sockaddr6_listen == NULL) {
		MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)", __FILE__, __LINE__);
		goto err_sockaddr6;
	}
	memset(sockaddr6_listen, 0, DEFAULT_NUMBER_OF_ADDRESSES * sizeof(*(sockaddr6_listen)));
	sockaddr6_listen_max = DEFAULT_NUMBER_OF_ADDRESSES;

	/* array for IPv4 listen addresses. this array will later be used with 
	 * sctp_bindx() for multi-homing support */
	sockaddr_listen = (struct sockaddr_in **) calloc(DEFAULT_NUMBER_OF_ADDRESSES, sizeof(*(sockaddr_listen)));
	if (sockaddr_listen == NULL) {
		MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)", __FILE__, __LINE__);
		goto err_sockaddr;
	}
	sockaddr_listen_max = DEFAULT_NUMBER_OF_ADDRESSES;

	/* try to parse XML configuration */
	doc = xmlReadMemory(params, strlen(params), "nobase.xml", NULL, 0);
	if (doc == NULL) {
		MSG_ERROR(msg_module, "Configuration not parsed successfully");
		goto err_xml;
	}

	cur = xmlDocGetRootElement(doc);
	if (cur == NULL) {
		MSG_ERROR(msg_module, "Empty configuration");
		goto err_xml;
	}

	if (xmlStrcmp(cur->name, (const xmlChar *) "sctpCollector")) {
		MSG_ERROR(msg_module, "Bad configuration (root node != sctpCollector)");
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
			case (AF_INET):
				/* add new IPv4 address */
				sockaddr = (struct sockaddr_in *) malloc(sizeof(*sockaddr));
				if (!sockaddr) {
					MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)", __FILE__, __LINE__);
					goto err_sockaddr_case;
				}
				memset(sockaddr, 0, sizeof(*sockaddr));

				/* fill address family and IP address */
				sockaddr->sin_family = AF_INET;
				ret = inet_pton(AF_INET, ip_str, &(sockaddr->sin_addr));
				if (ret != 1) {
					/* invalid address */
					MSG_ERROR(msg_module, "Init: %s is not a valid IP address", ip_str);
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
						MSG_ERROR(msg_module, "Realloc failed (%s:%d)", __FILE__, __LINE__);
						MSG_ERROR(msg_module, "Address %s cannot be added - system error", ip_str);
						goto err_sockaddr_case;
					}

					sockaddr6_listen_max *= 2;
				}

				/* everything is ok, add this new address */
				sockaddr_listen[sockaddr_listen_counter] = sockaddr;
				sockaddr_listen_counter += 1;

				break;

err_sockaddr_case:
				/* error handling */
				if (sockaddr != NULL) {
					free(sockaddr);
				}

				xmlFree(ip_str);
				break;
			/* end of case(AF_INET) */

			case (AF_INET6):
				/* add new IPv6 address */
				sockaddr6 = (struct sockaddr_in6 *) malloc(sizeof(*sockaddr6));
				if (!sockaddr6) {
					MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)", __FILE__, __LINE__);
					goto err_sockaddr6_case;
				}
				memset(sockaddr6, 0, sizeof(*sockaddr6));

				/* fill address family and IP address */
				sockaddr6->sin6_family = AF_INET6;
				ret = inet_pton(AF_INET6, ip_str, &(sockaddr6->sin6_addr));
				if (ret != 1) {
					/* invalid address */
					MSG_ERROR(msg_module, "Init: %s is not valid IP address", ip_str);
					goto err_sockaddr6_case;
				}

				/* note we don't know yet what the desired port
				 * is, so we will fill it later */
				if (sockaddr6_listen_counter >= sockaddr6_listen_max) {
					/* oops, we need to realloc array that holds these addresses */
					sockaddr_listen_old = (struct sockaddr_in **) sockaddr6_listen;
					sockaddr6_listen = realloc(sockaddr6_listen, sockaddr6_listen_max * 2);
					
					if (sockaddr6_listen == NULL) {
						/* realloc fails; discard this address and continue */
						sockaddr6_listen = (struct sockaddr_in6 **) sockaddr_listen_old;
						MSG_ERROR(msg_module, "Realloc failed (%s:%d)", __FILE__, __LINE__);
						MSG_ERROR(msg_module, "Address %s cannot be added - system error", ip_str);
						goto err_sockaddr6_case;
					}
					
					sockaddr6_listen_max *= 2;
				}

				/* everything is ok; add this new address */
				sockaddr6_listen[sockaddr6_listen_counter] = sockaddr6;
				sockaddr6_listen_counter += 1;

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
				/* unknown address family; this point should never be reached */
				MSG_ERROR(msg_module, "Unknown address family; this should never happen (%s, %d)", __FILE__, __LINE__);
				break;
			} /* switch */

			xmlFree(ip_str);
		}

		if ((!xmlStrcmp(cur->name, (const xmlChar *) "localPort"))) {
			/* user specified (another) listen port */
			if (port_set != 0) {
				/* damn, port is specified multiple times. this 
				 * might be a bug in configuration */
				MSG_WARNING(msg_module, "Listen port is specified multiple times in configuration file");
			}
			listen_port_str = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);

			conf->listen_port = atoi((char *) listen_port_str);
			port_set = 1;

			xmlFree(listen_port_str);
		}

		cur = cur->next;
	}

	/* use default address if user doesn't specified any */
	if ((sockaddr6_listen_counter == 0) && (sockaddr_listen_counter == 0)) {
		sockaddr6 = (struct sockaddr_in6 *) calloc(1, sizeof(*sockaddr6));
		if (!sockaddr6) {
			MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)", __FILE__, __LINE__);
			goto err;
		}

		sockaddr6->sin6_family = AF_INET6;

		/* listen on any IPv6 address */
		memcpy(&(sockaddr6->sin6_addr), &(DEFAULT_IPV6_LISTEN_ADDRESS), sizeof(DEFAULT_IPV6_LISTEN_ADDRESS));
		sockaddr6_listen[0] = sockaddr6;
		sockaddr6_listen_counter += 1;
	}

	/* use default listen port if not specified otherwise */
	if (conf->listen_port == 0) {
		conf->listen_port = DEFAULT_LISTEN_PORT;
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
	 * SOCK_SEQPACKET would create one-to-many style socket. For our 
	 * purpose, one-to-one is just fine */
	conf->listen_socket = socket(AF_INET6, SOCK_STREAM, IPPROTO_SCTP);

	/* Retry with IPv4 when the implementation does not support the specified address family */
	if (conf->listen_socket == (uint16_t) -1 && errno == EAFNOSUPPORT) {
		conf->listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP);
	}

	/* Check result */
	if (conf->listen_socket == -1) {
		MSG_ERROR(msg_module, "socket() - %s", strerror(errno));
		goto err;
	}

	/* bind IPv6 listen addresses */
	for (i = 0; i < sockaddr6_listen_counter; i++) {
		ret = sctp_bindx(conf->listen_socket, (struct sockaddr *) sockaddr6_listen[i], 1, SCTP_BINDX_ADD_ADDR);
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

	conf->epollfd = epollfd;

	/* create listen worker */
	ret = pthread_create(&(conf->listen_thread), NULL, listen_worker, conf);
	if (ret != 0) {
		MSG_ERROR(msg_module, "Unable to create listen_worker thread");
		goto err_listen;
	}

	/* allocate memory for templates */
	if (convert_init(SCTP_PLUGIN, MSG_MAX_LENGTH) != 0) {
		MSG_ERROR(msg_module, "malloc() for templates failed!");
		goto err_listen;
	}

	/* print info */
	if (sockaddr6_listen_counter > 0) {
		inet_ntop(AF_INET6, &(sockaddr6_listen[0]->sin6_addr), dst_addr, INET6_ADDRSTRLEN);
	} else {
		inet_ntop(AF_INET, &(sockaddr_listen[0]->sin_addr), dst_addr, INET6_ADDRSTRLEN);
	}
	
	MSG_INFO(msg_module, "Input plugin listening on %s, port %u", dst_addr, conf->listen_port);

	/* pass general information to the collector */
	*config = (void*) conf;

	/* normal exit, all OK */
	MSG_INFO(msg_module, "Plugin initialization completed successfully");

	/* do some cleanup */
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
 * \brief Receive data from opened associations
 *
 * \param[in] config plugin config structure
 * \param[out] info information about input
 * \param[out] packet IPFIX message
 * \param[out] source_status Status of source (new, opened, closed)
 * \return message length on success, error code otherwise
 */
int get_packet(void *config, struct input_info** info, char **packet, int *source_status)
{	
	struct sctp_config *conf;
	int socket;
	struct sctp_sndrcvinfo sinfo;
	int flags;
	ssize_t msg_length;
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
		MSG_ERROR(msg_module, "epoll_wait() wakes up, but no descriptors are ready - %s", strerror(errno));
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
		MSG_ERROR(msg_module, "Something is horribly wrong; missing input_info for SCTP association");
		ret = INPUT_ERROR;
		goto out;
	}

	/* allocate memory for message, if needed */
	if (*packet == NULL) {
		/* TODO - we can check how big the message is from its header */
		*packet = (char *) malloc(MSG_MAX_LENGTH);
		if (*packet == NULL) {
			MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)", __FILE__, __LINE__);
			ret = INPUT_ERROR;
			goto out;
		}
		packet_allocated_here = 1;
	}

	/* new message */
	flags = 0;
	msg_length = sctp_recvmsg(socket, *packet, MSG_MAX_LENGTH, NULL, NULL, &sinfo, &flags);
	if (msg_length == -1) {
		MSG_ERROR(msg_module, "sctp_recvmsg()");
		ret = INPUT_ERROR;
		goto out;
	}

	if (msg_length < IPFIX_HEADER_LENGTH) {
		MSG_WARNING(msg_module, "Packet header is incomplete; skipping message...", msg_length);
		return INPUT_INTR;
	}

	/* Convert packet from Netflow v5/v9/sflow to IPFIX format */
	if (htons(((struct ipfix_header *) (*packet))->version) != IPFIX_VERSION) {
		if (convert_packet(packet, &msg_length, MSG_MAX_LENGTH, NULL) != 0) {
			MSG_WARNING(msg_module, "Message conversion error; skipping message...");
			return INPUT_INTR;
		}
	}

	/* Check if lengths are the same */
	if (msg_length < htons(((struct ipfix_header *) *packet)->length)) {
		MSG_WARNING(msg_module, "Packet is incomplete; skipping message...");
		return INPUT_INTR;
	} else if (msg_length > htons(((struct ipfix_header *) *packet)->length)) {
		msg_length = htons(((struct ipfix_header *) *packet)->length);
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
			MSG_INFO(msg_module, "SCTP input plugin: Exporter disconnected");

			/* remove disconnected socket from event poll */
			ret = epoll_ctl(conf->epollfd, EPOLL_CTL_DEL, socket, NULL);
			if (ret == -1) {
				/* damn... what can i do */
				MSG_ERROR(msg_module, "epoll_ctl(...,EPOLL_CTL_DEL,...) error (%s:%d)", __FILE__, __LINE__);
			}

			/* no more data from this exporter */
			/* \todo free input info structure now (in near future) */
			*source_status = SOURCE_STATUS_CLOSED;
			return INPUT_CLOSED;
		} else {
			MSG_WARNING(msg_module, "Unsupported SCTP event occured");
			goto wait_for_data;
		}
	} else if (!(flags & MSG_EOR)) {
		MSG_WARNING(msg_module, "SCTP input plugin: message is too long");
	}

	/* Set source status */
	*source_status = info_node->info.status;
	if (info_node->info.status == SOURCE_STATUS_NEW) {
		info_node->info.status = SOURCE_STATUS_OPENED;
		info_node->info.odid = ntohl(((struct ipfix_header *) *packet)->observation_domain_id);
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
			MSG_ERROR(msg_module, "Error while closing association");
		}
		
		/* free input_info structure for this association */
		free(node);

		node = next_node;
	}

	free(conf);
	convert_close();

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
