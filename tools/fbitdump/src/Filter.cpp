/**
 * \file Filter.cpp
 * \author Petr Velan <petr.velan@cesnet.cz>
 * \brief Class for management of result filtering
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
#include <arpa/inet.h>
#include <time.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdlib.h>

#include "Filter.h"
#include "Configuration.h"
#include "Column.h"
#include "typedefs.h"
#include "scanner.h"
#include "protocols.h"
#include "Verbose.h"

namespace fbitdump
{

void Filter::error(const parser::location& loc, const std::string& msg)
{
	std::cerr << "error at " << loc << ": " << msg << std::endl;
}

void Filter::error(const std::string& msg)
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
	(void) cur;
	return true;
}

void Filter::setFilterString(std::string newFilter)
{
	this->filterString = newFilter;
}

void Filter::init(Configuration &conf) throw (std::invalid_argument)
{
	std::string input = conf.getFilter(), tw;
	
	if (input == "1=1") {
		input.clear();
	}

	/* incorporate time windows argument in filter */
	if (!conf.getTimeWindowStart().empty()) {
		tw = "(%ts >= " + conf.getTimeWindowStart();
		if (!conf.getTimeWindowEnd().empty()) {
			tw += " AND %te <= " + conf.getTimeWindowEnd();
		}
		tw += ")";
		if (input.empty()) {
			input = tw;
		} else {
			input = tw + " AND (" + input + ")";
		}
	}

	/* We need access to configuration in parseColumn() function */
	this->actualConf = &conf;

	if (input.empty()) {
		this->setFilterString("1 = 1");
	} else {
		/* Initialise lexer structure (buffers, etc) */
		yylex_init(&this->scaninfo);

		YY_BUFFER_STATE bp = yy_scan_string(input.c_str(), this->scaninfo);
		yy_switch_to_buffer(bp, this->scaninfo);

		/* create the parser, Filter is its param (it will provide scaninfo to the lexer) */
		parser::Parser parser(*this);

		/* run parser */
		if (parser.parse() != 0) {
			yylex_destroy(this->scaninfo);
			throw std::invalid_argument(std::string("Error while parsing filter (" + input + ")"));
		}

		yy_flush_buffer(bp, this->scaninfo);
		yy_delete_buffer(bp, this->scaninfo);

		/* clear the context */
		yylex_destroy(this->scaninfo);
	}
	MSG_FILTER("Filter", "%s", this->filterString.c_str());

}

time_t Filter::parseTimestamp(std::string str) const throw (std::invalid_argument)
{
	struct tm ctime;

	if (strptime(str.c_str(), "%Y/%m/%d.%H:%M:%S", &ctime) == NULL) {
		throw std::invalid_argument(std::string("Cannot parse timestamp '") + str + "'");
	}

	ctime.tm_isdst = -1;
	return timelocal(&ctime);
}

