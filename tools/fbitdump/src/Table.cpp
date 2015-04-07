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

static const Filter emptyFilter;

Table::Table(ibis::part *part): usedFilter(NULL), queryDone(true), orderAsc(true), deleteTable(true)
{
	this->table = ibis::table::create(*part);
}

Table::Table(ibis::partList &partList): usedFilter(NULL), queryDone(true), orderAsc(true), deleteTable(true)
{
	this->table = ibis::table::create(partList);
}

Table::Table(Table *table): usedFilter(NULL), queryDone(true), orderAsc(true), deleteTable(false)
{
	this->table = table->table;
}

Cursor* Table::createCursor()
{
	return new Cursor(*this);
}

std::string Table::createSelect(const columnVector columns, bool summary, bool has_flows)
{
	std::stringstream select;
	
	for (auto col: columns) {
		if (summary) {
			/* Database already know flows column (it was aggregated), summarize it */
			if (has_flows && col->getSemantics() == "flows") {
				select << "sum(" << col->getSelectName() << ") as " << col->getSummaryType() << col->getSelectName() << ",";
			} else if (!col->getSummaryType().empty()) {
				select << col->getSummaryType() << "(" << col->getSelectName() << ") " << " as " << col->getSummaryType() << col->getSelectName() << ",";
			}
		} else if (col->getParts() > 1) {
			for (int i = 0; i < col->getParts(); ++i) {
				select << col->getElement() << "p" << i << ",";
			}
		} else if (col->getElement() == col->getSelectName()) {
			select << col->getElement() << ",";
		} else {
			select << col->getElement() << " as " << col->getSelectName() << ",";
		}
	}
	
	return select.str().substr(0, select.str().length() - 1);
}

stringSet Table::getColumnsNames(const columnVector &columns)
{
	stringSet names;
	
	for (auto col: columns) {
		for (auto cname: col->getColumns()) {
			names.insert(cname);
		}
	}
	
	return names;
}

columnVector Table::getColumnsByNames(const columnVector columns, const stringSet names)
{
	columnVector cols;
	
	for (auto col: columns) {
		bool isThere = true;
		for (auto name: col->getColumns()) {
			if (names.find(name) == names.end()) {
				isThere = false;
				break;
			}
		}
		if (isThere) {
			cols.push_back(col);
		}
	}
	
	return cols;
}

void Table::aggregateWithFunctions(const columnVector& aggregateColumns, const columnVector& summaryColumns, const Filter& filter)
{
	stringSet cols;
	
	/* Get set of columns names with their aggregation functions */
	for (auto col: aggregateColumns) {
		for (auto name: col->getColumns()) {
			cols.insert(name);
		}
	}
	
	for (auto col: summaryColumns) {
		for (auto name: col->getColumns()) {
			cols.insert(name);
		}
	}
	
	/* Create select clause */
	bool flows = false;
	std::string select;
	for (auto name: cols) {
		int begin = name.find_first_of('(') + 1;
		int end = name.find_first_of(')');
		std::string tmp = name.substr(begin, end-begin);
		if (tmp != "*") { /* ignore column * used for flows aggregation */
			select += name + " as " + tmp + ", ";
		} else {
			flows = true;
		}
	}

	/* Add aggregation */
	select += "count(*) as flows, ";

	select = select.substr(0, select.length() - 1);
	
	/* Create table */
	queueQuery(select.c_str(), filter);
	
	/* Aggregate created table */
	columnVector aCols, sCols;
	for (auto col: aggregateColumns) {
		if (col->getSemantics() != "flows") {
			aCols.push_back(col);
		}
	}
	
	for (auto col: summaryColumns) {
		if (col->getSemantics() != "flows") {
			sCols.push_back(col);
		}
	}
	
	aggregate(aCols, sCols, emptyFilter, false, flows);
}

void Table::aggregate(const columnVector &aggregateColumns, const columnVector &summaryColumns, const Filter &filter, bool summary, bool select_flows)
{
	/* check for empty table */
	if (!this->table || this->table->nRows() == 0) {
		return;
	}
	
	bool has_flows = false;
	for (auto col: table->columnNames()) {
		if (std::string(col) == "flows") {
			has_flows = true;
			break;
		}
	}
	
	std::string select = createSelect(aggregateColumns, summary, has_flows);
	if (!select.empty()) {
		select += ", ";
	}
	if (select_flows) {
		select += "flows, ";
	}
	
	select += createSelect(summaryColumns, summary, has_flows);

	/* add query */
	queueQuery(select.c_str(), filter);
}

void Table::filter(columnVector columns, Filter &filter)
{
	if (!this->table || this->table->nRows() == 0) {
		return;
	}
	
	stringSet oldnames = getColumnsNames(columns), names;
	
	for (auto name: oldnames) {
		for (auto tname: table->columnNames()) {
			if (tname == name) {
				names.insert(name);
			}
		}
	}
	
	columnVector cols = getColumnsByNames(columns, names);
	
	std::string select = createSelect(cols);
	
	queueQuery(select.c_str(), filter);
}

void Table::filter(Filter& filter)
{
	if (!this->table || this->table->nRows() == 0) {
		return;
	}
	
	/* We need to apply column aliases from previous query */
	this->doQuery();

	/* SELECT * FROM <table> WHERE <filter> */
	if (!this->table || this->table->nRows() == 0) {
		return;
	}

	std::stringstream ss;
	this->table->dumpNames(ss);
	
	queueQuery(ss.str().c_str(), filter);
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
		bool missing = false;
		/* do select */
		ibis::table *tmpTable = this->table;
		this->table = this->table->select(this->select.c_str(), this->usedFilter->getFilter().c_str());
		
		/* Check that we have valid table */
		if (!this->table) {
			throw std::runtime_error("Select '"+ this->select + "' with filter '"+ this->usedFilter->getFilter() +"' failed");
		}

		/* do order by only on valid result */
		if (this->table && this->table->nRows() && !this->orderColumns.empty()) {
			/* transform the column names to table names */
			ibis::table::stringList orderByList;
			std::vector<bool> direc;
			for (stringSet::const_iterator it = this->orderColumns.begin(); it != this->orderColumns.end(); it++) {
				bool isThere = false;
				for (auto tabcol: this->table->columnNames()) {
					if ((*it) == tabcol) {
						isThere = true;
						orderByList.push_back(tabcol);
						direc.push_back(this->orderAsc);
					}
				}
				
				if (!isThere) {
					missing = true;
					break;
				}
			}
			/* Column for ordering is missing - we don't need this table */
			if (missing) {
				delete this->table;
				this->table = NULL;
			} else {
				/* order the table */
				this->table->orderby(orderByList, direc);
			}
		}

		/* delete original table */
		if (this->deleteTable) {
			delete tmpTable;
		}

		/* the new table is our responsibility */
		this->deleteTable = true;

		queryDone = true;
	}
}

Table::~Table()
{
	if (this->deleteTable) { /* do not delete tables not managed by this class */
		delete this->table;
	}
}

} /* end namespace fbitdump */
