/**
 * \file Translator.cpp
 * \author Michal Kozubik <kozubik@cesnet.cz>
 * \brief Translator for JSON storage plugin
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

#include "Translator.h"
#include "protocols.h"

#include <arpa/inet.h>
#include <vector>

#define BUFF_SIZE 128

Translator::Translator()
{
	buffer.reserve(BUFF_SIZE);
}

Translator::~Translator()
{
}


/**
 * \brief Format flags
 */
std::string Translator::formatFlags(uint16_t flags) const
{
	std::string result = "......";
	flags = ntohs(flags);
	
	if (flags & 0x20) {
		result[0] = 'U';
	}
	if (flags & 0x10) {
		result[1] = 'A';
	}
	if (flags & 0x08) {
		result[2] = 'P';
	}
	if (flags & 0x04) {
		result[3] = 'R';
	}
	if (flags & 0x02) {
		result[4] = 'S';
	}
	if (flags & 0x01) {
		result[5] = 'F';
	}
	
	return result;
}

/**
 * \brief Format IPv6
 */
std::string Translator::formatIPv4(uint32_t addr)
{
	buffer.clear();
	
	inet_ntop(AF_INET, &addr, buffer.data(), INET_ADDRSTRLEN);
	return std::string(buffer.data());
}

/**
 * \brief Format IPv6
 */
std::string Translator::formatIPv6(uint8_t* addr)
{
	buffer.clear();
	
	inet_ntop(AF_INET6, (struct in6_addr *) addr, buffer.data(), INET6_ADDRSTRLEN);
	return std::string(buffer.data());
}

/**
 * \brief Format timestamp
 */
std::string Translator::formatMac(uint8_t* addr)
{
	buffer.clear();
	
	snprintf(buffer.data(), BUFF_SIZE, "%02x:%02x:%02x:%02x:%02x:%02x", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
	return std::string(buffer.data());
}

/**
 * \brief Format protocol
 */
std::string Translator::formatProtocol(uint8_t proto) const
{
	return std::string(protocols[proto]);
}

/**
 * \brief Format timestamp
 */
std::string Translator::formatTimestamp(uint64_t tstamp, t_units units)
{
	buffer.clear();
	
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
	
	time_t timesec = tstamp / 1000;
	uint64_t msec  = tstamp % 1000;
	struct tm *tm  = localtime(&timesec);
	
	strftime(buffer.data(), 20, "%FT%T", tm);
	/* append miliseconds */
	sprintf(&(buffer.data()[19]), ".%03u", (const unsigned int) msec);
	
	return std::string(buffer.data());
}
