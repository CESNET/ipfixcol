/**
 * \file ipfixsend.c
 * \author Michal Kozubik <kozubik@cesnet.cz>
 * \brief Tool that sends ipfix packets
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

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <ipfixcol.h>

#include "ipfixsend.h"
#include "reader.h"
#include "sender.h"

#ifdef HAVE_SCTP
#include <netinet/sctp.h>
#endif


#define OPTSTRING "hi:d:p:t:n:"
#define DEFAULT_PORT 4739
#define INFINITY_LOOPS (-1)

#define CHECK_SET(_ptr_, _name_) \
do { \
    if (!(_ptr_)) { \
        fprintf(stderr, "%s must be set!\n", (_name_)); \
        return 1; \
    } \
} while (0)

/**
 * \brief Print usage
 */
void usage()
{
    printf("\n");
    printf("Usage: ipfixsend [options]\n");
    printf("  -h       show this text\n");
    printf("  -i path  IPFIX input file\n");
    printf("  -d ip    Destination IP address\n");
    printf("  -p port  Destination port number, default is %d\n", DEFAULT_PORT);
    printf("  -t type  Connection type (udp, tcp or sctp), default is udp\n");
	printf("  -n num   How many times should file be sent, default is infinity\n");
    printf("\n");
}

/**
 * \brief Main function
 */
int main(int argc, char** argv) 
{
    char *ip = NULL, *input = NULL;
    int c, port = DEFAULT_PORT, type = CT_UDP, loops = INFINITY_LOOPS;
    
	if (argc == 1) {
		usage();
		return 0;
	}
	
    /* Parse parameters */
    while ((c = getopt(argc, argv, OPTSTRING)) != -1) {
        switch (c) {
        case 'h':
            usage();
            return 0;
        case 'i':
            input = optarg;
            break;
        case 'd':
            ip = optarg;
            break;
        case 'p':
            port = atoi(optarg);
            break;
        case 't':
            type = decode_type(optarg);
            if (type == CT_UNKNOWN) {
                fprintf(stderr, "Unknown type \"%s\"!\n", optarg);
                return 1;
            }
#ifndef HAVE_SCTP
			if (type == CT_SCTP) {
				fprintf(stderr, "Tool built without SCTP support!\n");
				return 1;
			}
#endif
            break;
		case 'n':
			loops = atoi(optarg);
			break;
        default:
            fprintf(stderr, "Unknown option\n");
            return 1;
        }
    }
    
    /* Check whether everything is set */
    CHECK_SET(input, "Input file");
    CHECK_SET(ip,    "IP address");
   
    /* Get collector's address */
    struct ip_addr addr;
    if (parse_ip(&addr, ip, port)) {
        return 1;
    }
    
    /* Read input file */
    char **packets = read_packets(input);
    if (!packets) {
        return 1;
    }
    
    /* Create connection */
    int sockfd = create_connection(&addr, type);
    if (sockfd <= 0) {
        free_packets(packets);
        return 1;
    }
    
    /* Send packets */
	int i, ret;
    for (i = 0; loops == INFINITY_LOOPS || i < loops; ++i) {
        ret = send_packets(packets, sockfd);
		if (ret != 0) {
			break;
		}
    }
    
    /* Close connection and free resources */
    close_connection(sockfd);
    free_packets(packets);
    
    return 0;
}

