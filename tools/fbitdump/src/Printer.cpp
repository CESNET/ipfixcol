/**
 * \file Printer.cpp
 * \author Petr Velan <petr.velan@cesnet.cz>
 * \brief Class for printing fastbit tables
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
#include <cstdlib>
#include <cstring>
#include <arpa/inet.h>
#include <netdb.h>
#include "protocols.h"
#include "Column.h"
#include "Values.h"
#include "Table.h"
#include "Resolver.h"
#include "Printer.h"
#include "Utils.h"
#include "plugins/plugin_header.h"

namespace fbitdump
{

bool Printer::print(TableManager &tm)
{
	/* if there is nothing to print, return */
	if (conf.getColumns().size() == 0) {
		return true;
	}

	this->tableManager = &tm;


	/* print table header */
	if (!conf.getQuiet()) {

		printHeader();
	}

	const Cursor *cursor;
	TableManagerCursor *tmc = tm.createCursor();
	if (tmc == NULL) {
		/* no tables, no rows */
		return true;
	}

	uint64_t numPrinted = 0;
	while (tmc->next()) {
		cursor = tmc->getCurrentCursor();
		printRow(cursor);
		numPrinted++;
	}

	delete(tmc);

	if (!conf.getQuiet()) {
		printFooter(numPrinted);
	}

	return true;
}

void Printer::printHeader() const
{
	/* check for statistics header line */
	if (conf.getStatistics()) {
		out << "Top " << conf.getMaxRecords() << " "
			<< *conf.getAggregateColumnsAliases().begin() <<  " ordered by "
			<< *conf.getOrderByColumn()->getAliases().begin() << std::endl;
	}

	/* print column names */
	for (size_t i = 0; i < conf.getColumns().size(); i++) {
		/* set defined column width */
		if (conf.getStatistics() && conf.getColumns()[i]->isSumSummary()) {
			/* widen for percentage */
			out.width(conf.getColumns()[i]->getWidth() + this->percentageWidth);
		} else {
			out.width(conf.getColumns()[i]->getWidth());
		}

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

void Printer::printFooter(uint64_t numPrinted) const
{
	out << "Total rows outputed: " << numPrinted << std::endl << "Processed " << this->tableManager->getNumParts() << " tables with ";
	Utils::formatNumber(this->tableManager->getInitRows(), out, conf.getPlainNumbers());
	out << " rows" << std::endl;

	if (conf.getExtendedStats()) {
		const TableSummary *st = this->tableManager->getSummary();

		columnVector stats = conf.getStatisticsColumns();

		for (auto col: stats) {
			if (col->isAvgSummary()) {
				out << "Average ";
			} else {
				out << "Total ";
			}

			out << col->getName() << ": ";
			std::string s = col->getSummaryType() + col->getSelectName();
			Utils::formatNumber(st->getValue(s), out, conf.getPlainNumbers());
			out << std::endl;
		}
	}
}

void Printer::printRow(const Cursor *cur) const
{
	/* go over all defined columns */
	for (size_t i = 0; i < conf.getColumns().size(); i++) {
		/* set defined column width */
		if (conf.getStatistics() && conf.getColumns()[i]->isSumSummary()) {
			/* widen for percentage */
			out.width(conf.getColumns()[i]->getWidth() + this->percentageWidth);
		} else {
			out.width(conf.getColumns()[i]->getWidth());
		}

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

const std::string Printer::printValue(const Column *col, const Cursor *cur) const
{
	static char plugin_buffer[PLUGIN_BUFFER_SIZE];

	if (col->isSeparator()) {
		return col->getName();
	}

	const Values *val = col->getValue(cur);

	/* check for missing column */
	if (val == NULL) {
		return col->getNullStr();
	}

	std::string valueStr;

	if (!col->getSemantics().empty() && (col->getSemantics() != "flows") && (col->format != NULL)) {
		plugin_arg_t arg = {.type = val->type, .val = (const plugin_arg_val *) val->value};
		col->format(&arg, (int) this->conf.getPlainNumbers(col->getSemantics()), plugin_buffer, col->pluginConf);
		valueStr.append(plugin_buffer);
	} else {
		valueStr = val->toString(conf.getPlainNumbers());

		/* when printing statistics, add percent part */
		if (conf.getStatistics() && col->isSumSummary()) {
			std::ostringstream ss;
			double sum;
			std::string name = col->getSummaryType() + col->getSelectName();
			sum = this->tableManager->getSummary()->getValue(name);

			ss.precision(1);
			ss << std::fixed << " (" << 100 * val->toDouble()/sum << "%)";
			valueStr += ss.str();
		}
	}

	/* clean value variable */
	delete val;

	/* empty the plugin_buffer */
	plugin_buffer[0] = '\0';

	return valueStr;
}

/* copy output stream and format */
Printer::Printer(std::ostream &out, Configuration &conf):
		out(out), conf(conf), tableManager(NULL), percentageWidth(8)
{}

}
