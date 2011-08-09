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

void printer::addTable(ibis::table *table) {
	/* check input */
	if (table == NULL) {
		return;
	}

	tables.push_back(table);
	/* save names and types to associative array*/
	for (size_t i = 0; i < table->columnNames().size(); i++) {
		namesTypes[table->columnNames()[i]] = table->columnTypes()[i];
	}
}

void printer::addTables(tableVector &tables) {
	for (tableVector::iterator it = tables.begin(); it != tables.end(); it++) {
		addTable(*it);
	}
}

tableVector printer::clearTables() {
	tableVector tmp = tables;
	tables.clear();
	namesTypes.clear();

	return tmp;
}

int printer::print(uint64_t limit) {
	int ierr = 0;
	uint64_t maxRows, nRows, printedRows = 0;

	/* process format -- fills this->colInfo */
	parseFormat();

	/* if there is nothing to print, return */
	if (colInfos.size() == 0) {
		return ierr;
	}


	/* print table header */
	printHeader();

	/* go over all tables to print */
	for (tableVector::iterator tableIt = tables.begin(); tableIt != tables.end(); tableIt++) {

		/* create cursor */
		ibis::table::cursor *cur = (*tableIt)->createCursor();
		if (cur == 0) return -1;

		/* get number of rows */
		nRows = (*tableIt)->nRows();

		/* set limit */
		maxRows = limit - printedRows;
		if (limit == 0 || maxRows > nRows) {
			maxRows = nRows;
		} else if (maxRows == 0) { /* we want no more rows */
			delete cur;
			break;
		}

		/* print rows */
		for (size_t i = 0; i < maxRows; i++) {
			ierr = cur->fetch(); /* make the next row ready */
			if (ierr == 0) {
				printRow(cur);
				printedRows++;
			} else {
				std::cerr << "print() failed to fetch row " << i << std::endl;
				ierr = -2;
				/* stop printing */
				break;
			}
		}
		/* free cursor */
		delete cur;
	}

	/* free class information about current table */
	for (std::vector<columnInfoVector>::iterator it = colInfos.begin(); it != colInfos.end(); it++) {
		for (columnInfoVector::iterator i = it->begin(); i != it->end(); i++) {
			delete *i;
		}
	}
	colInfos.clear();

	return ierr;
}

void printer::printHeader() {
	/* go over all defined column vectors and use the first one */
	for (size_t i = 0; i < colInfos.size(); i++) {
		/* set defined output width */
		out.width(colInfos[i][0]->width);

		/* set defined alignment */
		if (colInfos[i][0]->align) {
			out.setf(std::ios_base::left, std::ios_base::adjustfield);
		} else {
			out.setf(std::ios_base::right, std::ios_base::adjustfield);
		}

		/* print column name */
		out << colInfos[i][0]->name;

		/* skip next ipv6 column */
		if (colInfos[i][0]->semantics == "ipv6Address") {
			i++;
		}
	}
	/* new line */
	out << std::endl;
}

