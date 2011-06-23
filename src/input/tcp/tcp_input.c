/**
 * \file tcp_input.c
 * \author Petr Velan <petr.velan@cesnet.cz>
 * \brief IPFIX Collector TCP Input Plugin
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
 * \defgroup tcpInput TCP input plugin for ipfixcol
 * \ingroup inputPLugins
 *
 * This is implementation of the input plugin API for TCP network input.
 * Input parameters are passed in xml format.
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
#include <pthread.h>
#include <commlbr.h>
#include <signal.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include <ipfixcol.h>

/* input buffer length */
#define BUFF_LEN 10000
/* default port for tcp collector */
#define DEFAULT_PORT "4739"
/* backlog for tcp connections */
#define BACKLOG SOMAXCONN
/* size of array of socket addresses */
#define ADDR_ARRAY_SIZE 50

/**
 * \struct plugin_conf
 * \brief  Plugin configuration structure passed by the collector
 */
struct plugin_conf
{
    int socket; /**< listening socket */
    struct input_info_network *info; /**< infromation structure passed
                                      * to collector */
    fd_set master; /**< set of all active sockets */
    int fd_max; /**< max file descriptor number */
    struct sockaddr_in6 *sock_addresses[ADDR_ARRAY_SIZE]; /*< array of addresses indexed by sockets */
};

pthread_mutex_t mutex;
pthread_t listen_thread;

/**
 * \brief Free address variable on input_listen exit
 *
 * \param[in] address Address to free on exit
 * \return void
 */
void input_listen_cleanup(void *address)
{
    if (address != NULL) {
        free(address);
    }
}

/**
 * \brief Funtion that listens for new connetions
 *
 * Runs in a thread and adds new connections to plugin_conf->master set
 *
 * \param[in, out] config Plugin configuration structure
 * \return NULL always
 */
void *input_listen(void *config)
{
    struct plugin_conf *conf = (struct plugin_conf *) config;
    int new_sock;
    /* use IPv6 sockaddr structure to store address information (IPv4 fits easily) */
    struct sockaddr_in6 *address = NULL;
    socklen_t addr_length;
    char src_addr[INET6_ADDRSTRLEN];

    /* loop ends when thread is cancelled by pthread_cancel() function */
    while (1) {
        /* allocate space for the address */
        addr_length = sizeof(struct sockaddr_in6);
        address = malloc(addr_length);

        /* ensure that address will be freed when thread is canceled */ 
        pthread_cleanup_push(input_listen_cleanup, (void *) address);

        if ((new_sock = accept(conf->socket, (struct sockaddr*) address, &addr_length)) == -1) {
            VERBOSE(CL_VERBOSE_BASIC, "Cannot accept new socket: %s", strerror(errno));
            /* exit and call cleanup */
            pthread_exit(0);
        } else {
            pthread_mutex_lock(&mutex);
            FD_SET(new_sock, &conf->master);

            if (conf->fd_max < new_sock) {
                conf->fd_max = new_sock;
            }

            /* copy socket address to config structure */
            conf->sock_addresses[new_sock] = address;

            /* print info */
            if (conf->info->l3_proto == 4) {
                inet_ntop(AF_INET, (void *)&((struct sockaddr_in*) address)->sin_addr, src_addr, INET6_ADDRSTRLEN);
            } else {
                inet_ntop(AF_INET6, &address->sin6_addr, src_addr, INET6_ADDRSTRLEN);
            }
            VERBOSE(CL_VERBOSE_BASIC, "Exporter connected from address %s", src_addr);

            /* unset the address so that we do not free it incidentally */
            address = NULL;
            pthread_mutex_unlock(&mutex);
        }
        pthread_cleanup_pop(0);
    }
    return NULL;
}


