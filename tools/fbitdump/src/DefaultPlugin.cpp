/**
 * \file DefaultPlugin.h
 * \author Michal Kozubik <kozubik.michal@google.com>
 * \brief Default plugin for parsing input filter
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

#include <iostream>
#include <protocols.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <arpa/inet.h>
#include "plugins/plugin_header.h"
#include "Filter.h"
#include "DefaultPlugin.h"

using namespace fbitdump;

void parseFlags(char *strFlags, char out[PLUGIN_BUFFER_SIZE], void *conf)
{
	(void) conf;
	uint16_t i, intFlags;
	std::stringstream ss;

	/* Convert flags from string into numeric form
	 * 000001 FIN.
	 * 000010 SYN
	 * 000100 RESET
	 * 001000 PUSH
	 * 010000 ACK
	 * 100000 URGENT
	 */
	intFlags = 0;
	for (i = 0; i < strlen(strFlags); i++) {
		switch (strFlags[i]) {
			case 'f':
			case 'F':
				intFlags |= 1;
				break;
			case 's':
			case 'S':
				intFlags |= 2;
				break;
			case 'r':
			case 'R':
				intFlags |= 4;
				break;
			case 'p':
			case 'P':
				intFlags |= 8;
				break;
			case 'a':
			case 'A':
				intFlags |= 16;
				break;
			case 'u':
			case 'U':
				intFlags |= 32;
				break;
		}
	}
	snprintf(out, PLUGIN_BUFFER_SIZE, "%d", intFlags);
}

void parseProto(char *strProto, char out[PLUGIN_BUFFER_SIZE], void *conf)
{
	(void) conf;
	int i;
	std::stringstream ss;

	for (i = 0; i < 138; i++) {
		if (strcasecmp(strProto, protocols[i]) == 0) {
			snprintf(out, PLUGIN_BUFFER_SIZE, "%d", i);
			return;
		}
	}
	out[0] = '\0';
}

void parseDuration(char *duration, char out[PLUGIN_BUFFER_SIZE], void *conf)
{	
	(void) conf;
	snprintf(out, PLUGIN_BUFFER_SIZE, "%f", std::atof(duration) * 1000.0);
}

void printProtocol(const plugin_arg_t * val, int plain_numbers, char ret[PLUGIN_BUFFER_SIZE], void *conf) {
	(void) conf;
	if (!plain_numbers) {
		snprintf( ret, PLUGIN_BUFFER_SIZE, "%s", protocols[val->val[0].uint8] );
	} else {
		snprintf( ret, PLUGIN_BUFFER_SIZE, "%d", (uint16_t)val->val[0].uint8 );
	}
}

void printIPv4(const plugin_arg_t * val, int plain_numbers, char buf[PLUGIN_BUFFER_SIZE], void *conf)
{
	(void) conf; (void) plain_numbers;
	int ret;
	Resolver *resolver;

	resolver = Configuration::instance->getResolver();

	/* translate IP address to domain name, if user wishes so */
	if (resolver != NULL) {
		std::string host;

		ret = resolver->reverseLookup(val->val[0].uint32, host);
		if (ret == true) {
			snprintf( buf, PLUGIN_BUFFER_SIZE, "%s", host.c_str() );
			return;
		}

		/* Error during DNS lookup, print IP address instead */
	}

	/*
	 * user don't want to see domain names, or DNS is somehow broken.
	 * print just IP address
	 */
	struct in_addr in_addr;

	in_addr.s_addr = htonl(val->val[0].uint32);
	inet_ntop(AF_INET, &in_addr, buf, INET_ADDRSTRLEN);
}

void printIPv6(const plugin_arg_t * val, int plain_numbers, char buf[PLUGIN_BUFFER_SIZE], void *conf)
{
	(void) conf; (void) plain_numbers;
	int ret;
	Resolver *resolver;

	resolver = Configuration::instance->getResolver();

	/* translate IP address to domain name, if user wishes so */
	if (resolver != NULL) {
		std::string host;

		ret = resolver->reverseLookup6(val->val[0].uint64, val->val[1].uint64, host);
		if (ret == true) {
			snprintf( buf, PLUGIN_BUFFER_SIZE, "%s", host.c_str() );
		}

		/* Error during DNS lookup, print IP address instead */
	}

	/*
	 * user don't want to see domain names, or DNS is somehow broken.
	 * print just IP address
	 */
	struct in6_addr in6_addr;

	*((uint64_t*) &in6_addr.s6_addr) = htobe64(val->val[0].uint64);
	*(((uint64_t*) &in6_addr.s6_addr)+1) = htobe64(val->val[1].uint64);
	inet_ntop(AF_INET6, &in6_addr, buf, INET6_ADDRSTRLEN);
}

void printTimestamp32(const plugin_arg_t * val, int plain_numbers, char buf[PLUGIN_BUFFER_SIZE], void *conf)
{
	(void) conf; (void) plain_numbers;
	time_t timesec = val->val[0].uint32;
	struct tm *tm = localtime(&timesec);

	printTimestamp(tm, 0, buf);
}

void printTimestamp64(const plugin_arg_t * val, int plain_numbers, char buf[PLUGIN_BUFFER_SIZE], void *conf)
{
	(void) conf; (void) plain_numbers;
	time_t timesec = val->val[0].uint64/1000;
	uint64_t msec = val->val[0].uint64 % 1000;
	struct tm *tm = localtime(&timesec);

	printTimestamp(tm, msec, buf);
}

void printTimestamp(struct tm *tm, uint64_t msec, char buff[PLUGIN_BUFFER_SIZE])
{
	strftime(buff, 20, "%Y-%m-%d %H:%M:%S", tm);
	/* append miliseconds */
	sprintf(&buff[19], ".%03u", (const unsigned int) msec);
}

void printTCPFlags(const plugin_arg_t * val, int plain_numbers, char result[PLUGIN_BUFFER_SIZE], void *conf)
{
	(void) conf; (void) plain_numbers;
	sprintf( result, "%s", "......" );

	if (val->val[0].uint8 & 0x20) {
		result[0] = 'U';
	}
	if (val->val[0].uint8 & 0x10) {
		result[1] = 'A';
	}
	if (val->val[0].uint8 & 0x08) {
		result[2] = 'P';
	}
	if (val->val[0].uint8 & 0x04) {
		result[3] = 'R';
	}
	if (val->val[0].uint8 & 0x02) {
		result[4] = 'S';
	}
	if (val->val[0].uint8 & 0x01) {
		result[5] = 'F';
	}
}

void printDuration(const plugin_arg_t * val, int plain_numbers, char buff[PLUGIN_BUFFER_SIZE], void *conf)
{
	(void) conf; (void) plain_numbers;
	static std::ostringstream ss;
	static std::string str;
	ss << std::fixed;
	ss.precision(3);

	ss << (float) val->val[0].dbl/1000.0;

	str = ss.str();
	ss.str("");

	snprintf( buff, PLUGIN_BUFFER_SIZE, "%s", str.c_str() );
}
