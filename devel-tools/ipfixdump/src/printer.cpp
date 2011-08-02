/**
 * \file printer.cpp
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

#include "printer.h"

namespace ipfixdump
{

int printer::print(ibis::table *table, uint64_t limit) {
	int ierr = 0;

	/* set internal variables */
	this->table = table;
	colNames.clear();
	for (size_t i = 0; i < table->columnNames().size(); i++) {
		colNames.push_back(table->columnNames()[i]);
	}
	colTypes.clear();
	colTypes = table->columnTypes();

	/* if there is nothing to print, return */
	if (colNames.size() == 0) {
		return ierr;
	}

	/* create cursor */
	ibis::table::cursor *cur = table->createCursor();
	if (cur == 0) return -1;

	/* get number of rows */
	uint64_t nRows = table->nRows();

	/* set limit */
	if (limit == 0 || limit > nRows) {
		limit = nRows;
	}

	/* print table header */
	printHeader();

	/* print rows */
	for (size_t i = 0; i < limit; i++) {
		ierr = cur->fetch(); // make the next row ready
		if (ierr == 0) {
			printRow(cur);
		} else {
			std::cerr << "print() failed to fetch row " << i << std::endl;
			ierr = -2;
			/* stop printing */
			break;
		}
	}

	/* free cursor */
	delete cur;

	return ierr;
}

void printer::printHeader() {
	for (size_t i = 0; i < colNames.size(); i++) {
		if (colNames[i] == "e0id8" || colNames[i] == "e0id12") {
			out.width(INET_ADDRSTRLEN);
		} else if (colNames[i] == "e0id152" || colNames[i] == "e0id153") {
			/* length of timestamp */
			out.width(23);
		} else if (colNames[i] == "e0id27p0" || colNames[i] == "e0id28p0") {
			out.width(IPV6_STRLEN);
			i++;
		} else {
			out.width(getStrLength(colTypes[i]));
		}
		out << colNames[i] << COLUMN_SEPARATOR;
	}
	out << std::endl;
}

void printer::printRow(ibis::table::cursor *cur) {

	for (size_t i = 0; i < colNames.size(); i++) {
		if (colNames[i] == "e0id8" || colNames[i] == "e0id12") {
			uint32_t buf;
			cur->getColumnAsUInt(i, buf);
			printIPv4(buf);
		} else if (colNames[i] == "e0id152" || colNames[i] == "e0id153") {
			uint64_t buf;
			cur->getColumnAsULong(i, buf);
			printTimestamp(buf);
		} else if (colNames[i] == "e0id27p0" || colNames[i] == "e0id28p0") {
			uint64_t buf1, buf2;
			cur->getColumnAsULong(i, buf1);
			cur->getColumnAsULong(i+1, buf2);
			printIPv6(buf1, buf2);
			i++;
		} else {
			printByType(cur, i);
		}
		out << COLUMN_SEPARATOR;
	}
	out << std::endl;
}

void printer::printIPv4(uint32_t address) {
	char buf[INET_ADDRSTRLEN];
	struct in_addr in_addr;
	std::ios_base::fmtflags flags = out.flags();

	/* convert address */
	in_addr.s_addr = htonl(address);
	inet_ntop(AF_INET, &in_addr, buf, INET_ADDRSTRLEN);

	out.width(INET_ADDRSTRLEN);
	out << buf;
	out.flags(flags);
}

void printer::printIPv6(uint64_t part1, uint64_t part2) {
	char buf[INET6_ADDRSTRLEN];
	struct in6_addr in6_addr;
	std::ios_base::fmtflags flags = out.flags();

	/* convert address */
	*((uint64_t*) &in6_addr.s6_addr) = htobe64(part1);
	*(((uint64_t*) &in6_addr.s6_addr)+1) = htobe64(part2);
	inet_ntop(AF_INET6, &in6_addr, buf, INET6_ADDRSTRLEN);

	out.width(IPV6_STRLEN);
	out << buf;
	out.flags(flags);
}

