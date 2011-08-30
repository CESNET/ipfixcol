/**
 * \file conlumnFormat.cpp
 * \author Petr Velan <petr.velan@cesnet.cz>
 * \brief Class for management of columns format
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

#include "columnFormat.h"

namespace ipfixdump
{

std::string columnFormat::getName() {
	return name;
}

std::string columnFormat::getValue(ibis::table::cursor &cur, bool plainNumbers,
		namesColumnsMap &namesColumns) {
	std::string value;
	std::stringstream ss;
	values *val = NULL;

	/* check whether we have name column */
	if (groups.empty()) {
		return name;
	}

	/* check all groups until value is found */
	for (std::map<int, AST*>::iterator it = groups.begin(); it != groups.end(); it++) {
		/* handle special columns first */
		if (!it->second->semantics.empty() && it->second->semantics == "flows" && it->second->value.empty()) {
			value = "1";
			break;
		}

		/* try to compute column value */
		val = evaluate(it->second, cur, namesColumns);
		/* if value cannot be computed (wrong group), try next */
		if (val == NULL) {
			continue;
		}

		/* print by semantics */
		if (!it->second->semantics.empty() && it->second->semantics != "flows") {
			if (it->second->semantics == "ipv4") {
				value = printIPv4(val->value[0].uint32);
			} else if (it->second->semantics == "timestamp") {
				value = printTimestamp(val->value[0].uint64);
			} else if (it->second->semantics == "ipv6") {
				value = printIPv6(val->value[0].uint64, val->value[1].uint64);
			} else if (it->second->semantics == "protocol") {
				if (!plainNumbers) {
					value = protocols[val->value[0].uint8];
				} else {
					ss << (uint16_t) val->value[0].uint8;
					value = ss.str();
				}
			} else if (it->second->semantics == "tcpflags") {
				value = printTCPFlags(val->value[0].uint8);
			}

		} else {
			/* print by type */
			/* TODO what to do with more parts here? */
			switch (val->type) {
			case ibis::BYTE:
				ss << (int16_t) val->value[0].int8;
				value = ss.str();
				break;
			case ibis::UBYTE:
				ss << (uint16_t) val->value[0].uint8;
				value = ss.str();
				break;
			case ibis::SHORT:
				ss << val->value[0].int16;
				value = ss.str();
				break;
			case ibis::USHORT:
				ss << val->value[0].uint16;
				value = ss.str();
				break;
			case ibis::INT:
				ss << val->value[0].int32;
				value = ss.str();
				break;
			case ibis::UINT:
				ss << val->value[0].uint32;
				value = ss.str();
				break;
			case ibis::LONG:
				ss << val->value[0].int64;
				value = ss.str();
				break;
			case ibis::ULONG:
				ss << val->value[0].uint64;
				value = ss.str();
				break;
			case ibis::FLOAT:
				ss << val->value[0].flt;
				value = ss.str();
				break;
			case ibis::DOUBLE:
				ss << val->value[0].dbl;
				value = ss.str();
				break;
			case ibis::TEXT:
			case ibis::CATEGORY:
			case ibis::OID:
			case ibis::BLOB:
			case ibis::UNKNOWN_TYPE:
				value = val->string;
				break;
			default:
				break;
			}
		}

		/* clear val and break if done */
		if (val != NULL) {
			delete val;
		}
		if (!value.empty()) {
			break;
		}
	}
	/* default empty value */
	if (value.empty()) {
		value = NULL_STR;
	}

	return value;
}

values *columnFormat::evaluate(AST *ast, ibis::table::cursor &cur,
		namesColumnsMap &namesColumns) {
	values *retVal = NULL;

	/* check input AST */
	if (ast == NULL) {
		return NULL;
	}

	/* evaluate AST */
	switch (ast->type) {
		case ipfixdump::value:
			retVal = getValueByType(ast, cur, namesColumns);
			break;
		case ipfixdump::operation:
			values *left, *right;
			left = evaluate(ast->left, cur,  namesColumns);
			right = evaluate(ast->right, cur,  namesColumns);

			if (left != NULL && right != NULL) {
				retVal = performOperation(left, right, ast->operation);
			}
			/* clean up */
			delete left;
			delete right;

			break;
		default:
			std::cerr << "Unknown AST type" << std::endl;
			break;
	}

	return retVal;
}