/**
 * \brief Input plugin initializtion function
 *
 * \param[in]  params  XML with input parameters
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
        VERBOSE(CL_VERBOSE_OFF, "Cannot allocate memory for config structure: %s", strerror(errno));
        retval = 1;
        goto out;
    }

    conf->info = calloc(1, sizeof(struct input_info_network));
    if (conf->info == NULL) {
        VERBOSE(CL_VERBOSE_OFF, "Cannot allocate memory for input_info structure: %s", strerror(errno));
        retval = 1;
        goto out;
    }

    /* empty the master set */
    FD_ZERO(&conf->master);

    /* parse params */
    xmlDoc *doc = NULL;
    xmlNode *root_element = NULL;
    xmlNode *cur_node = NULL;

    /* parse xml string */
    doc = xmlParseDoc(BAD_CAST params);
    if (doc == NULL) {
        VERBOSE(CL_VERBOSE_OFF, "Cannot parse config xml");
        retval = 1;
        goto out;
    }
    /* get the root element node */
    root_element = xmlDocGetRootElement(doc);
    if (root_element == NULL) {
        VERBOSE(CL_VERBOSE_OFF, "Cannot get document root element");
        retval = 1;
        goto out;
    }

    /* check that we have the right config xml, BAD_CAST is (xmlChar *) cast defined by libxml */
    if (!xmlStrEqual(root_element->name, BAD_CAST "tcpCollector")) {
        VERBOSE(CL_VERBOSE_OFF, "Expecting tcpCollector root element, got %s", root_element->name);
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
                VERBOSE(CL_VERBOSE_OFF, "Cannot allocate memory: %s", strerror(errno));
                retval = 1;
                goto out;
            }

            if (xmlStrEqual(cur_node->name, BAD_CAST "localPort")) { /* set local port */
                port = tmp_val;
            } else if (xmlStrEqual(cur_node->name, BAD_CAST "localIPAddress")) { /* set local address */
                address = tmp_val;
            /* save following configuration to input_info */
            } else if (xmlStrEqual(cur_node->name, BAD_CAST "templateLifeTime")) {
                conf->info->template_life_time = tmp_val;
            } else if (xmlStrEqual(cur_node->name, BAD_CAST "optionsTemplateLifeTime")) {
                conf->info->options_template_life_time = tmp_val;
            } else if (xmlStrEqual(cur_node->name, BAD_CAST "templateLifePacket")) {
                conf->info->template_life_packet = tmp_val;
            } else if (xmlStrEqual(cur_node->name, BAD_CAST "optionsTemplateLifePacket")) {
                conf->info->options_template_life_packet = tmp_val;
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
    hints.ai_socktype = SOCK_STREAM; /* TCP */
    hints.ai_family = ai_family; /* select IPv4 or IPv6*/
    hints.ai_flags = AI_V4MAPPED; /* we want to accept mapped addresses */
    if (address == NULL) {
        hints.ai_flags |= AI_PASSIVE; /* no address given, listen on all local addresses */
    }

    /* get server address */
    if ((ret = getaddrinfo(address, port, &hints, &addrinfo)) != 0) {
        VERBOSE(CL_VERBOSE_OFF, "getaddrinfo failed: %s", gai_strerror(ret));
        retval = 1;
        goto out;
    }

    /* create socket */
    conf->socket = socket(addrinfo->ai_family, addrinfo->ai_socktype, addrinfo->ai_protocol);
    if (conf->socket == -1) {
        VERBOSE(CL_VERBOSE_OFF, "Cannot create socket: %s", strerror(errno));
        retval = 1;
        goto out;
    }

    /* allow IPv4 connections on IPv6 */
    if ((addrinfo->ai_family == AF_INET6) &&
        (setsockopt(conf->socket, IPPROTO_IPV6, IPV6_V6ONLY, &ipv6_only, sizeof(ipv6_only)) == -1)) {
        VERBOSE(CL_VERBOSE_BASIC, "Cannot turn off socket option IPV6_V6ONLY. Plugin might not accept IPv4 connections");
    }

    /* bind socket to address */
    if (bind(conf->socket, addrinfo->ai_addr, addrinfo->ai_addrlen) != 0) {
        VERBOSE(CL_VERBOSE_OFF, "Cannot bind socket: %s", strerror(errno));
        retval = 1;
        goto out;
    }

    /* this is a listening socket */
    if (listen(conf->socket, BACKLOG) == -1) {
        VERBOSE(CL_VERBOSE_OFF, "Cannot listen on socket: %s", strerror(errno));
        retval = 1;
        goto out;
    }

    /* fill in general information */
    conf->info->type = SOURCE_TYPE_TCP;
    conf->info->dst_port = atoi(port);
    if (addrinfo->ai_family == AF_INET) { /* IPv4 */
        conf->info->l3_proto = 4;

        /* copy dst IPv4 address */
        conf->info->dst_addr.ipv4.s_addr =
            ((struct sockaddr_in*) addrinfo->ai_addr)->sin_addr.s_addr;

        inet_ntop(AF_INET, &conf->info->dst_addr.ipv4, dst_addr, INET6_ADDRSTRLEN);
    } else { /* IPv6 */
        conf->info->l3_proto = 6;

        /* copy dst IPv6 address */
        int i;
        for (i=0; i<4;i++) {
            conf->info->dst_addr.ipv6.s6_addr32[i] =
                ((struct sockaddr_in6*) addrinfo->ai_addr)->sin6_addr.s6_addr32[i];
        }

        inet_ntop(AF_INET6, &conf->info->dst_addr.ipv6, dst_addr, INET6_ADDRSTRLEN);
    }
    /* print info */
    VERBOSE(CL_VERBOSE_BASIC, "TCP input plugin listening on address %s, port %s", dst_addr, port);


    /* start listening thread */
    /* \todo check whether some signals shall be blocked */
    pthread_create(&listen_thread, NULL, &input_listen, (void *) conf);

    /* pass general information to the collector */
    *config = (void*) conf;

    /* normal exit, all OK */
    VERBOSE(CL_VERBOSE_BASIC, "Plugin initialization completed successfully");

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
        if (conf->info != NULL) {
            if (conf->info->template_life_time != NULL) {
                free (conf->info->template_life_time);
            }
            if (conf->info->options_template_life_time != NULL) {
                free (conf->info->options_template_life_time);
            }
            if (conf->info->template_life_packet != NULL) {
                free (conf->info->template_life_packet);
            }
            if (conf->info->options_template_life_packet != NULL) {
                free (conf->info->options_template_life_packet);
            }
            free(conf->info);
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
 * \return the length of packet on success, INPUT_CLOSE when some connection 
 *  closed, INPUT_ERROR on error.
 */
int get_packet(void *config, struct input_info **info, char **packet)
{
    /* temporary socket set */
    fd_set tmp_set;
    ssize_t length = 0;
    struct timeval tv;
    char src_addr[INET6_ADDRSTRLEN];
    struct sockaddr_in6 *address;
    struct plugin_conf *conf = config;
    int retval = 0, sock;

    /* allocate memory for packet, if needed */
    if (*packet == NULL) {
        *packet = malloc(BUFF_LEN*sizeof(char));
    }
   
    /* wait until some socket is ready */
    while (retval <= 0) {
        /* copy all sockets from master to tmp_set */
        FD_ZERO(&tmp_set);
        pthread_mutex_lock(&mutex);
        tmp_set = conf->master;
        pthread_mutex_unlock(&mutex);

        /* wait at most one second - give time to check for new sockets */
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        /* select active connections */
        retval = select(conf->fd_max + 1, &tmp_set, NULL, NULL, &tv);
        if (retval == -1) {
            VERBOSE(CL_VERBOSE_OFF, "Failed to select active connection: %s", strerror(errno));
            return INPUT_ERROR;
        }
    }

    /* find first socket that is ready */
    for (sock=0; sock <= conf->fd_max; sock++) {
        /* fetch first active connection */
        if (FD_ISSET(sock, &tmp_set)) {
            break;
        }
    }

    /* receive packet */
    length = recv(sock, *packet, BUFF_LEN, 0);
    if (length == -1) {
        VERBOSE(CL_VERBOSE_OFF, "Failed to receive packet: %s", strerror(errno));
        return INPUT_ERROR;
    }

    /* get peer address from configuration */
    address = conf->sock_addresses[sock];

    if (address->sin6_family == AF_INET) {
        /* copy src IPv4 address */
        conf->info->src_addr.ipv4.s_addr =
            ((struct sockaddr_in*) address)->sin_addr.s_addr;

        /* copy port */
        conf->info->src_port = ntohs(((struct sockaddr_in*)  address)->sin_port);
    } else {
        /* copy src IPv6 address */
        int i;
        for (i=0; i<4; i++) {
            conf->info->src_addr.ipv6.s6_addr32[i] = address->sin6_addr.s6_addr32[i];
        }

        /* copy port */
        conf->info->src_port = ntohs(address->sin6_port);
    }

    /* pass info to the collector */
    *info = (struct input_info*) conf->info;

    /* check whether socket closed */
    if (length == 0) {
        /* print info */
        if (conf->info->l3_proto == 4) {
            inet_ntop(AF_INET, (void *)&((struct sockaddr_in*) conf->sock_addresses[sock])->sin_addr, src_addr, INET6_ADDRSTRLEN);
        } else {
            inet_ntop(AF_INET6, &conf->sock_addresses[sock]->sin6_addr, src_addr, INET6_ADDRSTRLEN);
        }
        VERBOSE(CL_VERBOSE_BASIC, "Exporter on address %s closed connection", src_addr);
           
        /* use mutex so that listening thread does not reuse the socket too quickly */
        pthread_mutex_lock(&mutex);
        close(sock);
        FD_CLR(sock, &conf->master);
        free(conf->sock_addresses[sock]);
        pthread_mutex_unlock(&mutex);
        return INPUT_CLOSED;
    }

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
    int ret, error = 0, sock=0;
    struct plugin_conf *conf = (struct plugin_conf*) *config;

    /* kill the listening thread */
    if(pthread_cancel(listen_thread) != 0) {
        VERBOSE(CL_VERBOSE_OFF, "Cannot cancel listening thread");
    } else {
        pthread_join(listen_thread, NULL);
    }
    
    /* close listening socket */
    if ((ret = close(conf->socket)) == -1) {
        error++;
        VERBOSE(CL_VERBOSE_OFF, "Cannot close listening socket: %s", strerror(errno));
    }

    /* close open sockets */
    for (sock = 0; sock <= conf->fd_max; sock++) {
        if (FD_ISSET(sock, &conf->master)) {
            if ((ret = close(sock)) == -1) {
                error++;
                VERBOSE(CL_VERBOSE_OFF, "Cannot close socket: %s", strerror(errno));
            }
        }
    }
    
    /* free allocated structures */
    FD_ZERO(&conf->master);
    free(((struct plugin_conf*) *config)->info);
    free(*config);
    *config = NULL;

    VERBOSE(CL_VERBOSE_BASIC, "All allocated resources have been freed");

    return error;
}
/**@}*/
