/**
 * \file sender.h
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

#ifndef SENDER_H
#define	SENDER_H

#include <netdb.h>

/*
 * Structure for IP address
 */
struct ip_addr {
    int type;
    int port;
    union {
        struct sockaddr_in  addr4;
        struct sockaddr_in6 addr6;
    } addr;
};


/*
 * Connection types enumeration
 */
enum connection_type {
    CT_UDP,
    CT_TCP,
    CT_SCTP,
    CT_UNKNOWN
};

/**
 * \brief Decode connection type
 * 
 * @param type connection name
 * @return  connection type
 */
int decode_type(char *type);

/**
 * \brief Send packet
 * 
 * @param packet
 * @param sockfd socket
 * @return 0 on success
 */
int send_packet(char *packet, int sockfd);

/**
 * \brief Send all packets from array
 * 
 * @param packets Packets array
 * @param sockfd socket
 * @return 0 on success
 */
int send_packets(char **packets, int sockfd);

/**
 * \brief Create new connection
 * 
 * @param addr Destination address
 * @param type Connection type
 * @return new socket
 */
int create_connection(struct ip_addr *addr, int type);

/**
 * \brief Close active connection
 * 
 * @param sockfd socket
 */
void close_connection(int sockfd);

/**
 * \brief Parse IP address string
 * 
 * @param addr structure with result
 * @param ip IP address
 * @return 0 on success
 */
int parse_ip(struct ip_addr *addr, char *ip, int port);

/**
 * \brief Send data with limited maximum speed
 * @param data message to be send
 * @param datasize message size
 * @param sockfd socket
 * @param max_speed speed limit
 * @return 0 on success
 */
int send_data_limited(char *data, long datasize, int sockfd, int max_speed);

#endif	/* SENDER_H */

