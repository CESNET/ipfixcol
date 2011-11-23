/**
 * \file Printer.cpp
 * \author Petr Velan <petr.velan@cesnet.cz>
 * \brief Class for printing fastbit tables
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
#include <cstdlib>
#include <cstring>
#include <arpa/inet.h>
#include <netdb.h>
#include "protocols.h"
#include "Column.h"
#include "Printer.h"
#include "Table.h"
#include "Resolver.h"

namespace fbitdump
{


bool Printer::print(TableManager &tm)
{
	/* if there is nothing to print, return */
	if (conf.getColumns().size() == 0) {
		return true;
	}


	/* print table header */
	if (!conf.getQuiet()) {
		printHeader();
	}

	Cursor *cursor;
	TableManagerCursor *tmc = tm.createCursor();
	if (tmc == NULL) {
		/* no tables, no rows */
		return true;
	}

	while (tmc->next()) {
		cursor = tmc->getCurrentCursor();
		printRow(cursor);
	}

	delete(tmc);

	/* TODO print nfdump-like statistics, maybe in main program */


	return true;
}

void Printer::printHeader()
{
	/* print column names */
	for (size_t i = 0; i < conf.getColumns().size(); i++) {
		/* set defined column width */
		out.width(conf.getColumns()[i]->getWidth());

		/* set defined column alignment */
		if (conf.getColumns()[i]->getAlignLeft()) {
			out.setf(std::ios_base::left, std::ios_base::adjustfield);
		} else {
			out.setf(std::ios_base::right, std::ios_base::adjustfield);
		}

		out << conf.getColumns()[i]->getName();
	}
	/* new line */
	out << std::endl;

}

void Printer::printRow(Cursor *cur)
{
	/* go over all defined columns */
	for (size_t i = 0; i < conf.getColumns().size(); i++) {
		/* set defined column width */
		out.width(conf.getColumns()[i]->getWidth());

		/* set defined column alignment */
		if (conf.getColumns()[i]->getAlignLeft()) {
			out.setf(std::ios_base::left, std::ios_base::adjustfield);
		} else {
			out.setf(std::ios_base::right, std::ios_base::adjustfield);
		}

		out << printValue(conf.getColumns()[i], cur);
	}
	out << "\n"; /* much faster then std::endl */
}

std::string Printer::printValue(Column *col, Cursor *cur)
{
	if (col->isSeparator()) {
		return col->getName();
	}

	values *val = col->getValue(cur);

	/* check for missing column */
	if (val == NULL) {
		return col->getNullStr();
	}

	std::string valueStr;

	if (!col->getSemantics().empty() && col->getSemantics() != "flows") {
		if (col->getSemantics() == "ipv4") {
			valueStr = printIPv4(val->value[0].uint32);
		} else if (col->getSemantics() == "tmstmp64") {
			valueStr = printTimestamp64(val->value[0].uint64);
		} else if (col->getSemantics() == "tmstmp32") {
			valueStr = printTimestamp32(val->value[0].uint32);
		} else if (col->getSemantics() == "ipv6") {
			valueStr = printIPv6(val->value[0].uint64, val->value[1].uint64);
		} else if (col->getSemantics() == "protocol") {
			if (!conf.getPlainNumbers()) {
				valueStr = protocols[val->value[0].uint8];
			} else {
				static std::stringstream ss;
				ss << (uint16_t) val->value[0].uint8;
				valueStr = ss.str();
				ss.str("");
			}
		} else if (col->getSemantics() == "tcpflags") {
			valueStr = printTCPFlags(val->value[0].uint8);
		} else if (col->getSemantics() == "duration") {
			valueStr = printDuration(val->value[0].uint64);
		}
	} else {
		valueStr = val->toString(conf.getPlainNumbers());
	}

	/* clean value variable */
	delete val;

	return valueStr;

}

std::string Printer::printIPv4(uint32_t address)
{
	char buf[INET_ADDRSTRLEN];
	struct in_addr in_addr;
	int ret;
	Resolver *resolver;

	resolver = this->conf.getResolver();

	/* translate IP address to domain name, if user wishes so */
	if (resolver->isConfigured()) {
		char host[NI_MAXHOST];

		ret = resolver->reverseLookup(address, host, NI_MAXHOST);
		if (ret == true) {
			return host;
		}

		/* Error during DNS lookup, print IP address instead */
	}

	/*
	 * user don't want to see domain names, or DNS is somehow broken.
	 * print just IP address
	 */
	in_addr.s_addr = htonl(address);
	inet_ntop(AF_INET, &in_addr, buf, INET_ADDRSTRLEN);

	/* IP address in printable form */
	return buf;
}

std::string Printer::printIPv6(uint64_t part1, uint64_t part2)
{
	char buf[INET6_ADDRSTRLEN];
	struct in6_addr in6_addr;
	int ret;
	Resolver *resolver;

	resolver = this->conf.getResolver();

	/* translate IP address to domain name, if user wishes so */
	if (resolver->isConfigured()) {
		char host[NI_MAXHOST];

		ret = resolver->reverseLookup6(part1, part2, host, NI_MAXHOST);
		if (ret == true) {
			return host;
		}

		/* Error during DNS lookup, print IP address instead */
	}

	/*
	 * user don't want to see domain names, or DNS is somehow broken.
	 * print just IP address
	 */
	*((uint64_t*) &in6_addr.s6_addr) = htobe64(part1);
	*(((uint64_t*) &in6_addr.s6_addr)+1) = htobe64(part2);
	inet_ntop(AF_INET6, &in6_addr, buf, INET6_ADDRSTRLEN);

	return buf;
}

std::string Printer::printTimestamp32(uint32_t timestamp)
{
	time_t timesec = timestamp;
	struct tm *tm = gmtime(&timesec);

	return this->printTimestamp(tm, 0);
}

std::string Printer::printTimestamp64(uint64_t timestamp)
{
	time_t timesec = timestamp/1000;
	uint64_t msec = timestamp % 1000;
	struct tm *tm = localtime(&timesec);

	return this->printTimestamp(tm, msec);
}

std::string Printer::printTimestamp(struct tm *tm, uint64_t msec)
{
	char buff[23];

	strftime(buff, sizeof(buff), "%Y-%m-%d %T", tm);
	/* append miliseconds */
	sprintf(&buff[19], ".%03u", (unsigned int) msec);

	return buff;
}

std::string Printer::printTCPFlags(unsigned char flags)
{
	std::string result = "......";

	if (flags & 0x20) {
		result[0] = 'U';
	}
	if (flags & 0x10) {
		result[1] = 'A';
	}
	if (flags & 0x08) {
		result[2] = 'P';
	}
	if (flags & 0x04) {
		result[3] = 'R';
	}
	if (flags & 0x02) {
		result[4] = 'S';
	}
	if (flags & 0x01) {
		result[5] = 'F';
	}

	return result;
}

std::string Printer::printDuration(uint64_t duration) {
	static std::ostringstream ss;
	static std::string str;
	ss << std::fixed;
	ss.precision(3);

	ss << (float) duration/1000;

	str = ss.str();
	ss.str("");

	return str;
}

/* copy output stream and format */
Printer::Printer(std::ostream &out, Configuration &conf):
		out(out), conf(conf)
{}



}
