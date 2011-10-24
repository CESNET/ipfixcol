/**
 * \file Filter.cpp
 * \author Petr Velan <petr.velan@cesnet.cz>
 * \brief Class for management of result filtering
 *
 * Copyright (C) 2011 CESNET, z.s.p.o.
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
#include <arpa/inet.h>
#include <time.h>

#include "Filter.h"
#include "Configuration.h"
#include "Column.h"
#include "typedefs.h"
#include "scanner.h"

/* yylex is not in the header, declare it separately */
extern YY_DECL;

namespace ipfixdump
{

std::string Filter::getFilter()
{
	return this->filterString;
}

bool Filter::isValid(Cursor &cur)
{
	// TODO add this functionality
	return true;
}

int Filter::init()
{
	std::string input = this->conf.getFilter(), filter, tw;

	/* incorporate time windows argument in filter */
	if (!conf.getTimeWindowStart().empty()) {
		tw = "(%ts >= " + conf.getTimeWindowStart();
		if (!conf.getTimeWindowEnd().empty()) {
			tw += "AND %te <= " + conf.getTimeWindowEnd();
		}
		tw += ") AND ";
	input = tw + input;
	}

	YY_BUFFER_STATE bp;
	int c, ret = 0;
	std::string arg;

	bp = yy_scan_string(input.c_str());
	yy_switch_to_buffer(bp);

	/* Open XML configuration file */
	pugi::xml_document doc;
	doc.load_file(conf.getXmlConfPath());

	while ((c = yylex(arg)) != 0) {
		switch (c) {
		case COLUMN: {
			/* get real columns */
			Column *col = new Column();
			if (col->init(doc, arg, false)) {
				stringSet cols = col->getColumns();
				if (!col->isOperation()) { /* plain value */
					filter += *cols.begin() + " ";
				} else { /* operation */
					/* \TODO  save for post-filtering */
					std::cerr << "Computed column '" << arg << "' cannot be used for filtering." << std::endl;
					ret = -1;
				}
			} else {
				std::cerr << "Filter column '" << arg << "' not found!" << std::endl;
				ret = -2;
			}
			delete col;
			break;}
		case IPv4:{
			/* convert ipv4 address to uint32_t */
			struct in_addr addr;
			std::stringstream ss;
			/* convert ipv4 address, returns network byte order */
			inet_pton(AF_INET, arg.c_str(), &addr);
			/* convert to host byte order */
			uint32_t addrNum = ntohl(addr.s_addr);
			/* integer to string */
			ss << addrNum;
			filter += ss.str() + " ";
			break;}
		case NUMBER:
			switch (arg[arg.length()-1]) {
			case 'k':
			case 'K':
				filter += arg.substr(0, arg.length()-1) + "000";
				break;
			case 'm':
			case 'M':
				filter += arg.substr(0, arg.length()-1) + "000000";
				break;
			case 'g':
			case 'G':
				filter += arg.substr(0, arg.length()-1) + "000000000";
				break;
			case 't':
			case 'T':
				filter += arg.substr(0, arg.length()-1) + "000000000000";
				break;
			default:
				filter += arg;
				break;
			}
			filter += " ";
			break;
			case TIMESTAMP: {
				time_t ntime = parseTimestamp(arg);
				if (ntime == -1) { /* parsing error */
					ret = -3;
				} else {
					std::ostringstream ss;
					ss << ntime*1000; /* \TODO use ms only for miliseconds timetamp columns */

					filter += ss.str() + " ";
				}

				break;}
			/*		case RAWCOLUMN:
				std::cout << "raw column: ";
				break;
			case OPERATOR:
				std::cout << "operator: ";
				break;
			case CMP:
				std::cout << "comparison: ";
				break;*/
			case BRACKET:
				filter += arg + " ";
				break;
			case OTHER:
				std::cerr << "Wrong filter string: '"<< arg << "'" << std::endl;
				ret = -3;
				break;
			default:
				filter += arg + " ";
				break;
		}
	}

	/* free flex allocated resources */
	yy_flush_buffer(bp);
	yy_delete_buffer(bp);
	yylex_destroy();

	/* Don't show debug message with filter on error */
	if (ret != 0) {
		return ret;
	}

#ifdef DEBUG
	std::cerr << "Using filter: '" << filter << "'" << std::endl;
#endif
	this->filterString = filter;

	return ret;
}

time_t Filter::parseTimestamp(std::string str)
{
	struct tm ctime;

	if (strptime(str.c_str(), "%Y/%m/%d.%H:%M:%S", &ctime) == NULL) {
		std::cerr << "Cannot parse timestamp '" << str << "'" << std::endl;
		return -1;
	}

	return mktime(&ctime);
}

Filter::Filter(Configuration &conf): conf(conf) {}

} /* end of namespace ipfixdump */