void printer::printTimestamp(uint64_t timestamp) {
	std::ios_base::fmtflags flags = out.flags();
	time_t timesec = timestamp/1000;
	uint64_t msec = timestamp % 1000;
	struct tm *tm = gmtime(&timesec);

	out.setf(std::ios_base::right, std::ios_base::adjustfield);
	out.fill('0');

	out << (1900 + tm->tm_year) << "-";
	out.width(2);
	out << (1 + tm->tm_mon) << "-";
	out.width(2);
	out << tm->tm_mday << " ";
	out.width(2);
	out << tm->tm_hour << ":";
	out.width(2);
	out << tm->tm_min << ":";
	out.width(2);
	out << tm->tm_sec << ".";
	out.width(3);
	out << msec;

	out.fill(' ');
	out.flags(flags);
}

void printer::printByType(ibis::table::cursor *cur, size_t i) {
	switch (colTypes[i]) {
	case ibis::BYTE:
		char tmpbyte;
		cur->getColumnAsByte(i, tmpbyte);
		out.width(BYTE_STRLEN);
		out << (int16_t) tmpbyte;
		break;
	case ibis::UBYTE:
		unsigned char tmpubyte;
		cur->getColumnAsUByte(i, tmpubyte);
		out.width(BYTE_STRLEN);
		out << ((uint16_t) tmpubyte);
		break;
	case ibis::SHORT:
		int16_t tmpshort;
		cur->getColumnAsShort(i, tmpshort);
		out.width(SHORT_STRLEN);
		out << tmpshort;
		break;
	case ibis::USHORT:
		uint16_t tmpushort;
		cur->getColumnAsUShort(i, tmpushort);
		out.width(SHORT_STRLEN);
		out << tmpushort;
		break;
	case ibis::INT:
		int32_t tmpint;
		cur->getColumnAsInt(i, tmpint);
		out.width(INT_STRLEN);
		out << tmpint;
		break;
	case ibis::UINT:
		uint32_t tmpuint;
		cur->getColumnAsUInt(i, tmpuint);
		out.width(INT_STRLEN);
		out << tmpuint;
		break;
	case ibis::LONG:
		int64_t tmplong;
		cur->getColumnAsLong(i, tmplong);
		out.width(LONG_STRLEN);
		out << tmplong;
		break;
	case ibis::ULONG:
		uint64_t tmpulong;
		cur->getColumnAsULong(i, tmpulong);
		out.width(LONG_STRLEN);
		out << tmpulong;
		break;
	case ibis::FLOAT:
		float tmpfloat;
		cur->getColumnAsFloat(i, tmpfloat);
		out.width(FLOAT_STRLEN);
		out << tmpfloat;
		break;
	case ibis::DOUBLE:
		double tmpdouble;
		cur->getColumnAsDouble(i, tmpdouble);
		out.width(DOUBLE_STRLEN);
		out << tmpdouble;
		break;
	case ibis::TEXT:
	case ibis::CATEGORY: {
		std::string tmpstring;
		cur->getColumnAsString(i, tmpstring);
		out << tmpstring;
		break; }
	case ibis::OID:
	case ibis::BLOB:
	case ibis::UNKNOWN_TYPE:
		out << "\\TODO";
		break;
	default:
		break;
	}
}

int printer::getStrLength(ibis::TYPE_T type) {
	switch (type) {
		case ibis::BYTE:
		case ibis::UBYTE:
			return BYTE_STRLEN;
			break;
		case ibis::SHORT:
		case ibis::USHORT:
			return SHORT_STRLEN;
			break;
		case ibis::INT:
		case ibis::UINT:
			return INT_STRLEN;
			break;
		case ibis::LONG:
		case ibis::ULONG:
			return LONG_STRLEN;
			break;
		case ibis::FLOAT:
			return FLOAT_STRLEN;
			break;
		case ibis::DOUBLE:
			return DOUBLE_STRLEN;
			break;
		case ibis::TEXT:
		case ibis::CATEGORY:
		case ibis::OID:
		case ibis::BLOB:
		case ibis::UNKNOWN_TYPE:
		default:
			return 0;
			break;
	}

	return 0;
}

printer::printer(std::ostream &out, std::string format):
		out(out), format(format)
{}

}