void Filter::parseTimestamp(parserStruct *ps, std::string timestamp) const throw (std::invalid_argument)
{
	if (ps == NULL) {
		throw std::invalid_argument(std::string("Cannot parse timestamp '") + timestamp + "'");
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

/* private */
std::string Filter::parseIPv4(std::string addr) const
{
	uint32_t address;
	std::stringstream ss;

	/* Convert from address format into numeric fotmat */
	inet_pton(AF_INET, addr.c_str(), &address);

	/* Swap byte order and convert address from int to string */
	ss << ntohl(address);

	return ss.str();
}

void Filter::parseIPv4(parserStruct *ps, std::string addr) const throw (std::invalid_argument)
{
	if (ps == NULL) {
		throw std::invalid_argument(std::string("Cannot parse IPv4 address, NULL parser structure"));
	}

	/* Set right values of parser structure */
	ps->type = PT_IPv4;
	ps->nParts = 1;
	ps->parts.push_back(parseIPv4(addr));
}

void Filter::parseIPv4Sub(parserStruct *ps, std::string addr) const throw (std::invalid_argument)
{
	if (ps == NULL) {
		throw std::invalid_argument(std::string("Cannot parse IPv4 address (with subnet), NULL parser structure"));
	}

	uint8_t subnetPos;
	uint16_t subnet;
	uint32_t min, max, addrInt;
	std::stringstream ss;
	struct in_addr subnetIP;

	/* Get subnet number */
	subnetPos = addr.find('/') + 1;
	subnet = std::atoi(addr.substr(subnetPos, addr.length() - 1).c_str());

	/* Get mask IP */
	subnetIP.s_addr = ~0;
	subnetIP.s_addr <<= 32 - subnet;

	/* Parse IP address without subnet */
	addrInt = atoi(this->parseIPv4(addr.substr(0, subnetPos - 1)).c_str());

	/* Calculate minimal and maximal host address */
	min = addrInt & subnetIP.s_addr;
	max = min | (~subnetIP.s_addr);

	ss << min;
	ps->parts.push_back(ss.str());

	ss.str(std::string());
	ss.clear();

	ss << max;
	ps->parts.push_back(ss.str());

	/* Fill in parser structure */
	ps->nParts = 2;
	ps->type = PT_IPv4_SUB;
}

/* private */
void Filter::parseIPv6(std::string addr, std::string& part1, std::string& part2) const
{
	uint64_t address[2];
	std::stringstream ss;

	/* Convert from address format into numeric format (we need 128b) */
	inet_pton(AF_INET6, addr.c_str(), address);

	/* Save first part of address into parser structure string vector */
	ss << be64toh(address[0]);
	/* IPv6 is stored as unsigned long */
	ss << "UL";
	part1 = ss.str();

	/* Clear stringstream */
	ss.str(std::string());
	ss.clear();

	/* Save second part */
	ss << be64toh(address[1]);
	/* IPv6 is stored as unsigned long */
	ss << "UL";
	part2 = ss.str();
}

void Filter::parseIPv6(parserStruct *ps, std::string addr) const throw (std::invalid_argument)
{
	if (ps == NULL) {
		throw std::invalid_argument(std::string("Cannot parse IPv6 address, NULL parser structure"));
	}

	std::string part1, part2;

	/* Parse IPv6 address */
	parseIPv6(addr, part1, part2);

	ps->parts.push_back(part1);
	ps->parts.push_back(part2);

	/* Set right values of parser structure */
	ps->type = PT_IPv6;
	ps->nParts = 2;
}

void Filter::parseIPv6Sub(parserStruct *ps, std::string addr) const throw (std::invalid_argument)
{
	if (ps == NULL) {
		throw std::invalid_argument(std::string("Cannot parse IPv6 address (with subnet), NULL parser structure"));
	}

	uint8_t subnetPos, i;
	uint16_t subnet;
	uint64_t min[2], max[2], subnetIP[2];
	std::string part[2];
	std::stringstream ss;

	/* Get subnet number */
	subnetPos = addr.find('/') + 1;
	subnet = std::atoi(addr.substr(subnetPos, addr.length() - 1).c_str());

	/* Set subnet address */
	subnetIP[0] = ~0;
	subnetIP[1] = ~0;

	if (subnet > 64) {
		subnetIP[1] <<= 64 - (subnet - 64);
	} else {
		subnetIP[1] = 0;
		subnetIP[0] <<= 64 - subnet;
	}

	/* Parse IP address to calculate minimal and maximal host address */
	this->parseIPv6(addr.substr(0, subnetPos - 1), part[0], part[1]);

	/* Calculate minimal and maximal host address for both parts */
	for (i = 0; i < 2; i++) {
		min[i] = atol(part[i].c_str()) & subnetIP[i];
		max[i] = min[i] | (~subnetIP[i]);

		ss.str(std::string());
		ss.clear();

		ss << min[i];
		ps->parts.push_back(ss.str());

		ss.str(std::string());
		ss.clear();

		ss << max[i];
		ps->parts.push_back(ss.str());
	}

	/* Fill in parser structure */
	ps->nParts = 4;
	ps->type = PT_IPv6_SUB;
}



void Filter::parseHostname(parserStruct *ps, uint8_t af_type) const throw (std::invalid_argument)
{
	if (ps == NULL) {
		throw std::invalid_argument(std::string("Cannot parse hostname, NULL parser structure"));
	}
	int ret;
	struct addrinfo *result, *tmp;
	struct addrinfo hints;
	std::string address, part1, part2, last1, last2;
	void *addr;

	memset(&hints, 0, sizeof (hints));

	/* Set input structure values */
	hints.ai_family = af_type;
	hints.ai_protocol = 0;

	/* Get all addresses according to hostname */
	ret = getaddrinfo(ps->parts[0].c_str(), "domain", &hints, &result);

	if (ret != 0) {
		throw std::invalid_argument(std::string("Unable to resolve address " + ps->parts[0]));
	}

	/* Erase parts */
	ps->parts.pop_back();
	ps->nParts = 0;

	/* Iterate through all addresses and save them into parser structure */
	for (tmp = result; tmp != NULL; tmp = tmp->ai_next) {
		if (af_type == AF_INET) {
			/* IPv4 */
			/* Get numeric address */
			struct sockaddr_in *ipv4 = (struct sockaddr_in *) tmp->ai_addr;
			addr = &(ipv4->sin_addr);

			/* Int to string */
			std::stringstream ss;
			ss << ntohl(*((uint32_t *) addr));
			address = ss.str();

			/* Each address is contained twice (don't know why) - check if previous addr is the same */
			if (address != last1) {
				/* Save address */
				ps->parts.push_back(address);
				ps->nParts++;
			}

			last1 = address;
		} else {
			/* IPv6 */
			/* Get numeric address */
			struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *) tmp->ai_addr;
			addr = &(ipv6->sin6_addr);

			/* Int to str both parts of address */
			std::stringstream ss;
			ss << be64toh(*((uint64_t *) addr));
			part1 = ss.str();

			ss.str(std::string());
			ss.clear();

			ss << be64toh(*(((uint64_t *) addr) + 1));
			part2 = ss.str();

			if ((part1 != last1) || (part2 != last2)) {
				/* Save address */
				ps->parts.push_back(part1);
				ps->parts.push_back(part2);
				ps->nParts += 2;
			}

			last1 = part1;
			last2 = part2;
		}
		tmp = tmp->ai_next;
	}
	freeaddrinfo(result);

	ps->type = (af_type == AF_INET) ? PT_HOSTNAME : PT_HOSTNAME6;
}


void Filter::parseNumber(parserStruct *ps, std::string number) const throw (std::invalid_argument)
{
	if (ps == NULL) {
		throw std::invalid_argument(std::string("Cannot parse number, NULL parser structure"));
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

void Filter::parseHex(parserStruct *ps, std::string number) const throw (std::invalid_argument)
{
	if (ps == NULL) {
		throw std::invalid_argument(std::string("Cannot parse hexa number, NULL parser structure"));
	}

	std::stringstream ss;
	ss << strtol(number.c_str(), NULL, 16);

	/* Set right values of parser structure */
	ps->type = PT_NUMBER;
	ps->nParts = 1;
	ps->parts.push_back(ss.str());
}

void Filter::parseFloat(parserStruct* ps, std::string number) const
{
	float num;
	
	switch (number[number.length() - 1]) {
	case 'k': case 'K':
		num = atof(number.substr(0, number.length() - 1).c_str()) * 1000.0;
		break;
	case 'm': case 'M':
		num = atof(number.substr(0, number.length() - 1).c_str()) * 1000000.0;
		break;
	case 'g': case 'G':
		num = atof(number.substr(0, number.length() - 1).c_str()) * 1000000000.0;
		break;
	case 't': case 'T':
		num = atof(number.substr(0, number.length() - 1).c_str()) * 1000000000000.0;
		break;
	default:
		num = atof(number.c_str());
	}
	
	std::stringstream ss;
	ss << num;
	
	ps->type = PT_NUMBER;
	ps->nParts = 1;
	ps->parts.push_back(ss.str());
}

bool Filter::parseColumnGroup(parserStruct *ps, std::string alias, bool aggregate) const
{
	(void) aggregate;

	if (ps == NULL) {
		return false;
	}

	/* search xml for a group alias */
	pugi::xpath_node column = this->actualConf->getXMLConfiguration().select_single_node(("/configuration/groups/group[alias='" + alias + "']").c_str());

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

void Filter::parseColumn(parserStruct *ps, std::string alias) const
{
	if (ps == NULL) {
		throw std::invalid_argument(std::string("Cannot parse column, NULL parser structure"));
	}

	/* Get right column (find entered alias in xml file) */
	Column *col = NULL;
	try {
		col = new Column(this->actualConf->getXMLConfiguration(), alias, false);
	} catch (std::exception &e) {
		/* If column not found, check column groups */
		if (this->parseColumnGroup(ps, alias, false) == false) {
			/* No column, no column group, error */
			std::string err = std::string("Filter column '") + alias + "' not found!";
			throw std::invalid_argument(err);
		}
		return;
	}

	/* Save all its parts into string vector of parser structure */
	stringSet cols = col->getColumns();
	if (!col->isOperation()) {
		/* Iterate through all aliases */
		for (stringSet::iterator it = cols.begin(); it != cols.end(); it++) {
			/* Check for flows column */
			if (col->getSemantics() == "flows") {
				delete col;
				std::string err = "Filtering by flows (" + alias + ") not supported!";
				throw std::invalid_argument(err);
			}
			ps->parts.push_back(*it);
			ps->nParts++;
		}
		ps->type = PT_COLUMN;
	} else {
		ps->parts.push_back(col->getElement());
		ps->nParts = 1;
		ps->baseCols = col->getColumns();
		ps->type = PT_COMPUTED;
	}
	if (this->actualConf->plugins.find(col->getSemantics()) != this->actualConf->plugins.end()) {
		ps->parse = this->actualConf->plugins[col->getSemantics()].parse;
		if (this->actualConf->plugins[col->getSemantics()].init) {
			this->actualConf->plugins[col->getSemantics()].init(col->getSemanticsParams().c_str(), &(ps->parseConf));
		}
	} else {
		ps->parse = NULL;
	}

	ps->colType = col->getSemantics();
	/* add parse params from column object */
	delete col;

	/* Set type of structure */

}

void Filter::parseRawcolumn(parserStruct *ps, std::string colname) const
{
	if (ps == NULL) {
		throw std::invalid_argument(std::string("Cannot parse raw column, NULL parser structure"));
	}

	ps->nParts = 1;
	ps->type = PT_RAWCOLUMN;
	ps->parts.push_back(colname);
}

void Filter::parseBitColVal(parserStruct *ps, parserStruct *left, std::string op, parserStruct *right) const throw (std::invalid_argument)
{
	if ((ps == NULL) || (left == NULL) || (right == NULL)) {
		throw std::invalid_argument(std::string("Cannot parse column with bit operator, NULL parser structure"));
	}
	/* Parse expression "column BITOPERATOR value" */

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

		/* Parse value if necessary */
		if (left->parse) {
			parsePlugin(left, right);
		}

		/* Create new expression and save it into parser structure */
		ps->nParts++;
		ps->parts.push_back(
				std::string("( " + left->parts[i] + " " + op + " " + right->parts[j] + " ) "));
	}

	/* Set type of structure */
	ps->type = PT_BITCOLVAL;

	/* Copy the parse function so that the value can also be parsed */
	if (left->parse) {
		ps->parse = left->parse;
	}
}

void Filter::parsePlugin(parserStruct *left, parserStruct *right) const throw (std::invalid_argument)
{
	char buff[PLUGIN_BUFFER_SIZE];
	left->parse((char *) right->parts[0].c_str(), buff, left->parseConf);
	if (strlen(buff) > 0) {
		right->parts[0] = std::string(buff);
		right->type = PT_NUMBER;
	} else {
		/* Wrong filter, end processing */
		throw std::invalid_argument(std::string("Cannot parse '" + right->parts[0] + "' as " + left->colType));
	}
}

std::string Filter::parseExp(parserStruct *left, std::string cmp, parserStruct *right) const throw (std::invalid_argument)
{
	if ((left == NULL) || (right == NULL)) {
		throw std::invalid_argument(std::string("Cannot parse expression, NULL parser structure"));
	}

	if (right->type == PT_STRING) {
		/* If it is string, we need to parse it
		 * Type may transform from string to number - we can't change cmp to LIKE here.
		 *
		 * Column::parse function (from plugins) takes "char *input" and "char[size] output" but for parsing
		 * hostnames we need function that fills parser structure with translated addresses
		 *
		 * For all other string values, parse function from plugin is called
		 */
		if (!left->colType.empty() && left->colType == "ipv4") { /* IPv4 hostname */
			this->parseHostname(right, AF_INET);
		} else if (!left->colType.empty() && left->colType == "ipv6") { /* IPv6 hostname */
			this->parseHostname(right, AF_INET6);
		} else if (left->parse) { /* String with parse function */
			parsePlugin(left, right);
		} else { /* Standard string type */
			this->parseStringType(right, left, cmp);
		}
	}

	/* Provide default comparison operator */
	if (cmp.empty()) {
		cmp = "=";
	}

	/* Subnets and IPv6 hostnames must be parsed separately - they require complicated expressions */
	switch (right->type) {
		case PT_IPv4_SUB:
		case PT_IPv6_SUB:
			/* If it is IPv4/6 address with subnet, we need to parse it (get minimal and maximal host address) */
			return this->parseExpSub(left, cmp, right);
		case PT_HOSTNAME6:
			/* If it was IPv6 hostname, we need to combine "and" and "or" operators - parse it somewhere else */
			return this->parseExpHost6(left, cmp, right);
		default: /* Don't touch others */
			break;
	}

	std::string exp = "(", op, opGroup;

	/* Parser expression "column CMP value" */
	if ((left->nParts == 1) && (right->nParts == 1)) {
		exp += createExists(left, 0, cmp == "!=");
		if (cmp == "!=") {
			if (right->type == PT_STRING) {
				exp += "(not (" + left->parts[0] + " LIKE " + right->parts[0] + ")))";
				return exp;
			}
		}

		exp += "(" + left->parts[0] + " " + cmp + " " + right->parts[0] + "))";
		return exp;
	}

	/* Set operator */
	if (right->type == PT_HOSTNAME || (right->type == PT_IPv6 && cmp == "!=")) {
		op = " or ";
	} else {
		op = " and ";
	}

	if (cmp == "!=") {
		opGroup = " and ";
	} else {
		opGroup = " or ";
	}

	/* Create expression */
	for (uint16_t i = 0, j = 0; (i < left->nParts) || (j < right->nParts); i++, j++) {
		/* If one string vector is at the end but second not, duplicate its last value */
		if (i == left->nParts) {
			i--;
		}
		if (j == right->nParts) {
			j--;
			if (left->type == PT_GROUP) {
				j = 0;
				exp = exp.substr(0, exp.length() - op.length()) + ")" + opGroup;
			}
		}

		if (j == 0 && left->type == PT_GROUP) {
			exp += "(";
		}

		/* Add part into expression */
		if (cmp == "!=") {
			exp += "(" + createExists(left, 0, true);
			if (right->type == PT_STRING) {
				exp += "(not (" + left->parts[0] + " LIKE " + right->parts[0] + ")))" + op;
			} else {
				exp += "(" + left->parts[i] + " " + cmp + " " + right->parts[j] + "))" + op;
			}
		} else {
			exp += "(" + createExists(left, i, false);
			exp += "(" + left->parts[i] + " " + cmp + " " + right->parts[j] + "))" + op;
		}
	}

	/* Remove last operator and close bracket */
	if (left->type == PT_GROUP) {
		exp = exp = exp.substr(0, exp.length() - op.length()) + "))";
	} else {
		exp = exp.substr(0, exp.length() - op.length()) + ")";
	}

	/* Return created expression */
	return exp;
}

std::string Filter::parseExp(parserStruct *left, parserStruct *right) const
{
	return this->parseExp(left, "", right);
}

std::string Filter::parseExpSub(parserStruct *left, std::string cmp, parserStruct *right) const throw (std::invalid_argument)
{
	if (left == NULL || right == NULL) {
		throw std::invalid_argument(std::string("Cannot parse expression with subnet, NULL parser structure"));
	}

	int i, rightPos = 0;
	std::string exp, op, opGroup, cmp1, cmp2;

	if (cmp == "!=") {
		cmp1 = " < ";
		cmp2 = " > ";
		op = " or ";
		opGroup = " and ";
	} else {
		cmp1 = " >= ";
		cmp2 = " <= ";
		op = " and ";
		opGroup = " or ";
	}

	/* Openning bracket */
	exp = "(";

	/* Create expression */
	for (i = 0; i < left->nParts; i++) {
		/* Check if left structure is group of columns and add openning bracket for group */
		if (rightPos == 0 && left->type == PT_GROUP) {
			exp += "(";
		}

		/* Insert (NOT) EXISTS according to comparison operator */
		exp += "(" + createExists(left, i, cmp == "!=") + "(";

		/* Add values from right parser structure */
		exp += "(" + left->parts[i] + cmp1 + right->parts[rightPos++] + ")" + op;
		exp += "(" + left->parts[i] + cmp2 + right->parts[rightPos++] + ")))" + op;

		/* If rightPos is at the end of right->parts, reset this counter */
		if (rightPos >= right->nParts) {
			/* Close group and insert according operator (and | or) */
			if (left->type == PT_GROUP) {
				exp = exp.substr(0, exp.length() - op.length()) + ")" + opGroup;
			}
			rightPos = 0;
		}
	}

	/* Remove last operator */
	if (left->type == PT_GROUP) {
		exp = exp.substr(0, exp.length() - opGroup.length());
	} else {
		exp = exp.substr(0, exp.length() - op.length());
	}

	/* Insert closing bracket */
	exp += ") ";

	return exp;
}

std::string Filter::parseExpHost6(parserStruct *left, std::string cmp, parserStruct *right) const throw (std::invalid_argument)
{
	if (left == NULL || right == NULL) {
		throw std::invalid_argument(std::string("Cannot parse hostname (IPv6) expression, NULL parser structure"));
	}

	int i = 0, leftPos = 0;
	std::string exp, op1 = " and ", op2 = " or ";

	/* Openning bracket */
	exp = "(";

	if (cmp == "!=") {
		op1 = " or ";
		op2 = " and ";
	}

	/* Create expression */
	while (i < right->nParts) {
		if (cmp == "!=") {
			exp += "(" + createExists(left, leftPos, true) + "(";
			exp += left->parts[leftPos++] + " " + cmp + " " + right->parts[i++] + "))" + op1;
			exp += "(" + createExists(left, leftPos, true) + "(";
			exp += left->parts[leftPos++] + " " + cmp + " " + right->parts[i++] + "))" + op2;
		} else {
			exp += "(" + createExists(left, leftPos, false) + "(";
			exp += left->parts[leftPos++] + " " + cmp + " " + right->parts[i++] + "))" + op1;
			exp += "(" + createExists(left, leftPos, false) + "(";
			exp += left->parts[leftPos++] + " " + cmp + " " + right->parts[i++] + "))" + op2;
		}

		/* If all column parts were used, jump to start and use them again */
		if (leftPos == left->nParts) {
			leftPos = 0;
		}
	}

	/* Remove last "or", insert closing bracket */
	exp = exp.substr(0, exp.length() - op2.length()) + ") ";

	return exp;
}

void Filter::parseString(parserStruct *ps, std::string text) const throw (std::invalid_argument)
{
	if (ps == NULL) {
		throw std::invalid_argument(std::string("Cannot parse string, NULL parser structure"));
	}

	ps->nParts = 1;
	ps->type = PT_STRING;
	ps->parts.push_back(text);
}

void Filter::parseStringType(parserStruct *ps, parserStruct *col, std::string &cmp) const throw (std::invalid_argument)
{
	if (ps == NULL) {
		throw std::invalid_argument(std::string("Cannot parse string by type, NULL parser structure"));
	}
/*
	if (col->parse != NULL) {
		char buff[PLUGIN_BUFFER_SIZE];
		col->parse((char *) ps->parts[0].c_str(), buff, col->parseConf);
		if (strlen(buff) > 0) {
			ps->parts[0] = std::string(buff);
			ps->type = PT_NUMBER;
		}
	}
*/

	if (ps->type == PT_STRING) {
		/* For all other columns with string value it stays as it is */
		if (cmp.empty()) {
			ps->parts[0] = "'%" + ps->parts[0] + "%'";
		} else if (cmp == ">") {
			ps->parts[0] = "'%" + ps->parts[0] + "'";
		} else if (cmp == "<") {
			ps->parts[0] = "'" + ps->parts[0] + "%'";
		}
		if (cmp == "!=") {
			//        	Not supported
			//            cmp = "NOT LIKE";
		} else {
			cmp = "LIKE";
		}
	}
}

void Filter::parseListCreate(std::vector<parserStruct *> *list, std::string cmp, parserStruct *column) const throw (std::invalid_argument)
{
	if (list == NULL || column == NULL) {
		throw std::invalid_argument(std::string("Cannot create list, NULL list or parser structure"));
	}
	column->parts.push_back(cmp);
	list->push_back(column);
}

void Filter::parseListAdd(std::vector<parserStruct *> *list, parserStruct *value) const throw (std::invalid_argument)
{
	if (list == NULL || value == NULL) {
		throw std::invalid_argument(std::string("Cannot add to list, NULL list parser structure"));
	}

	/* Add new parser structure into list */
	list->push_back(value);
}

std::string Filter::parseExpList(std::vector<parserStruct *> *list) const throw (std::invalid_argument)
{
	if (list == NULL) {
		throw std::invalid_argument(std::string("Cannot parse expression with list, NULL parser structure"));
	}

	/* Go through all structures in vector and parse them with column on first position */
	std::string cmp = "=", op = " or ", exp = "(";


	if (list->front()->parts.back() != "in") {
		cmp = "!=";
		op = " and ";
	}

	while (list->size() > 1) {

		exp += "(" + this->parseExp(list->front(), cmp, list->back()) + ")" + op;

		/* We don't need this structure anymore - free memory and remove it from list */
		delete list->back();
		list->pop_back();
	}

	/* Remove last " or " and close bracket */
	exp = exp.substr(0, exp.length() - op.length()) + ") ";

	return exp;
}

std::string Filter::parseExists(parserStruct* ps) const throw (std::invalid_argument)
{
	if (ps == NULL) {
		throw std::invalid_argument(std::string("Cannot parse exists expression, NULL parser structure"));
	}
	std::string exp = "(", op = " and ";
	uint16_t i;
	for (i = 0; i < ps->nParts; i++) {
		exp += createExists(ps, i, "EXISTS", op);
	}
	
	exp = exp.substr(0, exp.length() - op.length()) + ")";
	
	return exp;
}

std::string Filter::createExists(parserStruct *left, int i, bool neq) const
{
	/* prepare keyword(s) and operator according to comparison */
	std::string exp = "", exists = "EXISTS", op = " and ";
	if (neq) {
		exists = "NOT EXISTS";
		op = " or ";
	}

	/* create exists clause */
	return createExists(left, i, exists, op);
}

std::string Filter::createExists(parserStruct *left, int i, std::string exists, std::string op) const
{
	std::string exp = "";
	if (left->type == PT_COMPUTED) {
		/* computed column - add every part of enumeration */
		exp += "(";
		for (stringSet::iterator it = left->baseCols.begin(); it != left->baseCols.end(); ++it) {
			exp += exists + "(" + *it + ")" + op;
		}
		/* remove last operator */
		exp = exp.substr(0, exp.length() - op.length()) + ")" + op;
	} else {
		/* simple column */
		exp += exists + "(" + onlyCol(left->parts[i], left->type) + ")" + op;
	}
	return exp;
}


std::string Filter::onlyCol(std::string& expr, partsType type) const
{
	/* get only column name from expression */
	std::string res = expr;
	if (type == PT_BITCOLVAL) {
		res = expr.substr(2);
		res = res.substr(0, res.find(" "));
	}
	return res;
}

bool Filter::checkFilter()
{
	ibis::whereClause wc;
	
	return wc.parse(this->filterString.c_str()) == 0;
}


Filter::Filter(Configuration &conf) throw (std::invalid_argument)
{
	init(conf);
}

Filter::Filter(): scaninfo(NULL), actualConf(NULL)
{
	this->filterString = "1 = 1";
}

} /* end of namespace fbitdump */
