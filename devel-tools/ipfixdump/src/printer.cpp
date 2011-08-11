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


	/* if there is nothing to print, return */
	if (conf.columnsFormat.size() == 0) {
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

	return ierr;
}

void printer::printHeader() {
	/* print column names */
	for (size_t i = 0; i < conf.columnsFormat.size(); i++) {
		/* set defined column width */
		out.width(conf.columnsFormat[i]->width);

		/* set defined column alignment */
		if (conf.columnsFormat[i]->alignLeft) {
			out.setf(std::ios_base::left, std::ios_base::adjustfield);
		} else {
			out.setf(std::ios_base::right, std::ios_base::adjustfield);
		}

		out << conf.columnsFormat[i]->getName();
	}
	/* new line */
	out << std::endl;

}

/* TODO it might be faster to remember column positions for each name and table */
void printer::printRow(ibis::table::cursor *cur) {

	/* go over all defined columns */
	for (size_t i = 0; i < conf.columnsFormat.size(); i++) {
		/* set defined column width */
		out.width(conf.columnsFormat[i]->width);

		/* set defined column alignment */
		if (conf.columnsFormat[i]->alignLeft) {
			out.setf(std::ios_base::left, std::ios_base::adjustfield);
		} else {
			out.setf(std::ios_base::right, std::ios_base::adjustfield);
		}
		out << conf.columnsFormat[i]->getValue(*cur, namesTypes, conf.plainNumbers);
	}
	out << std::endl;
}


/* copy output stream and format */
printer::printer(std::ostream &out, configuration &conf):
		out(out), conf(conf)
{}

}
