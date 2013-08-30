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

void Filter::setFilterString(std::string newFilter)
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

	/* We need access to configuration in parseColumn() function */
	this->actualConf = &conf;

	if (conf.getFilter().compare("1=1") == 0) {
		this->setFilterString("1 = 1");
	} else {
		/* Initialise lexer structure (buffers, etc) */
		yylex_init(&this->scaninfo);

		YY_BUFFER_STATE bp = yy_scan_string(input.c_str(), this->scaninfo);
		yy_switch_to_buffer(bp, this->scaninfo);

		/* create the parser, Filter is its param (it will provide scaninfo to the lexer) */
		parser::Parser parser(*this);

		/* run run parser */
		if (parser.parse() != 0) {
			std::cerr << "Error while parsing filter, using default \"1 = 1\"\n";
			this->filterString = "1 = 1";
		}

		yy_flush_buffer(bp, this->scaninfo);
		yy_delete_buffer(bp, this->scaninfo);

		/* clear the context */
		yylex_destroy(this->scaninfo);
	}

	std::cout << "\nFilter: " << this->filterString << std::endl;
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

void Filter::parseTimestamp(struct _parserStruct *ps, std::string timestamp)
{
	if (ps == NULL) {
		return;
	}
	std::ostringstream ss;

	/* Get time in seconds */
	time_t ntime = parseTimestamp(timestamp);

	/* Sec -> ms -> string */
	ss << ntime * 1000;

	/* Set right values of parser structure */
	ps->type = PT_TIMESTAMP;
	ps->nParts = 1;
	ps->parts.push_back(ss.str());
}

void Filter::parseIPv4(struct _parserStruct *ps, std::string addr)
{
	if (ps == NULL) {
		return;
	}

	/* Parse IPv4 address */
	struct in_addr address;
	std::stringstream ss;

	/* Convert from address format into numeric fotmat */
	inet_pton(AF_INET, addr.c_str(), &address);

	/* Get it in host byte order */
	uint32_t addrNum = ntohl(address.s_addr);

	/* Integer to string */
	ss << addrNum;

	/* Set right values of parser structure */
	ps->type = PT_IPv4;
	ps->nParts = 1;
	ps->parts.push_back(ss.str());
}

void Filter::parseIPv6(struct _parserStruct *ps, std::string addr)
{
	if (ps == NULL) {

	}

	/* Parse IPv6 address */
	uint64_t address[2];
	std::stringstream ss;

	/* Convert from address format into numeric format (we need 128b) */
	inet_pton(AF_INET6, addr.c_str(), address);

	/* Save first part of address into parser structure string vector */
	ss << be64toh(address[0]);
	ps->parts.push_back(ss.str());

	/* Clear stringstream */
	ss.str(std::string());
	ss.clear();

	/* Save second part */
	ss << be64toh(address[1]);
	ps->parts.push_back(ss.str());

	/* Set right values of parser structure */
	ps->type = PT_IPv6;
	ps->nParts = 2;
}

void Filter::parseNumber(struct _parserStruct *ps, std::string number)
{
	if (ps == NULL) {
		return;
	}

	/* If there is some suffix (kKmMgG) convert it into number */
	switch (number[number.length() - 1]) {
	case 'k':
	case 'K':
		number = number.substr(0, number.length() - 1) + "000";
		break;
	case 'm':
	case 'M':
		number = number.substr(0, number.length() - 1) + "000000";
		break;
	case 'g':
	case 'G':
		number = number.substr(0, number.length() - 1) + "000000000";
		break;
	case 't':
	case 'T':
		number = number.substr(0, number.length() - 1) + "000000000000";
		break;
	default:
		break;
	}

	/* Set right values of parser structure */
	ps->type = PT_NUMBER;
	ps->nParts = 1;
	ps->parts.push_back(number);
}

