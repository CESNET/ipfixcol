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

#ifndef TRANSLATOR_H
#define	TRANSLATOR_H

#include <string>
#include <vector>

#define BUFF_SIZE 128

enum class t_units {
    SEC,
    MILLISEC,
    MICROSEC,
    NANOSEC
};

class Translator {
public:
    
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
	const char *formatTimestamp(uint64_t tstamp, t_units units);
    
    /**
     * \brief Format protocol
     * 
     * @param proto protocol
     * @return formatted protocol
     */
	const char *formatProtocol(uint8_t proto) const;
    
    /**
     * \brief Format TCP flags
     * 
     * @param flags
     * @return formatted flags
     */
	const char *formatFlags(uint16_t flags);
    

private:

	char buffer[BUFF_SIZE];

	struct tm *tm{};
	time_t timesec;
	uint64_t msec;
};

#endif	/* TRANSLATOR_H */

