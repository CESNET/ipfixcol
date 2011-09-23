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
#include "Column.h"
#include "Printer.h"
#include "Table.h"

namespace ipfixdump
{


bool Printer::print(TableManager &tm, size_t limit)
{
	bool hasRow;
	size_t printedRows = 0;


	/* if there is nothing to print, return */
	if (conf.getColumns().size() == 0) {
		return true;
	}


	/* print table header */
	if (!conf.getQuiet()) {
		printHeader();
	}

	/* go over all tables to print */
	for (tableVector::iterator tableIt = tm.getTables().begin(); tableIt != tm.getTables().end(); tableIt++) {

		/* create cursor */
		Cursor *cur = (*tableIt)->createCursor();
		/* this should not happen */
		if (cur == NULL) return false;

		/* set default for first iteration */
		hasRow = true;

		/* print rows */
		while((limit == 0 || limit - printedRows > 0) && hasRow) {
			hasRow = cur->next(); /* make the next row ready */
			if (hasRow) {
				printRow(cur);
				printedRows++;
			}
		}

		/* free cursor */
		delete cur;
	}

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
		out << conf.getColumns()[i]->getValue(cur, conf.getPlainNumbers());
	}
	out << std::endl;
}


/* copy output stream and format */
Printer::Printer(std::ostream &out, Configuration &conf):
		out(out), conf(conf)
{}



}