bool Filter::parseColumnGroup(struct _parserStruct *ps, std::string alias, bool aggeregate)
{
	if (ps == NULL) {
		return false;
	}

	/* search xml for a group alias */
	pugi::xpath_node column = this->actualConf->getXMLConfiguration().select_single_node(("/configuration/groups/group[alias='"+alias+"']").c_str());

	/* check what we found */
	if (column == NULL) {
		std::cerr << "Column group '" << alias << "' not defined";
		return false;
	}

	/* Check if "members" node exists */
	pugi::xml_node members = column.node().child("members");
	if (members == NULL) {
		std::cerr << "Wrong XML file, no \"members\" child in group " << alias << "!\n";
		return false;
	}

	/* Get members aliases and parse them */
	for (pugi::xml_node_iterator it = members.begin(); it != members.end(); it++) {
		this->parseColumn(ps, it->child_value());
	}

	/* Set type of structure */
	ps->type = PT_GROUP;
	return true;
}

void Filter::parseColumn(struct _parserStruct *ps, std::string strcol)
{
	if (ps == NULL) {
		return;
	}

	/* Get right column (find entered alias in xml file) */
	Column *col = NULL;
	try {
		col = new Column(this->actualConf->getXMLConfiguration(), strcol, false);
	} catch (std::exception &e){
		/* If column not found, check column groups */
		if (this->parseColumnGroup(ps, strcol, false) == false) {
			/* No column, no column group, error */
			std::string err = std::string("Filter column '") + strcol + "' not found!";
			throw std::invalid_argument(err);
		}
		return;
	}

	/* Save all its parts into string vector of parser structure */
	stringSet cols = col->getColumns();
	if (!col->isOperation()) {
		/* Iterate through all aliases */
		for (stringSet::iterator ii = cols.begin(); ii != cols.end(); ii++) {
			ps->parts.push_back(*ii);
			ps->nParts++;
		}
	} else {
		std::string err = std::string("Computed column '") + strcol + "' cannot be used for filtering!";
		delete col;
		throw std::invalid_argument(err);
	}
	delete col;

	/* Set type of structure */
	ps->type = PT_COLUMN;
}

void Filter::parseRawcolumn(struct _parserStruct *ps, std::string strcol)
{
	if (ps == NULL) {
		return;
	}
	ps->nParts = 1;
	ps->type = PT_RAWCOLUMN;
	ps->parts.push_back(strcol);
}

void Filter::parseBitColVal(struct _parserStruct *ps, struct _parserStruct *left, std::string op, struct _parserStruct *right)
{
	if ((ps == NULL) || (left == NULL) || (right == NULL)) {
		return;
	}
	/* Parse expression "column BITOPERATOR value" */

	/* Set type of structure */
	ps->type = PT_BITCOLVAL;
	ps->nParts = 0;

	/* Iterate through all parts */
	for (uint16_t i = 0, j = 0; (i < left->nParts) || (j < right->nParts); i++, j++) {
		/* If one string vector is at the end but second not, duplicate its last value */
		if (i == left->nParts) {
			i--;
		}
		if (j == right->nParts) {
			j--;
		}

		/* Create new expression and save it into parser structure */
		ps->nParts++;
		ps->parts.push_back(
				std::string("( " + left->parts[i] + " " + op + " " + right->parts[j] + " ) "));
	}
}

std::string Filter::parseExp(struct _parserStruct *left, std::string cmp, struct _parserStruct *right)
{
	if ((left == NULL) || (right == NULL)) {
		return "";
	}

	/* Parser expression "column CMP value" */
	std::string exp;
	std::string op;

	if ((left->nParts == 1) && (right->nParts == 1)) {
		exp += left->parts[0] + " " + cmp + " " + right->parts[0] + " ";
	} else {
		exp += "(";

		/* Set operator */
		if (left->type == PT_GROUP) {
			op = "or ";
		} else {
			op = "and ";
		}

		/* Create expression */
		for (uint16_t i = 0, j = 0; (i < left->nParts) || (j < right->nParts); i++, j++) {
			/* If one string vector is at the end but second not, duplicate its last value */
			if (i == left->nParts) {
				i--;
			}
			if (j == right->nParts) {
				j--;
			}

			/* Add string into stringstream */
			exp += "( " + left->parts[i] + " " + cmp + " " + right->parts[j] + " ) " + op;
		}
		/* Remove last operator and close bracket */
		exp = exp.substr(0, exp.length() - op.length() - 1) + ") ";
	}
	/* Return created expression */
	return exp;
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
