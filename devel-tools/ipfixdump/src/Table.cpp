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

#include "Table.h"

namespace ipfixdump {

Table::Table(ibis::part *part) {
	this->table = ibis::table::create(*part);
}

Table::Table(ibis::partList &partList) {
	this->table = ibis::table::create(partList);
}

Cursor* Table::createCursor() {
	/* check for NULL table */
	if (this->table == NULL) {
		return NULL;
	}

	return new Cursor(*this);
}

bool Table::aggregate(stringSet &aggregateColumns, stringSet &summaryColumns,
		Filter &filter) {
	ibis::table *tmpTable = this->table;
	std::string colNames;
	stringSet combined;
	size_t i = 0;

	/* Save the filter */
	//this->usedFilter = &filter; TODO check this

	/* create select string and build namesColumns map */
	combined.insert(aggregateColumns.begin(), aggregateColumns.end());
	combined.insert(summaryColumns.begin(), summaryColumns.end());

	for (stringSet::iterator it = combined.begin(); it != combined.end(); it++, i++) {
		colNames += *it;
		this->namesColumns.insert(std::pair<std::string, int>(*it, i));
		if (i != combined.size() - 1) {
			colNames += ",";
		}
	}

	/* do select */
	this->table = this->table->select(colNames.c_str(), filter.getFilter().c_str());

	/* delete original table */
	delete tmpTable;

	if (this->table == NULL) return false;

	return true;
}

bool Table::filter(Filter &filter) {
	ibis::table *tmpTable = this->table;
	std::string colNames;

	/* Save the filter */
	this->usedFilter = &filter;

	/* create select string and build namesColumns map */
	for (size_t i = 0; i < table->columnNames().size(); i++) {
		colNames += table->columnNames()[i];
		this->namesColumns.insert(std::pair<std::string, int>(table->columnNames()[i], i));
		if (i != table->columnNames().size() - 1) {
			colNames += ",";
		}
	}

	/* do select */
	this->table = this->table->select(colNames.c_str(), filter.getFilter().c_str());

	/* delete original table */
	delete tmpTable;

	if (this->table == NULL) return false;

	return true;
}

size_t Table::nRows() {
	return this->table->nRows();
}

ibis::table* Table::getFastbitTable() {
	return this->table;
}

namesColumnsMap& Table::getNamesColumns() {
	return this->namesColumns;
}

Filter& Table::getFilter() {
	return *this->usedFilter;
}

Table::~Table() {
	delete this->table;
}

} /* end namespace ipfixdump */
