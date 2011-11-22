/**
 * \file Table.cpp
 * \author Petr Velan <petr.velan@cesnet.cz>
 * \brief Class wrapping ibis::table
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

#include <exception>
#include "Table.h"

namespace fbitdump {

Table::Table(ibis::part *part): queryDone(true)
{
	this->table = ibis::table::create(*part);
}

Table::Table(ibis::partList &partList): queryDone(true)
{
	this->table = ibis::table::create(partList);
}

Cursor* Table::createCursor()
{
	return new Cursor(*this);
}

void Table::aggregate(stringSet &aggregateColumns, stringSet &summaryColumns,
		Filter &filter)
{
	std::string colNames;
	stringSet combined;
	size_t i = 0;
	stringSet sColumns;

	/* later queries must be made with proper column names */
	if (namesColumns.size() > 0) { /* not first query */
		for (stringSet::iterator it = summaryColumns.begin(); it != summaryColumns.end(); it++) {
			/* get location of the column */
			namesColumnsMap::iterator cit;
			if ((cit = this->getNamesColumns().find(*it)) != this->getNamesColumns().end()) {
				sColumns.insert(this->table->columnNames()[cit->second]);
			} else {
				std::cerr << "Cannot find column '" << *it << "'" << std::endl;
			}
		}
	} else {
		sColumns = summaryColumns;
	}

	/* create select string and build namesColumns map */
	combined.insert(aggregateColumns.begin(), aggregateColumns.end());
	combined.insert(sColumns.begin(), sColumns.end());

	for (stringSet::iterator it = combined.begin(); it != combined.end(); it++, i++) {
		colNames += *it;
		this->namesColumns.insert(std::pair<std::string, int>(*it, i));
		if (i != combined.size() - 1) {
			colNames += ",";
		}
	}

	/* add query */
	queueQuery(colNames.c_str(), filter);
}

void Table::filter(stringSet columnNames, Filter &filter)
{
	std::string colNames;
	size_t idx = 0;

	/* create select string and build namesColumns map */
	for (size_t i = 0; i < table->columnNames().size(); i++) {
		/* ignore unused columns */
		if (columnNames.find(table->columnNames()[i]) != columnNames.end()) {
			colNames += table->columnNames()[i];
			this->namesColumns.insert(std::pair<std::string, int>(table->columnNames()[i], idx++));
			colNames += ",";
		}
	}

	/* remove trailing comma */
	colNames = colNames.substr(0, colNames.size()-1);

#ifdef DEBUG
	uint64_t min, max;
	this->table->estimate(filter.getFilter().c_str(), min, max);
	std::cerr << "Estimating between " << min << " and " << max << " records" << std::endl;
#endif

	/* add query */
	queueQuery(colNames.c_str(), filter);
}

size_t Table::nRows()
{
	this->doQuery();
	return this->table->nRows();
}

ibis::table* Table::getFastbitTable()
{
	this->doQuery();
	return this->table;
}

namesColumnsMap& Table::getNamesColumns()
{
	this->doQuery();
	return this->namesColumns;
}

Filter* Table::getFilter()
{
	this->doQuery();
	return this->usedFilter;
}

void Table::queueQuery(std::string select, Filter &filter)
{
	/* Run any previous query */
	this->doQuery();

	this->select = select;
	this->usedFilter = &filter;
	this->queryDone = false;
}

void Table::doQuery()
{
	if (!queryDone) {
		/* do select */
		ibis::table *tmpTable = this->table;
		this->table = this->table->select(this->select.c_str(), this->usedFilter->getFilter().c_str());

		/* delete original table */
		delete tmpTable;

		queryDone = true;
	}
}

Table::~Table()
{
	delete this->table;
}

} /* end namespace fbitdump */
