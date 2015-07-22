/**
 * \file Translator.cpp
 * \author Michal Kozubik <kozubik@cesnet.cz>
 * \brief Translator for JSON storage plugin
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

#include "Translator.h"
#include "protocols.h"

#include <arpa/inet.h>
#include <vector>

/**
 * \brief Format flags
 */
const char *Translator::formatFlags(uint16_t flags)
{
	flags = ntohs(flags);

	buffer[0] = flags & 0x20 ? 'U' : '.';
	buffer[1] = flags & 0x10 ? 'A' : '.';
	buffer[2] = flags & 0x08 ? 'P' : '.';
	buffer[3] = flags & 0x04 ? 'R' : '.';
	buffer[4] = flags & 0x02 ? 'S' : '.';
	buffer[5] = flags & 0x01 ? 'F' : '.';
	buffer[6] = '\0';
	
	return buffer;
}

/**
 * \brief Format IPv6
 */
const char *Translator::formatIPv4(uint32_t addr)
{	
	inet_ntop(AF_INET, &addr, buffer, INET_ADDRSTRLEN);
	return buffer;
}

/**
 * \brief Format IPv6
 */
const char *Translator::formatIPv6(uint8_t* addr)
{	
	inet_ntop(AF_INET6, (struct in6_addr *) addr, buffer, INET6_ADDRSTRLEN);
	return buffer;
}

/**
 * \brief Format timestamp
 */
const char *Translator::formatMac(uint8_t* addr)
{	
	snprintf(buffer, BUFF_SIZE, "%02x:%02x:%02x:%02x:%02x:%02x", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
	return buffer;
}

/**
 * \brief Format protocol
 */
const char *Translator::formatProtocol(uint8_t proto) const
{
	return protocols[proto];
}

/**
 * \brief Format timestamp
 */
const char *Translator::formatTimestamp(uint64_t tstamp, t_units units)
{	
	tstamp = be64toh(tstamp);
	
	/* Convert to milliseconds */
	switch (units) {
	case t_units::SEC:
		tstamp *= 1000;
		break;
	case t_units::MICROSEC:
		tstamp /= 1000;
		break;
	case t_units::NANOSEC:
		tstamp /= 1000000;
		break;
	default: /* MILLI is default */
		break;
	}
	
	timesec = tstamp / 1000;
	msec	= tstamp % 1000;
	tm		= localtime(&timesec);
	
	strftime(buffer, 20, "%FT%T", tm);
	/* append miliseconds */
	sprintf(&(buffer[19]), ".%03u", (const unsigned int) msec);
	
	return buffer;
}