/* TODO it might be faster to remember column positions for each name and table */
void printer::printRow(ibis::table::cursor *cur) {
	/* go over all defined columns */
	for (size_t i = 0; i < colInfos.size(); i++) {

		/* set defined column width */
		out.width(colInfos[i][0]->width);

		/* set defined column alignment */
		if (colInfos[i][0]->align) {
			out.setf(std::ios_base::left, std::ios_base::adjustfield);
		} else {
			out.setf(std::ios_base::right, std::ios_base::adjustfield);
		}

		bool printed = false;

		for (columnInfoVector::iterator colInfoIt = colInfos[i].begin();
				colInfoIt != colInfos[i].end() && !printed; colInfoIt++) {
			/* detect some special cases (IP addresses, timestamps) */
			if ((*colInfoIt)->semantics == "ipv4Address") { /*IPv4*/
				uint32_t buf;
				if (cur->getColumnAsUInt((*colInfoIt)->name.c_str(), buf) == 0) {
					printIPv4(buf);
					printed = true;
				}
			} else if ((*colInfoIt)->semantics == "dateTimeMilliseconds") { /*Timestamp*/
				uint64_t buf;
				if (cur->getColumnAsULong((*colInfoIt)->name.c_str(), buf) == 0) {
					printTimestamp(buf);
					printed = true;
				}
			} else if ((*colInfoIt)->semantics == "ipv6Address") {/*IPv6*/
				uint64_t buf1, buf2;
				if (cur->getColumnAsULong((*colInfoIt)->name.c_str(), buf1) == 0 &&
						cur->getColumnAsULong((*(colInfoIt+1))->name.c_str(), buf2) == 0 ) {
					printIPv6(buf1, buf2);
					printed = true;
				}
				colInfoIt++;
			} else if ((*colInfoIt)->name == "e0id4" && !conf.plainNumbers) { /* Protocol type */
				unsigned char buf;
				if (cur->getColumnAsUByte((*colInfoIt)->name.c_str(), buf) == 0) {
					out << protocols[buf];
					printed = true;
				}
			} else {
				/* default print */
				if (printByType(cur, *colInfoIt) == 0) {
					printed = true;
				}
			}
		}

		/* no column in group found */
		if (!printed) {
			out << NULL_STR;
		}
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

	out << buf;
	out.flags(flags);
}

void printer::printTimestamp(uint64_t timestamp) {
	/* save current stream flags */
	std::ios_base::fmtflags flags = out.flags();
	time_t timesec = timestamp/1000;
	uint64_t msec = timestamp % 1000;
	struct tm *tm = gmtime(&timesec);

	/* width is manipulated separately here, flush empty spaces */
	if (out.width() > 23) {
		out.width(out.width()-23);
		out << "";
	}

	out.width(0);
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

	/* restore stream settings */
	out.fill(' ');
	out.flags(flags);
}

int printer::printByType(ibis::table::cursor *cur, columnInfo *colInfo) {
	int ret = 0;

	switch (colInfo->type) {
	case ibis::BYTE:
		char tmpbyte;
		if ((ret = cur->getColumnAsByte(colInfo->name.c_str(), tmpbyte)) == 0) {
			out << (int16_t) tmpbyte;
		}
		break;
	case ibis::UBYTE:
		unsigned char tmpubyte;
		if ((ret = cur->getColumnAsUByte(colInfo->name.c_str(), tmpubyte)) == 0) {
			out << ((uint16_t) tmpubyte);
		}
		break;
	case ibis::SHORT:
		int16_t tmpshort;
		if ((ret = cur->getColumnAsShort(colInfo->name.c_str(), tmpshort)) == 0) {
			out << tmpshort;
		}
		break;
	case ibis::USHORT:
		uint16_t tmpushort;
		if ((ret = cur->getColumnAsUShort(colInfo->name.c_str(), tmpushort)) == 0) {
			out << tmpushort;
		}
		break;
	case ibis::INT:
		int32_t tmpint;
		if ((ret = cur->getColumnAsInt(colInfo->name.c_str(), tmpint)) == 0) {
			out << tmpint;
		}
		break;
	case ibis::UINT:
		uint32_t tmpuint;
		if ((ret = cur->getColumnAsUInt(colInfo->name.c_str(), tmpuint)) == 0) {
			out << tmpuint;
		}
		break;
	case ibis::LONG:
		int64_t tmplong;
		if ((ret = cur->getColumnAsLong(colInfo->name.c_str(), tmplong)) == 0) {
			out << tmplong;
		}
		break;
	case ibis::ULONG:
		uint64_t tmpulong;
		if ((ret = cur->getColumnAsULong(colInfo->name.c_str(), tmpulong)) == 0) {
			out << tmpulong;
		}
		break;
	case ibis::FLOAT:
		float tmpfloat;
		if ((ret = cur->getColumnAsFloat(colInfo->name.c_str(), tmpfloat)) == 0) {
			out << tmpfloat;
		}
		break;
	case ibis::DOUBLE:
		double tmpdouble;
		if ((ret = cur->getColumnAsDouble(colInfo->name.c_str(), tmpdouble)) == 0) {
			out << tmpdouble;
		}
		break;
	case ibis::TEXT:
	case ibis::CATEGORY: {
		std::string tmpstring;
		if ((ret = cur->getColumnAsString(colInfo->name.c_str(), tmpstring)) == 0) {
			out << tmpstring.c_str();
		}
		break; }
	case ibis::OID:
	case ibis::BLOB:
		out << "\\TODO";
		break;
	case ibis::UNKNOWN_TYPE: /* column separators */
		out << colInfo->name.c_str();
		break;
	default:
		break;
	}

	return ret;
}

int printer::getStrLengthByType(ibis::TYPE_T type) {
	/* return defined values */
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

int printer::getStrLengthBySemantics(std::string semantics) {
	if (semantics == "dateTimeMilliseconds") {
		return 23;
	}
	if (semantics == "ipv6Address") {
		return IPV6_STRLEN;
	}
	if (semantics == "ipv4Address") {
		return INET_ADDRSTRLEN;
	}

	return 0;
}

int printer::getStrLengthByName(std::string name) {
	if (name == "e0id4") { /* Protocol identifier */
		return PROTOCOL_STRLEN;
	}

	return 0;
}

void printer::parseFormat() {
	regex_t reg;
	int err;
	regmatch_t matches[4];
	std::string tmpformat = conf.format, name, semantics;
	columnInfo *cInfo;
	columnInfoVector group;
	ibis::table::namesTypes::iterator typeIt;
	ibis::TYPE_T type;
	int align = 0;
	int width = 0;
	bool inBrackets = false, pushColumn = true;

	/* prepare regular expresion */
	regcomp(&reg, "\\(?e[0-9]+id[0-9]+(p[0-9]+)?\\)?l?", REG_EXTENDED);

	while (tmpformat.length() > 0) {
		/* set default align and width */
		align = 0;
		width = 0;

		/* run regular expression match */
		if ((err = regexec(&reg, tmpformat.c_str(), 4, matches, 0)) == 0) {
			/* check for columns separator (only outside the group) */
			if (matches[0].rm_so != 0 && !inBrackets) {
				cInfo = new columnInfo;
				cInfo->name = tmpformat.substr(0, matches[0].rm_so);
				cInfo->type = ibis::UNKNOWN_TYPE;
				cInfo->align = 0;
				cInfo->width = 0;
				group.push_back(cInfo);
				colInfos.push_back(group);
				group.clear();
			}

			/* get column name and alignment  */
			name = tmpformat.substr(matches[0].rm_so, matches[0].rm_eo - matches[0].rm_so);

			/* parse possible group start */
			if (name[0] == '(') {
				if (inBrackets) {
					std::cerr << "Nested groups detected in format string. Ignoring." << std::endl;
				} else {
					inBrackets = true;
					pushColumn = false;
				}
				name = name.substr(1);
			}

			/* parse possible alignment */
			if (name[name.length()-1] == 'l') {
				align = 1;
				/* set align for fist member of the group, it is read from there */
				if (group.size() > 0) {
					group[0]->align = 1;
				}
				name = name.substr(0,name.length()-1);
			}

			/* parse possible group end */
			if (name[name.length()-1] == ')') {
				if (inBrackets) {
					inBrackets = false;
					pushColumn = true;
				} else {
					std::cerr << "Invalid group end detected. Ignoring." << std::endl;
				}
				name = name.substr(0, name.length()-1);
			}

			/* get type for column by name */
			if ((typeIt = namesTypes.find(name.c_str())) == namesTypes.end()) {
				/* this will print NULL if set tu any type except unknown */
				type = ibis::UINT;
			} else {
				type = typeIt->second;
			}

			/* get column semantics */
			semantics = getSemantics(name);

			/* get column width based on semantics (higher priority) or type */
			if ((width = getStrLengthBySemantics(semantics)) == 0) {
				if ((width = getStrLengthByName(name)) == 0) {
					width = getStrLengthByType(type);
				}
			}

			/* width is taken from the first element of the group so update it */
			if (group.size() > 0 && group[0]->width < width) {
				group[0]->width = width;
			}

			/* add column */
			cInfo = new columnInfo;
			cInfo->name = name;
			cInfo->type = type;
			cInfo->align = align;
			cInfo->semantics = semantics;
			cInfo->width = width;
			group.push_back(cInfo);
			if (pushColumn) {
				colInfos.push_back(group);
				group.clear();
			}

			/* discard processed part of the format string */
			tmpformat = tmpformat.substr(matches[0].rm_eo);
		} else if ( err != REG_NOMATCH ) {
			std::cerr << "Bad output format string" << std::endl;
			break;
		} else {
			/* add rest of the format string */
			cInfo = new columnInfo;
			cInfo->name = tmpformat;
			cInfo->type = ibis::UNKNOWN_TYPE;
			cInfo->align = 0;
			cInfo->width = 0;
			group.push_back(cInfo);
			colInfos.push_back(group);
			group.clear();
			break;
		}
	}

	/* free regex */
	regfree(&reg);

	/* set column info by current tables, if format is not set */
	if (conf.format.length() == 0) {
		stringSet existingColumns;

		for (tableVector::iterator tableIt = tables.begin(); tableIt != tables.end(); tableIt++) {
			for (size_t i = 0; i < (*tableIt)->columnNames().size(); i++) {

				/* get name */
				name = (*tableIt)->columnNames()[i];
				/* if column with this name exists, skip it */
				if (existingColumns.find(name) != existingColumns.end()) {
					continue;
				}
				/* add column to set of useds ones */
				existingColumns.insert(name);

				/* get type */
				type = (*tableIt)->columnTypes()[i];

				/* get semantics */
				semantics = getSemantics(name);

				/* set width */
				if ((width = getStrLengthBySemantics(semantics)) == 0) {
					if ((width = getStrLengthByName(name)) == 0) {
						width = getStrLengthByType(type);
					}
				}

				/* store column */
				cInfo = new columnInfo;
				cInfo->name = name;
				cInfo->type = type;
				cInfo->align = 0;
				cInfo->semantics = semantics;
				cInfo->width = width;
				group.push_back(cInfo);
				/* ip ipv6 address, clear only after second part */
				if (semantics != "ipv6Address" || group.size() > 1) {
					colInfos.push_back(group);
					group.clear();
				}

				/* store separator */
				if (semantics != "ipv6Address") {
					cInfo = new columnInfo;
					cInfo->name = COLUMN_SEPARATOR;
					cInfo->type = ibis::UNKNOWN_TYPE;
					cInfo->align = 0;
					cInfo->width = 0;
					group.push_back(cInfo);
					colInfos.push_back(group);
					group.clear();
				}
			}
		}
	}
}

std::string printer::getSemantics(std::string name) {
	if (name == "e0id8" || name == "e0id12") { /*IPv4*/
		return "ipv4Address";
	} else if (name == "e0id152" || name == "e0id153") { /*Timestamp*/
		return "dateTimeMilliseconds";
	} else if (name == "e0id27p0" || name == "e0id28p0") {/*IPv6*/
		return "ipv6Address";
	}
	return "";
}

/* copy output stream and format */
printer::printer(std::ostream &out, configuration &conf):
		out(out), conf(conf)
{}

}
