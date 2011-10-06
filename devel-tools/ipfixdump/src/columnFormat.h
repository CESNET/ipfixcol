/**
 * \file columnFormat.h
 * \author Petr Velan <petr.velan@cesnet.cz>
 * \brief Header of class for management of columns format
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

#ifndef COLUMN_FORMAT_H_
#define COLUMN_FORMAT_H_

#include <cstdio>
#include <vector>
#include <set>
#include <getopt.h>
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <ibis.h>
#include <arpa/inet.h>
#include "typedefs.h"
#include "protocols.h"

/**
 * \brief Namespace of the ipfixdump utility
 */
namespace ipfixdump {

#define MAX_PARTS 2

/**
 * \brief structure for passing values of unknown type
 */
struct values {
	ibis::TYPE_T type;
	union {
		char int8;
		unsigned char uint8;
		int16_t int16;
		uint16_t uint16;
		int32_t int32;
		uint32_t uint32;
		int64_t int64;
		uint64_t uint64;
		float flt;
		double dbl;
	} value[MAX_PARTS];
	std::string string;

	uint64_t toULong() {
		switch(type) {
		case ibis::BYTE:
			return (uint64_t) this->value[0].int8;
			break;
		case ibis::UBYTE:
			return (uint64_t) this->value[0].uint8;
			break;
		case ibis::SHORT:
			return (uint64_t) this->value[0].int16;
			break;
		case ibis::USHORT:
			return (uint64_t) this->value[0].uint16;
			break;
		case ibis::INT:
			return (uint64_t) this->value[0].int32;
			break;
		case ibis::UINT:
			return (uint64_t) this->value[0].uint32;
			break;
		case ibis::LONG:
			return (uint64_t) this->value[0].int64;
			break;
		case ibis::ULONG:
			return this->value[0].uint64;
			break;
		default: return 0;
		}
	}

	double toDouble() {
		switch(type) {
		case ibis::FLOAT:
			return (double) this->value[0].flt;
			break;
		case ibis::DOUBLE:
			return this->value[0].dbl;
			break;
		default:
			return (double) this->toULong();
		}
	}
};

/**
 * \brief types for AST structure
 */
enum astTypes {
	value,   //!< value
	operation//!< operation
};

/**
 * \brief Abstract syntax tree structure
 *
 * Desribes the way that column value is constructed from database columns
 */
struct AST {
	astTypes type; /**< AST type */
	unsigned char operation; /**< one of '/', '*', '-', '+' */
	std::string semantics; /**< semantics of the column */
	std::string value; /**< value (column name) */
	std::string aggregation; /**< how to aggregate this column */
	int parts; /**< number of parts of column (ipv6 => e0id27p0 and e0id27p1)*/
	AST *left; /**< left subtree */
	AST *right; /**< right subtree */

	/**
	 * \brief AST constructor - sets default values
	 */
	AST(): parts(1), left(NULL), right(NULL) {};
	~AST() {
		delete left;
		delete right;
	}
};

/**
 * \brief Class managing columns format
 *
 * Represents one column for printing
 */
class columnFormat {
private:

	values *evaluate(AST *ast, ibis::table::cursor &cur,
				namesColumnsMap &namesColumns);

	values *getValueByType(AST *ast, ibis::table::cursor &cur,
				namesColumnsMap &namesColumns);

	/**
	 * \brief Print formatted IPv4 address
	 */
	std::string printIPv4(uint32_t address);

	/**
	 * \brief Print formatted IPv6 address
	 */
	std::string printIPv6(uint64_t part1, uint64_t part2);

	/**
	 * \brief Print formatted timestamp
	 */
	std::string printTimestamp(uint64_t timestamp);

	/**
	 * \brief Print formatted TCP flags
	 */
	std::string printTCPFlags(unsigned char flags);

	/**
	 * \brief Performs operation on given data
	 *
	 * Can change value type (bigger integer)
	 *
	 * @param left left operand
	 * @param right right operand
	 * @param operation one of '+', '-', '/', '*'
	 * @return return resulting value of appropriate type
	 */
	values* performOperation(values *left, values *right, unsigned char op);


	/**
	 * \brief Return column names used in this AST
	 *
	 * @param ast to go through
	 * @return Set of column names
	 */
	stringSet getColumns(AST* ast);

	/**
	 * \brief Is AST aggregable?
	 *
	 * @param ast AST to check
	 * @return true when AST is aggregable
	 */
	bool canAggregate(AST* ast);

	/**
	 * \brief Initialization function called from constructors
	 * @param null_string
	 */
	void init(std::string nullStr);

	std::string nullStr; /**< String used when table doesn`t have the column*/

public:
	std::string name; /**< column name */
	stringSet aliases; /**< set of column aliases */
	int width; /**< width of the column */
	bool alignLeft; /**< align column to left */
	/**
	 * Groups of columns, first one that can evaluate will be used
	 * When empty, column is separator and name will be printed
	 *
	 * When more that one group exists, keys are group IDs from XML
	 */
	std::map<int, AST*> groups;

	/**
	 * \brief columnFormat class destructor
	 */
	~columnFormat();

	/**
	 * \brief columnFormat class constructor
	 */
	columnFormat(std::string null_str);

	/**
	 * \brief columnFormat class constructor
	 *
	 * Sets NULL string to 'NULL'
	 */
	columnFormat();

	/**
	 * \brief Returns value of current column in row specified by cursor
	 *
	 * @param cur cursor pointing to current row
	 * @param namesTypes mapping of names to types
	 * @param plainNumbers print plain numbers
	 * @param namesColumns map of column names to column numbers
	 * @return string containing the value to print
	 */
	std::string getValue(ibis::table::cursor &cur,
			bool plainNumbers, namesColumnsMap &namesColumns);

	/**
	 * \brief Returns the name of the column
	 *
	 * @return string containing the name to print
	 */
	std::string getName();

	/**
	 * \brief Retursn map of group ids to column names
	 * Every Column names set creates one group of columns that will be checked
	 * when evaluating a query
	 *
	 * @return map of Column names sets
	 */
	std::map<int, stringSet> getColumns();

	/**
	 * \brief Can this column be used in aggregation?
	 * @return true when column is aggregable
	 */
	bool canAggregate();
};

}  // namespace ipfixdump


#endif /* COLUMN_FORMAT_H_ */
