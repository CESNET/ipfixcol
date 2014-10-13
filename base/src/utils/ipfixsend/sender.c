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
#include <time.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>

#include <sys/time.h>
#include <ipfixcol.h>

#include "ipfixsend.h"
#include "sender.h"

/* Ethernet MTU */
static int ETH_MTU = 1500;
static int stop_sending = 0;

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

void sender_stop()
{
	stop_sending = 1;
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
    int sent_now;
	char *ptr = data;
    int to_send = len;
	
    while (to_send > 0) {
		sent_now = send(sockfd, ptr, (to_send < ETH_MTU) ? to_send : ETH_MTU, 0);
		if (sent_now == -1) {
			fprintf(stderr, "Error when sending packet (%s)\n", sys_errlist[errno]);
			return -1;
		}

		ptr += sent_now;
		to_send -= sent_now;
    }
    
    return len;
}

static struct timeval begin = {0};

/**
 * \brief Send data with limited max_speed
 */
int send_data_limited(char *data, long datasize, int sockfd, int max_speed)
{
	ssize_t len = 0, towrite = 0, todo = datasize;
	double elapsed;
	char *ptr = data;
	struct timeval end;
	static int sent = 0;
	
//	printf("size: %d B\t limit: %d B/s\n", datasize, max_speed);
	
	while (todo > 0) {
		/* send data */
		towrite = (todo > max_speed) ? max_speed : todo;
		len = send_data(ptr, towrite, sockfd);
		if (len != towrite) {
			return -1;
		}
		
		sent += len;
		if (sent >= max_speed) {
			gettimeofday(&end, NULL);
			
			/* Should sleep? */
			elapsed = end.tv_usec - begin.tv_usec;
			if (elapsed < 1000000.0) {
				usleep(1000000.0 - elapsed);
				gettimeofday(&end, NULL);
			}
			
			begin = end;
			sent = 0;
		}
		
		/* how long should i sleep? */
		todo -= len;
		ptr += len;
	}
	
	return 0;
}

/**
 * \brief Send packet
 */
int send_packet(char *packet, int sockfd, int speed)
{
	if (speed > 0) {
		return send_data_limited(packet, (int) ntohs(((struct ipfix_header *) packet)->length), sockfd, speed);
	}
	
    return send_data(packet, (int) ntohs(((struct ipfix_header *) packet)->length), sockfd);
}

/**
 * \brief Send all packets from array
 */
int send_packets(char **packets, int sockfd, int packets_s, int speed)
{
    int i, ret;
	struct timeval end;
	double ellapsed;
	
	/* These must be static variables - local would be rewrited with each call of send_packets */
	static int pkts_from_begin = 0;
	
	
    for (i = 0; packets[i] && stop_sending == 0; ++i) {
        /* send packet */
        ret = send_packet(packets[i], sockfd, speed);
        if (ret < 0) {
            return -1;
        }
		
		pkts_from_begin++;
		/* packets counter reached, sleep? */
		if (packets_s > 0 && (pkts_from_begin >= packets_s || begin.tv_sec == 0)) {
			/* end of sending interval in microseconds */
			gettimeofday(&end, NULL);
			
			/* Should sleep? */
			ellapsed = end.tv_usec - begin.tv_usec;
			if (ellapsed < 1000000.0) {
				usleep(1000000.0 - ellapsed);
				gettimeofday(&end, NULL);
			}
			
			begin = end;
			pkts_from_begin = 0;
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