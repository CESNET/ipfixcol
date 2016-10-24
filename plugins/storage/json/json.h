/**
 * \file json.h
 * \author Michal Kozubik <kozubik@cesnet.cz>
 * \brief Header for JSON storage plugin
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


#ifndef JSON_H
#define JSON_H

extern "C" {
#include <ipfixcol/verbose.h>
}

#include <string>
#include "pugixml/pugixml.hpp"

// Class prototype
class Storage;

/**
 * \brief JSON plugin configuration
 */
struct json_conf {
	bool metadata;
	Storage *storage;
	bool tcpFlags;       /**< TCP flags format - true(formatted), false(RAW)  */
	bool timestamp;      /**< timestamp format - true(formatted), false(UNIX) */
	bool protocol;       /**< protocol format  - true(RAW), false(formatted)  */
	bool ignoreUnknown;  /**< Ignore unknown elements                        */
	bool whiteSpaces;    /**< Convert white spaces in strings (do not skip)  */
};

class Output
{
public:
	Output() {}
	Output(const pugi::xpath_node& config);
	virtual ~Output() {}

	virtual void ProcessDataRecord(const std::string& record) = 0;
};

#endif // JSON_H

