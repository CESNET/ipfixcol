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
#include <string.h>
#include <ipfixcol.h>
#include <signal.h>

#include <siso.h>

#include "ipfixsend.h"
#include "reader.h"
#include "sender.h"

#ifdef HAVE_SCTP
#include <netinet/sctp.h>
#endif


#define OPTSTRING "hi:d:p:t:n:s:S:"
#define DEFAULT_PORT "4739"
#define DEFAULT_TYPE "UDP"
#define INFINITY_LOOPS (-1)

#define CHECK_SET(_ptr_, _name_) \
do { \
	if (!(_ptr_)) { \
		fprintf(stderr, "%s must be set!\n", (_name_)); \
		return 1; \
	} \
} while (0)

static int stop = 0;

/**
 * \brief Print usage
 */
void usage()
{
	printf("\n");
	printf("Usage: ipfixsend [options]\n");
	printf("  -h         Show this help\n");
	printf("  -i path    IPFIX input file\n");
	printf("  -d ip      Destination IP address\n");
	printf("  -p port    Destination port number (default: %s)\n", DEFAULT_PORT);
	printf("  -t type    Connection type (UDP, TCP or SCTP) (default: UDP)\n");
	printf("  -n num     How many times the file should be sent (default: infinity)\n");
	printf("  -s speed   Maximum data sending speed/s\n");
	printf("             Supported suffixes: B (default), K, M, G\n");
	printf("  -S packets Speed limit in packets/s\n");
	printf("\n");
}

void handler(int signal)
{
	(void) signal; // skip compiler warning
	sender_stop();
	stop = 1;
}

/**
 * \brief Main function
 */
int main(int argc, char** argv) 
{
	char *ip = NULL, *input = NULL, *speed = NULL, *type = DEFAULT_TYPE, *port = NULL;
	int c, loops = INFINITY_LOOPS;
	int packets_s = 0;
	
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
			port = optarg;
			break;
		case 't':
			type = optarg;
			break;
		case 'n':
			loops = atoi(optarg);
			break;
		case 's':
			speed = optarg;
			break;
		case 'S':
			packets_s = atoi(optarg);
			break;
		default:
			fprintf(stderr, "Unknown option\n");
			return 1;
		}
	}
	
	/* Check whether everything is set */
	CHECK_SET(input, "Input file");
	CHECK_SET(ip,    "IP address");

	signal(SIGINT, handler);
	
	/* Get collector's address */
	sisoconf *sender = siso_create();
	if (!sender) {
		fprintf(stderr, "Memory allocation error\n");
		return 1;
	}
	
	/* Load packets from file */
	char **packets = read_packets(input);
	if (!packets) {
		return 1;
	}
	
	/* Create connection */
	int ret = siso_create_connection(sender, ip, port, type);
	if (ret != SISO_OK) {
		fprintf(stderr, "%s\n", siso_get_last_err(sender));
		siso_destroy(sender);
		return 1;
	}
	
	/* Set max. speed */
	if (speed) {
		siso_set_speed_str(sender, speed);
	}
	
	/* Send packets */
	int i;
	for (i = 0; stop == 0 && (loops == INFINITY_LOOPS || i < loops); ++i) {
		ret = send_packets(sender, packets, packets_s);
		if (ret != SISO_OK) {
			fprintf(stderr, "%s\n", siso_get_last_err(sender));
			break;
		}
	}
		
	/* Free resources*/
	free_packets(packets);
	
	siso_destroy(sender);
	return 0;
}
