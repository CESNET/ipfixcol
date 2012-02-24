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

namespace fbitdump
{

const std::string Filter::getFilter() const
{
	return this->filterString;
}

bool Filter::isValid(Cursor &cur) const
{
	// TODO add this functiononality
	return true;
}

void Filter::init(Configuration &conf) throw (std::invalid_argument)
{
	std::string input = conf.getFilter(), filter, tw;

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
	int c;
	std::string arg;
	Column *col = NULL;

	bp = yy_scan_string(input.c_str());
	yy_switch_to_buffer(bp);

	try {
		while ((c = yylex(arg)) != 0) {
			switch (c) {
			case COLUMN: {
				/* get real columns */
				try {
					col = new Column(conf.getXMLConfiguration(), arg, false);
				} catch (std::exception &e) {
					/* rethrow the exception */
					std::string err = std::string("Filter column '") + arg + "' not found!";
					throw std::invalid_argument(err);
				}
				stringSet cols = col->getColumns();
				if (!col->isOperation()) { /* plain value */
					filter += *cols.begin() + " ";
				} else { /* operation */
					/* \TODO  save for post-filtering */
					std::string err = std::string("Computed column '") + arg + "' cannot be used for filtering.";
					/* delete column before throwing an exception */
					delete col;
					throw std::invalid_argument(err);
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

					std::ostringstream ss;
					ss << ntime*1000; /* \TODO use ms only for miliseconds timetamp columns */

					filter += ss.str() + " ";

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
					throw std::invalid_argument(std::string("Wrong filter string: '") + arg + "'");
					break;
				default:
					filter += arg + " ";
					break;
			}
		}

	} catch (std::invalid_argument &e) {
		/* free flex allocated resources */
		yy_flush_buffer(bp);
		yy_delete_buffer(bp);
		yylex_destroy();

		/* send the exception up */
		throw;
	}

	/* free flex allocated resources */
	yy_flush_buffer(bp);
	yy_delete_buffer(bp);
	yylex_destroy();

#ifdef DEBUG
	std::cerr << "Using filter: '" << filter << "'" << std::endl;
#endif
	this->filterString = filter;
}

time_t Filter::parseTimestamp(std::string str) const throw (std::invalid_argument)
{
	struct tm ctime;

	if (strptime(str.c_str(), "%Y/%m/%d.%H:%M:%S", &ctime) == NULL) {
		throw std::invalid_argument(std::string("Cannot parse timestamp '") + str + "'");
	}

	return mktime(&ctime);
}

Filter::Filter(Configuration &conf) throw (std::invalid_argument)
{
	init(conf);
}

Filter::Filter()
{
	this->filterString = "1 = 1";
}

} /* end of namespace fbitdump */
