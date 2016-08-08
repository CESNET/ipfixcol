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

#include "Storage.h"

/**
 * \brief Format flags 16bits
 */
const char *Translator::formatFlags16(uint16_t flags)
{
	flags = ntohs(flags);
	return formatFlags8((uint8_t) flags);
}

/**
 * \brief Format flags 8bits
 */
const char *Translator::formatFlags8(uint8_t flags)
{
	buffer[0] = '"';
	buffer[1] = flags & 0x20 ? 'U' : '.';
	buffer[2] = flags & 0x10 ? 'A' : '.';
	buffer[3] = flags & 0x08 ? 'P' : '.';
	buffer[4] = flags & 0x04 ? 'R' : '.';
	buffer[5] = flags & 0x02 ? 'S' : '.';
	buffer[6] = flags & 0x01 ? 'F' : '.';
	buffer[7] = '"';
	buffer[8] = '\0';

	return buffer;
}

/**
 * \brief Constructor
 */
Translator::Translator()
{
	buffer = new char[Translator::BUFF_SIZE];
}

Translator::~Translator()
{
	delete[] buffer;
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
	snprintf(buffer, BUFF_SIZE, "%02x:%02x:%02x:%02x:%02x:%02x",
		addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
	return buffer;
}

/**
 * \brief Format protocol
 */
const char *Translator::formatProtocol(uint8_t proto)
{
	snprintf(buffer, BUFF_SIZE, "\"%s\"", protocols[proto]);
	return buffer;
}

/**
 * \brief Format timestamp
 */
const char *Translator::formatTimestamp(uint64_t tstamp, t_units units, struct json_conf * config)
{	
	if(config->timestamp) {
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
	
		strftime(buffer, 21, "\"%FT%T", tm);
		/* append miliseconds */
		sprintf(&(buffer[20]), ".%03u\"", (const unsigned int) msec);
	} else {
		tstamp = be64toh(tstamp);
		sprintf(buffer, "%" PRIu64 , tstamp);
	}	

	return buffer;
}

/**
 * \brief Conversion of unsigned int
 */
const char *Translator::toUnsigned(uint16_t *length, uint8_t *data_record,
	uint16_t offset, const ipfix_element_t * element, struct json_conf * config)
{
	if(*length == BYTE1) {
		// 1 byte
		if(element->en == 0 && element->id == 6 && config->tcpFlags) {
			// Formated TCP flags
			return formatFlags8(read8(data_record + offset));
		} else if (element->en == 0 && element->id == 4 && !config->protocol) {
			// Formated protocol identification (TCP, UDP, ICMP,...)
			return (formatProtocol(read8(data_record + offset)));
		} else {
			// Other elements
			snprintf(buffer, BUFF_SIZE, "%" PRIu8, read8(data_record + offset));
		}
	} else if(*length == BYTE2) {
		// 2 bytes
		if (element->en == 0 && element->id == 6 && config->tcpFlags) {
			// Formated TCP flags
			return formatFlags16(read16(data_record + offset));
		} else {
			// Other elements
			snprintf(buffer, BUFF_SIZE, "%" PRIu16, ntohs(read16(data_record + offset)));
		}
	} else if(*length == BYTE4) {
		// 4 bytes
		snprintf(buffer, BUFF_SIZE, "%" PRIu32, ntohl(read32(data_record + offset)));
	} else if(*length == BYTE8) {
		// 8 bytes
		snprintf(buffer, BUFF_SIZE, "%" PRIu64, be64toh(read64(data_record + offset)));
	} else {
		// Other sizes
		snprintf(buffer, BUFF_SIZE, "%s", "\"unknown\"");
	}

	return buffer;
}

/**
 * \brief Conversion of signed int
 */
const char *Translator::toSigned(uint16_t *length, uint8_t *data_record, uint16_t offset)
{
	if(*length == BYTE1) {
		// 1 byte
		snprintf(buffer, BUFF_SIZE, "%" PRId8, (int8_t) read8(data_record + offset));
	} else if(*length == BYTE2) {
		// 2 bytes
		snprintf(buffer, BUFF_SIZE, "%" PRId16, (int16_t) ntohs(read16(data_record + offset)));
	} else if(*length == BYTE4) {
		// 4 bytes
		snprintf(buffer, BUFF_SIZE, "%" PRId32, (int32_t) ntohl(read32(data_record + offset)));
	} else if(*length == BYTE8) {
		// 8 bytes
		snprintf(buffer, BUFF_SIZE, "%" PRId64, (int64_t) be64toh(read64(data_record + offset)));
	} else {
		snprintf(buffer, BUFF_SIZE, "\"unknown\"");
	}

	return buffer;
}

/**
 * \brief Conversion of float
 */
const char *Translator::toFloat(uint16_t *length, uint8_t *data_record, uint16_t offset)
{		
	if(*length == BYTE4)
		snprintf(buffer, BUFF_SIZE, "%f", (float) ntohl(read32(data_record + offset)));
	else if(*length == BYTE8)
		snprintf(buffer, BUFF_SIZE, "%lf", (double) be64toh(read64(data_record + offset)));
	else
		snprintf(buffer, BUFF_SIZE, "\"unknown\"");

	return buffer;
}

/**
 * \brief Convert string to JSON format
 */
const char *Translator::escapeString(uint16_t length, const uint8_t *field,
	const json_conf *config)
{
	uint32_t idx_output = 0;

	#define ESCAPE_CHAR(ch) { \
		buffer[idx_output++] = '\\'; \
		buffer[idx_output++] = ch; \
	}

	// Beginning of the string
	buffer[idx_output++] = '"';

	for (uint32_t i = 0; i < length; ++i) {
		// All characters from the extended part of ASCII must be escaped
		if (field[i] > 0x7F) {
			snprintf(&buffer[idx_output], 7, "\\u00%02x", field[i]);
			idx_output += 6;
			continue;
		}

		/*
		 * Based on RFC 4627 (Section: 2.5. Strings):
		 * Control characters (i.e. 0x00 - 0x1F), '"' and  '\' must be escaped
		 * using "\"", "\\" or "\uXXXX" where "XXXX" is a hexa value.
		 */
		if (field[i] > 0x1F && field[i] != '"' && field[i] != '\\') {
			// Copy to the output buffer
			buffer[idx_output++] = field[i];
			continue;
		}

		// Copy as escaped character
		switch(field[i]) {
		case '\\': // Reverse solidus
			ESCAPE_CHAR('\\');
			continue;
		case '\"': // Quotation
			ESCAPE_CHAR('\"');
			continue;
		default:
			break;
		}

		if (config->whiteSpaces == false) {
			// Skip white space characters
			continue;
		}

		switch(field[i]) {
		case '\t': // Tabulator
			ESCAPE_CHAR('t');
			break;
		case '\n': // New line
			ESCAPE_CHAR('n');
			break;
		case '\b': // Backspace
			ESCAPE_CHAR('b');
			break;
		case '\f': // Form feed
			ESCAPE_CHAR('f');
			break;
		case '\r': // Return
			ESCAPE_CHAR('r');
			break;
		default: // "\uXXXX"
			snprintf(&buffer[idx_output], 7, "\\u00%02x", field[i]);
			idx_output += 6;
			break;
		}
	}
	#undef ESCAPE_CHAR

	// End of the string
	buffer[idx_output++] = '"';
	buffer[idx_output] = '\0';
	return buffer;
}