values *columnFormat::getValueByType(AST *ast, ibis::table::cursor &cur, namesColumnsMap &namesColumns) {
	values *retVal = new values();
	int ret = 0, colNum = 0;
	ibis::TYPE_T type;
	ibis::table::namesTypes::iterator it;
	namesColumnsMap::iterator cit;

	for (int i=0; i < ast->parts && i < MAX_PARTS; i++) {

		/* create column name */
		std::stringstream columnName;
		columnName << ast->value;
		/* add part number when there are more parts */
		if (ast->parts > 1) {
			columnName << "p" << i;
		}

		/* get location of the column */
		if ((cit = namesColumns.find(columnName.str().c_str())) != namesColumns.end()) {
			colNum = cit->second;
		} else {
			/* column not found */
			ret = 1;
			break;
		}

		/* find column's type */
//		if ((it = namesTypes.find(columnName.str().c_str())) != namesTypes.end()) {
//			type = it->second;
//		} else {
//			type = ibis::UNKNOWN_TYPE;
//		}
		type = cur.columnTypes()[colNum];

		switch (type) {
		case ibis::BYTE:
			ret = cur.getColumnAsByte(colNum, retVal->value[i].int8);
			retVal->type = ibis::BYTE;
			break;
		case ibis::UBYTE:
			ret = cur.getColumnAsUByte(colNum, retVal->value[i].uint8);
			retVal->type = ibis::UBYTE;
			break;
		case ibis::SHORT:
			ret = cur.getColumnAsShort(colNum, retVal->value[i].int16);
			retVal->type = ibis::SHORT;
			break;
		case ibis::USHORT:
			ret = cur.getColumnAsUShort(colNum, retVal->value[i].uint16);
			retVal->type = ibis::USHORT;
			break;
		case ibis::INT:
			ret = cur.getColumnAsInt(colNum, retVal->value[i].int32);
			retVal->type = ibis::INT;
			break;
		case ibis::UINT:
			ret = cur.getColumnAsUInt(colNum, retVal->value[i].uint32);
			retVal->type = ibis::UINT;
			break;
		case ibis::LONG:
			ret = cur.getColumnAsLong(colNum, retVal->value[i].int64);
			retVal->type = ibis::LONG;
			break;
		case ibis::ULONG:
			ret = cur.getColumnAsULong(colNum, retVal->value[i].uint64);
			retVal->type = ibis::ULONG;
			break;
		case ibis::FLOAT:
			ret = cur.getColumnAsFloat(colNum, retVal->value[i].flt);
			retVal->type = ibis::FLOAT;
			break;
		case ibis::DOUBLE:
			ret = cur.getColumnAsDouble(colNum, retVal->value[i].dbl);
			retVal->type = ibis::DOUBLE;
			break;
		case ibis::TEXT:
		case ibis::CATEGORY: {
			ret = cur.getColumnAsString(colNum, retVal->string);
			retVal->type = ibis::TEXT;
			break; }
		case ibis::OID:
		case ibis::BLOB:
			retVal->type = ibis::BLOB;
			retVal->string = "TODO";
			break;
		case ibis::UNKNOWN_TYPE:
			/* column not found in DB */
			ret = 1;
			break;
		default:
			break;
		}
	}

	if (ret != 0) {
		delete retVal;
		return NULL;
	}

	return retVal;
}

std::string columnFormat::printIPv4(uint32_t address) {
	char buf[INET_ADDRSTRLEN];
	struct in_addr in_addr;

	/* convert address */
	in_addr.s_addr = htonl(address);
	inet_ntop(AF_INET, &in_addr, buf, INET_ADDRSTRLEN);

	return buf;
}
std::string columnFormat::printIPv6(uint64_t part1, uint64_t part2) {
	char buf[INET6_ADDRSTRLEN];
	struct in6_addr in6_addr;

	/* convert address */
	*((uint64_t*) &in6_addr.s6_addr) = htobe64(part1);
	*(((uint64_t*) &in6_addr.s6_addr)+1) = htobe64(part2);
	inet_ntop(AF_INET6, &in6_addr, buf, INET6_ADDRSTRLEN);

	return buf;
}

