/**
 * \file printer.h
 * \author Petr Velan <petr.velan@cesnet.cz>
 * \brief Header of class for printing fastbit tables
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

#ifndef PRINTER_H_
#define PRINTER_H_

#include <cstdio>
#include <vector>
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <ibis.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "configuration.h"

/**
 * \brief Namespace of the ipfixdump utility
 */
namespace ipfixdump {

#define BYTE_STRLEN 7 /*real 3*/
#define SHORT_STRLEN 7 /*real 5*/
#define INT_STRLEN 10
#define LONG_STRLEN 10
#define FLOAT_STRLEN 10
#define DOUBLE_STRLEN 10
#define IPV6_STRLEN 30
#define COLUMN_SEPARATOR "  "

/**
 * \brief Class printing tables
 *
 * Handles output formatting
 */
class printer {
private:

	/**
	 * \brief print one row
	 *
	 * @param cur cursor poiting to the row
	 */
	void printRow(ibis::table::cursor *cur);

	/**
	 * \brief Print table header
	 */
	void printHeader();

	/**
	 * \brief Print formatted IPv4 address
	 */
	void printIPv4(uint32_t address);

	/**
	 * \brief Print formatted IPv6 address
	 */
	void printIPv6(uint64_t part1, uint64_t part2);

	/**
	 * \brief Print formatted timestamp
	 */
	void printTimestamp(uint64_t timestamp);

	/**
	 * \brief Print i-th element of current line base on its type
	 *
	 * @param cur Cursor pointing to current line
	 * @param i   Position of column in the table
	 */
	void printByType(ibis::table::cursor *cur, size_t i);

	/**
	 * \brief Returns string length based on type (for formatting)
	 *
	 * @param type column type
	 * @return
	 */
	int getStrLength(ibis::TYPE_T type);

	/**
	 * \brief Private default constructor
	 */
	printer();

	std::ostream &out; /**< Stream to write to */
	stringVector colNames; /**< Printed table column names */
	ibis::table::typeList colTypes; /**< Printed table column types */
	std::string format; /**< Print format */
	ibis::table *table; /**< table to print */

public:

	/**
	 * \brief Constructor
	 *
	 * @param out ostream to print to
	 * @param format output format
	 */
	printer(std::ostream &out, std::string format);

	/**
	 * \brief Prints output in specified format
	 *
	 * @param table table to print out
	 * @param limit print max limit rows (0 = all)
	 * @return 0 on success, non-zero otherwise
	 */
	int print(ibis::table *table, uint64_t limit);
};

}  // namespace ipfixdump


#endif /* PRINTER_H_ */
