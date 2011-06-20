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
#include <commlbr.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include <ipfixcol.h>

/* input buffer length */
#define BUFF_LEN 10000
/* default port for udp collector */
#define DEFAULT_PORT "4739"

/**
 * \struct plugin_conf
 * \brief  Plugin configuration structure passed by the collector
 */
struct plugin_conf
{
    int socket; /**< listening socket */
    struct input_info_network *info; /**< infromation structure passed
                                      * to collector */
};

/* expects port number in params argument */
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
    struct sockaddr_storage saddr, *saddrp=NULL;
    socklen_t saddr_len = sizeof (struct sockaddr_storage);
    int ret, ipv6_only = 0, retval = 0;
    /* 1 when using default port - don't free memory */
    int def_port = 0;

    saddrp = &saddr;

    /* allocate plugin_conf structure */
    conf = malloc(sizeof(struct plugin_conf));
    if (conf == NULL) {
        VERBOSE(CL_VERBOSE_OFF, "Cannot allocate memory for config structure: %s", strerror(errno));
        retval = 1;
        goto out;
    }

    conf->info = malloc(sizeof(struct input_info_network));
    if (conf->info == NULL) {
        VERBOSE(CL_VERBOSE_OFF, "Cannot allocate memory for input_info structure: %s", strerror(errno));
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
        VERBOSE(CL_VERBOSE_OFF, "Cannot parse config xml\n");
        retval = 1;
        goto out;
    }
    /* get the root element node */
    root_element = xmlDocGetRootElement(doc);
    if (root_element == NULL) {
        VERBOSE(CL_VERBOSE_OFF, "Cannot get document root element\n");
        retval = 1;
        goto out;
    }

    /* check that we have the right config xml, BAD_CAST is (xmlChar *) cast defined by libxml */
    if (!xmlStrEqual(root_element->name, BAD_CAST "udpCollector")) {
        VERBOSE(CL_VERBOSE_OFF, "Expecting udpCollector root element, got %s", root_element->name);
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
    hints.ai_socktype = SOCK_DGRAM; /* UDP */
    hints.ai_family = ai_family; /* both IPv4 and IPv6*/
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
        VERBOSE(CL_VERBOSE_OFF, "Cannot bind socket: %s\n", strerror(errno));
        retval = 1;
        goto out;
    }

    /* get binded address -- we listen on all interfaces, so it should be 0.0.0.0*/
    if (getsockname(conf->socket, (struct sockaddr*) saddrp, &saddr_len) != 0) {
        VERBOSE(CL_VERBOSE_BASIC, "Cannot get socket name, input_info might lack destination port and address");
        saddrp = (struct sockaddr_storage *) addrinfo->ai_addr;
    }

    /* fill in general information */
    conf->info->type = SOURCE_TYPE_UDP;
    conf->info->dst_port = atoi(port);
    if (addrinfo->ai_family == AF_INET) { /* IPv4 */
        conf->info->l3_proto = 4;

        /* copy dst IPv4 address */
        conf->info->dst_addr.ipv4.s_addr =
            ((struct sockaddr_in*) saddrp)->sin_addr.s_addr;
    } else { /* IPv6 */
        conf->info->l3_proto = 6;

        /* copy dst IPv6 address */
        int i;
        for (i=0; i<4;i++) {
            conf->info->dst_addr.ipv6.s6_addr32[i] =
                ((struct sockaddr_in6*) saddrp)->sin6_addr.s6_addr32[i];
        }
    }

    /* and pass it to the collector */
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
 * \return 0 on success, nonzero else.
 */
int get_packet(void *config, struct input_info **info, char **packet)
{
    /* get socket */
    int sock = ((struct plugin_conf*) config)->socket;
    size_t length = 0;
    socklen_t addr_length = sizeof(struct sockaddr_storage);
    //char buffer[BUFF_LEN];
    struct sockaddr_storage address;
    struct plugin_conf *conf = config;

    /* allocate memory for packet, if needed */
    if (*packet == NULL) {
        *packet = malloc(BUFF_LEN*sizeof(char));
    }

    /* receive packet */
    length = recvfrom(sock, *packet, BUFF_LEN, 0, (struct sockaddr*) &address, &addr_length);
    if (length == -1) {
        VERBOSE(CL_VERBOSE_OFF, "Failed to receive packet: %s", strerror(errno));
        return 1;
    }

    if (address.ss_family == AF_INET) {
        /* copy src IPv4 address */
        conf->info->src_addr.ipv4.s_addr =
            ((struct sockaddr_in*) &address)->sin_addr.s_addr;

        /* copy port */
        conf->info->src_port = ntohs(((struct sockaddr_in*)  &address)->sin_port);
    } else {
        /* copy src IPv6 address */
        int i;
        for (i=0; i<4; i++) {
            conf->info->src_addr.ipv6.s6_addr32[i] =
                ((struct sockaddr_in6*) &address)->sin6_addr.s6_addr32[i];
        }

        /* copy port */
        conf->info->src_port = ntohs(((struct sockaddr_in6*)  &address)->sin6_port);
    }

    /* pass info to the collector */
    *info = (struct input_info*) conf->info;

    return 0;
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

    /* close socket */
    int sock = ((struct plugin_conf*) *config)->socket;
    if ((ret = close(sock)) == -1) {
        VERBOSE(CL_VERBOSE_OFF, "Cannot close socket: %s", strerror(errno));
    }

    /* free allocated structures */
    free(((struct plugin_conf*) *config)->info);
    free(*config);

    VERBOSE(CL_VERBOSE_BASIC, "All allocated resources have been freed");

    return 0;
}
/**@}*/
