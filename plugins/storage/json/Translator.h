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

#ifndef TRANSLATOR_H
#define	TRANSLATOR_H

#include <string>
#include <vector>
//#include "Storage.h"

extern "C" {
#include <ipfixcol.h>
#include <ipfixcol/profiles.h>
#include <string.h>
//#include <ipfix_element.h>
}

/* size of bytes 1, 2, 4 or 8 */
#define BYTE1 1
#define BYTE2 2
#define BYTE4 4
#define BYTE8 8

class Storage;

enum class t_units {
    SEC,
    MILLISEC,
    MICROSEC,
    NANOSEC
};

class Translator {
public:
	/** Constructor */
	Translator();

	/** Destructor */
	~Translator();

    /**
     * \brief Format IPv4 address into dotted format
     * 
     * @param addr address
     * @return formatted address
     */
	const char *formatIPv4(uint32_t addr);

    /**
     * \brief Format IPv6 address
     * 
     * @param addr address
     * @return formatted address
     */
	const char *formatIPv6(uint8_t *addr);
    
    /**
     * \brief Format MAC address
     * 
     * @param addr address
     * @return formatted address
     */
	const char *formatMac(uint8_t *addr);
    
    /**
     * \brief Format timestamp
     * 
     * @param tstamp timestamp
     * @param units time units
     * @return  formatted timestamp
     */
	const char *formatTimestamp(uint64_t tstamp, t_units units, struct json_conf * config);
    
    /**
     * \brief Format protocol
     * 
     * @param proto protocol
     * @return formatted protocol
     */
	const char *formatProtocol(uint8_t proto); 
    
    /**
     * \brief Format TCP flags 16bits
     * 
     * @param flags
     * @return formatted flags
     */
	const char *formatFlags16(uint16_t flags);

    /**
     * \brief Format TCP flags 8bits
     *
     * @param flags
     * @return formatted flags
     */
	const char *formatFlags8(uint8_t flags);

    /**
     * \brief Checks, if real length of record is the same as its data type. If not, converts to real length.
     *
     * @param length length of record
     * @param data_record pointer to head of data record
     * @param offset offset since head of data record
     * @param element pointer to actuall element in record
     * @param config pointer to configuration structure
     * @return value of Unsigned int
     */
	const char *toUnsigned(uint16_t *length, uint8_t *data_record, uint16_t offset, const ipfix_element_t * element, struct json_conf * config);

    /**
     * \brief Checks, if real length of record is the same as its data type. If not, converts to real length.
     *
     * @param length length of record
     * @param data_record pointer to head of data record
     * @param offset offset since head of data record
     * @return value of Signed int
     */
	const char *toSigned(uint16_t *length, uint8_t *data_record, uint16_t offset);

    /**
     * \brief Checks, if real length of record is the same as its data type. If not, converts to real length.
     *
     * @param length length of record
     * @param data_record pointer to head of data record
     * @param offset offset since head of data record
     * @return value of Float
     */
	const char *toFloat(uint16_t *length, uint8_t *data_record, uint16_t offset);

	/**
	 * \brief Convert string to JSON format
	 *
	 * Escape non-printable characters and replace them.
	 * @param length Length of the field
	 * @param field Pointer to the field
	 * @param config Plugin configuration
	 * @return
	 */
	const char *escapeString(uint16_t length, const uint8_t *field,
		const struct json_conf *config);

private:
	/**
	 * \brief Size of internal buffer for data conversion
	 *
	 * This buffer is used by all conversion functions for storing string
	 * outputs. At worst Translator::escapeString() have to convert an
	 * IPFIX record (max size of IPFIX message: 65536) of a string record that
	 * contains only non-printable characters that will be replaced with
	 * string "\uXXXX" (6) each.
	 */
	static const int BUFF_SIZE = (65536 * 6);

	/** Buffer for JSON conversion */
	char *buffer;

	struct tm *tm{};
	time_t timesec{};
	uint64_t msec{};
};

#endif	/* TRANSLATOR_H */