std::string columnFormat::printTimestamp(uint64_t timestamp) {
	/* save current stream flags */
	time_t timesec = timestamp/1000;
	uint64_t msec = timestamp % 1000;
	struct tm *tm = gmtime(&timesec);
	std::stringstream timeStream;

	timeStream.setf(std::ios_base::right, std::ios_base::adjustfield);
	timeStream.fill('0');

	timeStream << (1900 + tm->tm_year) << "-";
	timeStream.width(2);
	timeStream << (1 + tm->tm_mon) << "-";
	timeStream.width(2);
	timeStream << tm->tm_mday << " ";
	timeStream.width(2);
	timeStream << tm->tm_hour << ":";
	timeStream.width(2);
	timeStream << tm->tm_min << ":";
	timeStream.width(2);
	timeStream << tm->tm_sec << ".";
	timeStream.width(3);
	timeStream << msec;

	return timeStream.str();
}

std::string columnFormat::printTCPFlags(unsigned char flags) {
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

values* columnFormat::performOperation(values *left, values *right, unsigned char op) {
	values *result = new values;
	/* TODO add some type checks maybe... */
	switch (op) {
				case '+':
					result->type = ibis::ULONG;
					result->value[0].int64 = left->toULong() + right->toULong();
					break;
				case '-':
					result->type = ibis::ULONG;
					result->value[0].int64 = left->toULong() - right->toULong();
					break;
				case '*':
					result->type = ibis::ULONG;
					result->value[0].int64 = left->toULong() * right->toULong();
					break;
				case '/':
					result->type = ibis::ULONG;
					if (right->toULong() == 0) {
						result->value[0].int64 = 0;
					} else {
						result->value[0].int64 = left->toULong() / right->toULong();
					}
					break;
				}
	return result;
}

columnFormat::~columnFormat() {
	for (std::map<int, AST*>::iterator it = groups.begin(); it != groups.end(); it++) {
		delete it->second;
	}
}

stringSet columnFormat::getColumns(AST* ast) {
	stringSet ss, ls, rs;
	if (ast->type == ipfixdump::value) {
		if (ast->semantics != "flows") {
			if (ast->parts > 1) {
				/* TODO: how to aggregate parts? */
				for (int i = 0; i < ast->parts; i++) {
					std::stringstream strStrm;
					strStrm << ast->value << "p" << i;
					ss.insert(strStrm.str());
				}
			} else {
				if (!ast->aggregation.empty()) {
					ss.insert(ast->aggregation + "(" + ast->value + ")");
				} else {
					ss.insert(ast->value);
				}
			}
		} else { /* flows need this (on aggregation value must be set to count) */
			ss.insert("count(*)");
			ast->value = "*";
		}
	} else { /* operation */
		ls = getColumns(ast->left);
		rs = getColumns(ast->right);
		ss.insert(ls.begin(), ls.end());
		ss.insert(rs.begin(), rs.end());
	}

	return ss;
}

std::map<int, stringSet> columnFormat::getColumns() {
	std::map<int, stringSet> columns;

	for (std::map<int, AST*>::iterator it = groups.begin();it != groups.end(); it++) {
		columns[it->first] = getColumns(it->second);
	}

	return columns;
}

bool columnFormat::canAggregate() {
	bool ret = true;

	/* all groups must be aggregable */
	for (std::map<int, AST*>::iterator it = groups.begin();it != groups.end(); it++) {
		if (!canAggregate(it->second)) {
			ret = false;
		}
	}

	return ret;
}

bool columnFormat::canAggregate(AST* ast) {

	/* AST type must be 'value' and aggregation string must not be empty */
	if (ast->type == ipfixdump::value) {
		if (!ast->aggregation.empty()) {
			return true;
		}
	/* or both sides of operation must be aggregable */
	} else if (ast->type == ipfixdump::operation) {
		return canAggregate(ast->left) && canAggregate(ast->right);
	}

	/* all other cases */
	return false;
}

columnFormat::columnFormat(): width(0), alignLeft(false) {}

}
