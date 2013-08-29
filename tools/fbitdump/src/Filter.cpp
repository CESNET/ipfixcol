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

namespace fbitdump
{

void Filter::error(const parser::location& loc, const std::string& msg)
{
	//std::cerr << "error at " << loc << ": " << s << std::endl;
	std::cerr << loc << ": " << msg << std::endl;
}

void Filter::error (const std::string& msg)
{
	std::cerr << msg << std::endl;
}


const std::string Filter::getFilter() const
{
	return this->filterString;
}

bool Filter::isValid(Cursor &cur) const
{
	// TODO add this functiononality
	return true;
}

void Filter::set_filterString(std::string newFilter)
{
	this->filterString = newFilter;
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

	this->filterString = "1 = 1";
	this->actualConf = &conf;

	/* Initialise lexer structure (buffers, etc) */
	yylex_init(&this->scaninfo);

	YY_BUFFER_STATE bp = yy_scan_string(input.c_str(), this->scaninfo);
	yy_switch_to_buffer(bp, this->scaninfo);

	/* create the parser, Filter is its param (it will provide scaninfo to the lexer) */
	parser::Parser parser(*this);

	/* run run parser */
	int ret = parser.parse();

	yy_flush_buffer(bp, this->scaninfo);
	yy_delete_buffer(bp, this->scaninfo);

	/* clear the context */
	yylex_destroy(this->scaninfo);

	std::cout << "Filtr: " << this->filterString << std::endl;
#ifdef DEBUG
	std::cerr << "Using filter: '" << filter << "'" << std::endl;
#endif
}

time_t Filter::parseTimestamp(std::string str) const throw (std::invalid_argument)
{
	struct tm ctime;

	if (strptime(str.c_str(), "%Y/%m/%d.%H:%M:%S", &ctime) == NULL) {
		throw std::invalid_argument(std::string("Cannot parse timestamp '") + str + "'");
	}

	return mktime(&ctime);
}

std::string Filter::parse_timestamp(std::string timestamp)
{
	time_t ntime = parseTimestamp(timestamp);

	std::ostringstream ss;
	ss << ntime * 1000;

	return ss.str();
}

std::string Filter::parse_ipv4(std::string addr)
{
	struct in_addr address;
	std::stringstream ss;

	inet_pton(AF_INET, addr.c_str(), &address);

	uint32_t addrNum = ntohl(address.s_addr);

	ss << addrNum;

	return ss.str();
}

std::string Filter::parse_ipv6(std::string addr)
{
	uint64_t address[2];

	inet_pton(AF_INET6, addr.c_str(), address);

	std::stringstream ss;
	std::string part1;
	std::string part2;

	ss << be64toh(address[0]);
	part1 = ss.str();

	ss.str(std::string());
	ss.clear();

	ss << be64toh(address[1]);
	part2 = ss.str();

	ss.str(std::string());
	ss.clear();

	ss << "part1 = " << part1 << " and part2 = " << part2;

	return ss.str();
}


std::string Filter::parse_number(std::string number)
{
	switch (number[number.length() - 1]) {
	case 'k':
	case 'K':
		return number.substr(0, number.length() - 1) + "000";
	case 'm':
	case 'M':
		return number.substr(0, number.length() - 1) + "000000";
	case 'g':
	case 'G':
		return number.substr(0, number.length() - 1) + "000000000";
	case 't':
	case 'T':
		return number.substr(0, number.length() - 1) + "000000000000";
	default:
		return number;
	}
}

std::string Filter::parse_column(std::string strcol)
{
	Column *col = NULL;
	try {
		col = new Column(this->actualConf->getXMLConfiguration(), strcol, false);
	} catch (std::exception &e){
		std::string err = std::string("Filter column '") + strcol + "' not found!";
		throw std::invalid_argument(err);
	}
	stringSet cols = col->getColumns();
	if (!col->isOperation()) {
		return *cols.begin();
	} else {
		std::string err = std::string("Computed column '") + strcol + "' cannot be used for filtering!";
		delete col;
		throw std::invalid_argument(err);
	}
	delete col;
	return std::string("");
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
