/**
 * \file sender.c
 * \author Michal Kozubik <kozubik@cesnet.cz>
 * \author Lukas Hutak <hutak@cesnet.cz>
 * \brief Functions for parsing IP, connecting to collector and sending packets
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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
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

// 1 second in microseconds
#define MICRO_SEC 1000000L
// 1 second in nanoseconds
#define NANO_SEC 1000000000L

static int stop_sending = 0;
static struct timeval begin = {0};

void sender_stop()
{
	stop_sending = 1;
}

/**
 * \brief Send packet
 */
int send_packet(sisoconf *sender, char *packet)
{
	return siso_send(sender, packet, (int) ntohs(((struct ipfix_header *) packet)->length));
}

/**
 * \brief Calculate time difference
 * \param[in] start First timestamp
 * \param[in] end Second timestamp
 * \return Number of microseconds between timestamps
 */
long timeval_diff(const struct timeval *start, const struct timeval *end)
{
	return (end->tv_sec - start->tv_sec) * MICRO_SEC
		+ (end->tv_usec - start->tv_usec);
}

/**
 * \brief Send all packets from array with speed limitation
 */
int send_packets_limit(sisoconf *sender, char **packets, int packets_s)
{
	struct timeval end;
	struct timespec sleep_time = {0};

	/* These must be static variables - local would be rewrited with each call
	 * of send_packets */
	static int pkts_from_begin = 0;
	static double time_per_pkt = 0.0; // [ms]

	if (begin.tv_sec == 0) {
		/* Absolutely first packet */
		gettimeofday(&begin, NULL);
		time_per_pkt = 1000000.0 / packets_s; // [micro seconds]
	}

	for (int i = 0; packets[i] && stop_sending == 0; ++i) {
		/* send packet */
		int ret = send_packet(sender, packets[i]);
        if (ret != SISO_OK) {
            return SISO_ERR;
        }

		pkts_from_begin++;
		if (packets_s <= 0) {
			/* Limit for packets/s is not enabled */
			continue;
		}

		/* Calculate expected time of sending next packet */
		gettimeofday(&end, NULL);
		long elapsed = timeval_diff(&begin, &end);
		if (elapsed < 0) {
			/* Should be never negative. Just for sure... */
			elapsed = pkts_from_begin * time_per_pkt;
		}

		long next_start = pkts_from_begin * time_per_pkt;
		long diff = next_start - elapsed;

		if (diff >= MICRO_SEC) {
			diff = MICRO_SEC - 1;
		}

		/* Sleep */
		if (diff > 0) {
			sleep_time.tv_nsec = diff * 1000L;
			nanosleep(&sleep_time, NULL);
		}

		if (pkts_from_begin >= packets_s) {
			/* Restart counter */
			gettimeofday(&begin, NULL);
			pkts_from_begin = 0;
		}
	}

	return SISO_OK;
}

/**
 * \brief Get time of the packet from the header
 * \param[in] packet Packet
 * \return Timestamp
 */
uint32_t packet_time(const char *packet)
{
	const struct ipfix_header *header;
	header = (const struct ipfix_header *) packet;

	return ntohl(header->export_time);
}

/**
 * \brief Get the count of packets in a group with the same timestamps
 *
 * The group timestamp is based on the timestamp of the first packet in an
 * array. The group ends when a packet with later timestamp is detected i.e.
 * the group also includes packets with earlier timestamps.
 *
 * \param[in] packets Array of packets
 * \return On success returns value > 0. Otherwise returns 0.
 */
int ts_grp_cnt(char **packets)
{
	if (!packets || !(*packets)) {
		return 0;
	}

	uint32_t grp_ts = packet_time(packets[0]);
	int grp_cnt = 1;

	while (packets[grp_cnt]) {
		if (packet_time(packets[grp_cnt]) > grp_ts) {
			break;
		}

		++grp_cnt;
	}

	return grp_cnt;
}

/**
 * \brief Send all packets from array with real-time simulation
 */
int send_packets_realtime(sisoconf *sender, char **packets, double speed)
{
	if (!packets || !(*packets)) {
		// No packets
		return SISO_OK;
	}

	int grp_cnt = 0; // Number of packets in a group with same timestamp
	int grp_id = 0;  // Index of the packet in the group

	uint32_t grp_ts_prev = 0;
	uint32_t grp_ts_now = packet_time(*packets);
	double time_per_pkt = 0.0;
	struct timeval group_ts_start, end;

	for (int i = 0; packets[i] && stop_sending == 0; ++i) {
		if (grp_cnt == grp_id) {
			/* New group */
			grp_cnt = ts_grp_cnt(&packets[i]);
			grp_id = 0;
			time_per_pkt = 1000000.0 / (grp_cnt * speed); // [micro seconds]

			grp_ts_prev = grp_ts_now;
			grp_ts_now = packet_time(packets[i]);

			/* Sleep between time groups only when difference > 1 second */
			if (grp_ts_now > grp_ts_prev + 1) {
				double  seconds_diff = (grp_ts_now - grp_ts_prev - 1) / speed;
				struct timespec sleep_time;
				sleep_time.tv_sec = (int) seconds_diff;
				sleep_time.tv_nsec = (seconds_diff - (int) seconds_diff) * NANO_SEC;
				nanosleep(&sleep_time, NULL);
			}

			gettimeofday(&group_ts_start, NULL);
		}

		/* Send a packet */
		int ret = send_packet(sender, packets[i]);
		if (ret != SISO_OK) {
			return SISO_ERR;
		}

		++grp_id;

		/* Calculate expected time of sending next packet */
		gettimeofday(&end, NULL);
		long elapsed = timeval_diff(&group_ts_start, &end);
		if (elapsed < 0) {
			/* Should be never negative. Just for sure... */
			elapsed = grp_id * time_per_pkt;
		}

		long next_start = grp_id * time_per_pkt;
		long diff = next_start - elapsed;

		/* Sleep between packets in the group */
		if (diff > 0) {
			struct timespec sleep_time;
			sleep_time.tv_sec = diff / MICRO_SEC;
			sleep_time.tv_nsec = (diff % MICRO_SEC) * 1000L;
			nanosleep(&sleep_time, NULL);
		}
	}

	return SISO_OK;
}
