/**
 * \file sender.c
 * \author Michal Kozubik <kozubik@cesnet.cz>
 * \brief Functions for parsing IP, connecting to collector and sending packets
 *
 * Copyright (C) 2014 CESNET, z.s.p.o.
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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>

#include <ipfixcol.h>

#include "ipfixsend.h"
#include "sender.h"

/*
 * Connection types by names 
 */
static const char *con_types[] = {
    "UDP",
    "TCP",
    "SCTP"
};

/**
 * \brief Decode connection type
 */
int decode_type(char *type)
{
    int i;
    for (i = 0; i < CT_UNKNOWN; ++i) {
        if (!strcasecmp(con_types[i], type)) {
            break;
        }
    }
    
    return i;
}

/**
 * \brief Send data into socket
 * 
 * @param data Data buffer
 * @param len Data length
 * @param sockfd socket
 * @return 0 on success
 */
int send_data(char *data, int len, int sockfd)
{
    int sent = 0, sent_now;
    
    while (sent != len) {
        sent_now = send(sockfd, data, len, 0);
        if (sent_now == -1) {
            fprintf(stderr, "Error when sending packet (%s)\n", sys_errlist[errno]);
            return -1;
        }
        
        sent += sent_now;
    }
    
    return 0;
}

/**
 * \brief Send packet
 */
int send_packet(char *packet, int sockfd)
{
    return send_data(packet, (int) ntohs(((struct ipfix_header *) packet)->length), sockfd);
}

/**
 * \brief Send all packets from array
 */
int send_packets(char **packets, int sockfd)
{
    int i, ret;
    for (i = 0; packets[i]; ++i) {
        /* send packet */
        ret = send_packet(packets[i], sockfd);
        if (ret) {
            return -1;
        }
    }
	
    return 0;
}

/**
 * \brief Create connection with collector
 */
int create_connection(struct ip_addr *addr, int type)
{
    int sockfd, family = addr->type;
    int socktype, proto = 0;
	
	if (type == CT_UDP) {
		/* create socket for UDP */
		socktype = SOCK_DGRAM;
	}
#ifdef HAVE_SCTP
	else if (type == CT_SCTP) {
		socktype = SOCK_STREAM;
		proto = IPPROTO_SCTP;
	}
#endif
	else {
		socktype = SOCK_STREAM;
	}
	
	/* create socket for TCP/SCTP */
	sockfd = socket(family, socktype, proto);
	if (sockfd < 0) {
		fprintf(stderr, "Cannot create socket\n");
		return -1;
	}
	
	/* connect to collector */
	if (addr->type == AF_INET) {
		/* IPv4 connection */
		if (connect(sockfd, (struct sockaddr *) &(addr->addr.addr4), sizeof(addr->addr.addr4)) < 0) {
			fprintf(stderr, "Cannot connect to collector (%s)\n", sys_errlist[errno]);
			return -1;
		}
	} else {
		/* IPv6 connection */
		if (connect(sockfd, (struct sockaddr *) &(addr->addr.addr6), sizeof(addr->addr.addr6)) < 0) {
			fprintf(stderr, "Cannot connect to collector (%s)\n", sys_errlist[errno]);
			return -1;
		}
	}
	
    return sockfd;
}


/**
 * \brief Parse IP address string
 */
int parse_ip(struct ip_addr *addr, char *ip, int port)
{
    struct hostent *dest = gethostbyname(ip);
    if (!dest) {
        fprintf(stderr, "No such host \"%s\"!\n", ip);
        return -1;
    }
	
    if (dest->h_addrtype == AF_INET) {
		memmove((char *) &(addr->addr.addr4.sin_addr), (char *) dest->h_addr, dest->h_length);
		addr->addr.addr4.sin_family = AF_INET;
		addr->addr.addr4.sin_port = htons(port);
	} else {
		memmove((char *) &(addr->addr.addr4.sin_addr), (char *) dest->h_addr, dest->h_length);
		addr->addr.addr6.sin6_flowinfo = 0;
		addr->addr.addr6.sin6_family = AF_INET6;
		addr->addr.addr6.sin6_port = htons(port);
	}
	
	addr->type = dest->h_addrtype;
	addr->port = port;
	
    return 0;
}

/**
 * \brief Close connection
 */
void close_connection(int sockfd)
{
    close(sockfd);
}