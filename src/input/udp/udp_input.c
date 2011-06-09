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
 * Input parameters are expected to be in format "port ip_version",
 * port is mandatory and ip_version can be 4 or 6 (default)
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

#include <ipfixcol.h>

/* input buffer length */
#define BUFF_LEN 10000


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
 * \param[in]  params  Expects at least port to listen on, handles space
 * separated number specifying ip version (4 or 6)
 * \param[out] config  Sets source and destination IP, destination port.
 * \return 0 on success, nonzero else.
 */
int input_init(char *params, void **config)
{
    if (params == NULL)  /* no port to listen on */
    {
        VERBOSE(CL_VERBOSE_OFF, "No port to listen on");
        return 1;
    }

    /* necessary structures */
    struct addrinfo *addrinfo, hints;
    struct plugin_conf *conf;
    char *port, *ip_version;
    int ai_family = AF_INET6; /* IPv6 is default */
    struct sockaddr_storage saddr, *saddrp;
    socklen_t saddr_len;
    int ret, ipv6_only = 0;

    saddrp = &saddr;

    /* parse params */
    /* expecting format "port ip_version" */
    port = strtok(params, " ");
    ip_version = strtok(NULL, " ");
    if (ip_version != NULL)
    {
        if (ip_version[0] == '4') ai_family = AF_INET;
        if (ip_version[0] == '6') ai_family = AF_INET6;
    }


    /* allocate plugin_conf structure */
    conf = malloc(sizeof(struct plugin_conf));
    if (conf == NULL)
    {
        VERBOSE(CL_VERBOSE_OFF, "Cannot allocate memory for config structure: %s", strerror(errno));
        return 1;
    }

    conf->info = malloc(sizeof(struct input_info_network));
    if (conf->info == NULL)
    {
        VERBOSE(CL_VERBOSE_OFF, "Cannot allocate memory for input_info structure: %s", strerror(errno));
        return 1;
    }
    /* and pass it to the collector */
    *config = (void*) conf;

    /* specify parameters of the connection */
    memset (&hints, 0, sizeof(struct addrinfo));
    hints.ai_socktype = SOCK_DGRAM; /* UDP */
    hints.ai_family = ai_family; /* both IPv4 and IPv6*/
    hints.ai_flags = AI_PASSIVE; /* server side */

    /* get server address */
    if ((ret = getaddrinfo(NULL, port, &hints, &addrinfo)) != 0)
    {
        VERBOSE(CL_VERBOSE_OFF, "getaddrinfo failed: %s", gai_strerror(ret));
        return 1;
    }

    /* create socket */
    conf->socket = socket(addrinfo->ai_family, addrinfo->ai_socktype, addrinfo->ai_protocol);
    if (conf->socket == -1)
    {
        VERBOSE(CL_VERBOSE_OFF, "Cannot create socket: %s", strerror(errno));
        return 1;
    }

    /* allow IPv4 connections on IPv6 */
    if ((addrinfo->ai_family == AF_INET6) &&
        (setsockopt(conf->socket, IPPROTO_IPV6, IPV6_V6ONLY, &ipv6_only, sizeof(ipv6_only)) == -1))
    {
        VERBOSE(CL_VERBOSE_BASIC, "Cannot turn off socket option IPV6_V6ONLY. Plugin might not accept IPv4 connections");
    }

    /* bind socket to address */
    if (bind(conf->socket, addrinfo->ai_addr, addrinfo->ai_addrlen) != 0)
    {
        VERBOSE(CL_VERBOSE_OFF, "Cannot bind socket: %s\n", strerror(errno));
        return 1;
    }

    /* get binded address -- we listen on all interfaces, so it should be 0.0.0.0*/
    if (getsockname(conf->socket, (struct sockaddr*) saddrp, &saddr_len) != 0)
    {
        VERBOSE(CL_VERBOSE_BASIC, "Cannot get socket name, input_info might lack destination port and address");
        saddrp = (struct sockaddr_storage *) addrinfo->ai_addr;
    }

    /* fill in general information */
    conf->info->type = SOURCE_TYPE_UDP;
    conf->info->dst_port = atoi(port);
    if (addrinfo->ai_family == AF_INET) /* IPv4 */
    {
        conf->info->l3_proto = 4;

        /* copy dst IPv4 address */
        conf->info->dst_addr.ipv4.s_addr =
            ((struct sockaddr_in*) saddrp)->sin_addr.s_addr;
    } else /* IPv6 */
    {
        conf->info->l3_proto = 6;

        /* copy dst IPv6 address */
        int i;
        for (i=0; i<4;i++)
        {
            conf->info->dst_addr.ipv6.s6_addr32[i] =
                ((struct sockaddr_in6*) saddrp)->sin6_addr.s6_addr32[i];
        }
    }

    freeaddrinfo(addrinfo);

    VERBOSE(CL_VERBOSE_BASIC, "Plugin initialization completed successfully");

    return 0;
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
    char buffer[BUFF_LEN];
    struct sockaddr_storage address;
    struct plugin_conf *conf = config;

    /* receive packet */
    length = recvfrom(sock, &buffer, BUFF_LEN, 0, (struct sockaddr*) &address, &addr_length);
    if (length == -1)
    {
        VERBOSE(CL_VERBOSE_OFF, "Failed to receive packet: %s", strerror(errno));
        return 1;
    }

    /* copy packet to collector */
    *packet = malloc(length*sizeof(char));
    memcpy(packet, buffer, length);

    if (address.ss_family == AF_INET)
    {
        /* copy src IPv4 address */
        conf->info->src_addr.ipv4.s_addr =
            ((struct sockaddr_in*) &address)->sin_addr.s_addr;

        /* copy port */
        conf->info->src_port = ntohs(((struct sockaddr_in*)  &address)->sin_port);
    } else
    {
        /* copy src IPv6 address */
        int i;
        for (i=0; i<4;i++)
        {
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
    if ((ret = close(sock)) == -1)
    {
        VERBOSE(CL_VERBOSE_OFF, "Cannot close socket: %s", strerror(errno));
    }

    /* free allocated structures */
    free(((struct plugin_conf*) *config)->info);
    free(*config);

    VERBOSE(CL_VERBOSE_BASIC, "All allocated resources have been freed");

    return 0;
}
/**@}*/
