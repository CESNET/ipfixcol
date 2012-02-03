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

#include <stdexcept>
#include "Table.h"

namespace fbitdump {
/* TODO !!! translate names on second query using namesColumnsMap
 * Also take care of stripping the sum,avg,max and so on before searching and append it again after
 * aggregate Columns should be translated too
 * translation should also apply to filter */

Table::Table(ibis::part *part): queryDone(true), orderAsc(true), deleteTable(true)
{
	this->table = ibis::table::create(*part);
}

Table::Table(ibis::partList &partList): queryDone(true), orderAsc(true), deleteTable(true)
{
	this->table = ibis::table::create(partList);
}

Table::Table(Table *table): queryDone(true), orderAsc(true), deleteTable(false)
{
	this->table = table->table;
	this->namesColumns = table->namesColumns;
}

Cursor* Table::createCursor()
{
	return new Cursor(*this);
}

void Table::aggregate(const stringSet &aggregateColumns, const stringSet &summaryColumns,
		const Filter &filter)
{
	std::string colNames;
	stringPairVector combined, sColumns, aColumns;
	size_t i = 0;

	/* later queries must be made with proper column names */
	if (!this->namesColumns.empty()) { /* not first query. translate columns  */
		sColumns = translateColumns(summaryColumns, true);
		aColumns = translateColumns(aggregateColumns);
		combined.insert(combined.end(), aColumns.begin(), aColumns.end());
		combined.insert(combined.end(), sColumns.begin(), sColumns.end());
	} else { /* just copy all names to combined vector and make translations same as names */
		for (stringSet::const_iterator it = aggregateColumns.begin(); it != aggregateColumns.end(); it++) {
			combined.push_back(stringPair(*it, *it));
		}
		for (stringSet::const_iterator it = summaryColumns.begin(); it != summaryColumns.end(); it++) {
			combined.push_back(stringPair(*it, *it));
		}
	}

	/* create select string and build namesColumns map */
	this->namesColumns.clear(); /* Clear the map for new values */

	for (stringPairVector::const_iterator it = combined.begin(); it != combined.end(); it++, i++) {
		colNames += it->first;
		this->namesColumns.insert(std::pair<std::string, int>(it->second, i));
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
	stringPairVector columns;

	/* names translation */
	if (this->namesColumns.empty()) { /* first query, do not translate */
		for (stringSet::const_iterator it = columnNames.begin(); it != columnNames.end(); it++) {
			columns.push_back(stringPair(*it, *it));
		}
	} else { /* not first query, translate */
		columns = translateColumns(columnNames);
	}

	this->namesColumns.clear(); /* Clear the map for new values */

	/* create select string and build namesColumns map */
	for (size_t i = 0; i < table->columnNames().size(); i++) {
		/* ignore unused columns */
		/* check whether current column from table is in the column vector */
		for (stringPairVector::const_iterator it = columns.begin(); it != columns.end(); it++) {
			if (it->first == table->columnNames()[i]) {
				colNames += it->first;
				this->namesColumns.insert(std::pair<std::string, int>(it->second, idx++));
				colNames += ",";
				continue; /* just to be sure that columns are not repeated */
			}
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

uint64_t Table::nRows()
{
	this->doQuery();
	return this->table->nRows();
}

const ibis::table* Table::getFastbitTable()
{
	this->doQuery();
	return this->table;
}

const namesColumnsMap& Table::getNamesColumns()
{
	this->doQuery();
	return this->namesColumns;
}

const Filter* Table::getFilter()
{
	this->doQuery();
	return this->usedFilter;
}

void Table::orderBy(stringSet orderColumns, bool orderAsc)
{
	this->orderColumns = orderColumns;
	this->orderAsc = orderAsc;
}

Table* Table::createTableCopy()
{
	this->doQuery();

	if (this->table != NULL) {
		return new Table(this);
	} else {
		return NULL;
	}
}

void Table::queueQuery(std::string select, const Filter &filter)
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

		/* do order by only on valid result */
		if (this->table && this->table->nRows() && !this->orderColumns.empty()) {
			/* transform the column names to table names */
			ibis::table::stringList orderByList;
			std::vector<bool> direc;
			for (stringSet::const_iterator it = this->orderColumns.begin(); it != this->orderColumns.end(); it++) {
				try {
					int i = this->namesColumns.at(*it);
					const char *s = this->table->columnNames()[i];
					orderByList.push_back(s);
					direc.push_back(this->orderAsc);
				} catch (std::out_of_range &e) {
					std::cerr << "Cannot order by column '" << *it << "' (not present in the table)" << std::endl;
				}
			}

			/* order the table */
			this->table->orderby(orderByList, direc);
		}

		/* delete original table */
		if (this->deleteTable) {
			delete tmpTable;
		}

		queryDone = true;
	}
}

stringPairVector Table::translateColumns(const stringSet &columns, bool summary)
{
	stringPairVector result;

	/* later queries must be made with proper column names */
	for (stringSet::const_iterator it = columns.begin(); it != columns.end(); it++) {
		std::string name, function;

		if (summary) {
			int begin = it->find_first_of('(') + 1;
			int end = it->find_last_of(')');

			name = it->substr(begin, end-begin);
			function = it->substr(0, begin);
		} else {
			name = *it;
		}

		/* get location of the column */
		namesColumnsMap::const_iterator cit;
		if ((cit = this->getNamesColumns().find(name)) != this->getNamesColumns().end()) {
			if (summary) {
				result.push_back(stringPair(function + this->table->columnNames()[cit->second] + ")", *it));
			} else {
				result.push_back(stringPair(this->table->columnNames()[cit->second], *it));
			}
		} else {
			std::cerr << "Cannot find column '" << name << "'" << std::endl;
		}
	}

	return result;
}

Table::~Table()
{
	if (this->deleteTable) { /* do not delete tables not managed by this class */
		delete this->table;
	}
}

} /* end namespace fbitdump */
