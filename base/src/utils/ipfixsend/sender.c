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

#include <siso.h>

/* Ethernet MTU */
/* Should be MSG_MAX_LENGTH (- some bytes) */
#define UDP_MTU MSG_MAX_LENGTH - 535
#define MIN(_first_, _second_) ((_first_) < (_second_)) ? (_first_) : (_second_)

static int stop_sending = 0;
static struct timeval begin = {0};

void sender_stop()
{
	stop_sending = 1;
}

/**
 * \brief Send packet
 */
inline int send_packet(sisoconf *sender, char *packet)
{
	return siso_send(sender, packet, (int) ntohs(((struct ipfix_header *) packet)->length));
}

/**
 * \brief Send all packets from array
 */
int send_packets(sisoconf *sender, char **packets, int packets_s)
{
    int i, ret;
	struct timeval end;
	double ellapsed;
	
	/* These must be static variables - local would be rewrited with each call of send_packets */
	static int pkts_from_begin = 0;
	
    for (i = 0; packets[i] && stop_sending == 0; ++i) {
        /* send packet */
        ret = send_packet(sender, packets[i]);
        if (ret != SISO_OK) {
            return SISO_ERR;
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
