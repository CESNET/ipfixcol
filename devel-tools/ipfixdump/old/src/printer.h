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

#include "typedefs.h"
#include "configuration.h"

/**
 * \brief Namespace of the ipfixdump utility
 */
namespace ipfixdump {

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
	 * @param namesColumns map of names to column numbers for current table
	 */
	void printRow(ibis::table::cursor *cur, namesColumnsMap &namesColumns);

	/**
	 * \brief Print table header
	 */
	void printHeader();

	/**
	 * \brief Private default constructor
	 */
	printer();

	std::ostream &out; /**< Stream to write to */
	configuration &conf; /**< program configuration */

	tableVector tables; /**< tables to print */
	ibis::table::namesTypes namesTypes; /**< assoc. array of names and types */

public:

	/**
	 * \brief Constructor
	 *
	 * @param out ostream to print to
	 * @param conf configuration class
	 */
	printer(std::ostream &out, configuration &conf);

	/**
	 * \brief Prints output in specified format
	 *
	 * This is the real printer function, public ones only
	 * set tables to work with and call this.
	 *
	 * @param limit print max limit rows (0 = all)
	 * @return 0 on success, non-zero otherwise
	 */
	int print(uint64_t limit);

	/**
	 * \brief Adds table to list of tables to print
	 *
	 * @param table table_container to be added
	 */
	void addTable(tableContainer *table);

	/**
	 * \brief Add tables to list of tables to print
	 *
	 * @param tables vector of tables to be added
	 */
	void addTables(tableVector &tables);

	/**
	 * \brief clear list of tables to print
	 *
	 * @return Vector of tables preciously stored
	 */
	tableVector clearTables();
};

}  // namespace ipfixdump


#endif /* PRINTER_H_ */
