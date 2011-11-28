/**
 * \file TableManager.cpp
 * \author Petr Velan <petr.velan@cesnet.cz>
 * \brief Class for managing table parts and tables
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

#include "TableManager.h"
#include <algorithm>
#include <fastbit/ibis.h>

namespace fbitdump {

void TableManager::aggregate(stringSet aggregateColumns, stringSet summaryColumns,
		Filter &filter)
{

	std::vector<stringSet> colIntersect;
	stringSet partCols;
	Table *table;
	ibis::partList parts; /* this overrides class attribute parts */

	/* omit parts that don't have necessary summary columns */
	/* strip summary columns of aggregation functions to get plain names */
	stringSet sCols;
	for (stringSet::iterator it = summaryColumns.begin(); it != summaryColumns.end(); it++) {
		int begin = it->find_first_of('(') + 1;
		int end = it->find_first_of(')');

		std::string tmp = it->substr(begin, end-begin);
		if (tmp != "*") { /* ignore column * used for flows aggregation */
			sCols.insert(it->substr(begin, end-begin));
		}
	}

	/* filter out parts */
	for (size_t i = 0; i < this->parts.size(); i++) {
		/* put columns from part to set */
		stringSet partColumns;
		for (size_t j = 0; j < this->parts[i]->columnNames().size(); j++) {
			partColumns.insert(this->parts[i]->columnNames()[j]);
		}

		/* compute set difference */
		stringSet difference;
		std::set_difference(sCols.begin(), sCols.end(), partColumns.begin(),
				partColumns.end(), std::inserter(difference, difference.begin()));

		/* When all summary columns are in current part, difference is empty */
		if (difference.empty()) {
			parts.push_back(this->parts[i]);
		} else {
			std::cerr << "Ommiting part [" << i << "], does not have column '" << *difference.begin() << "'" << std::endl;
		}
	}

	/* go over all parts and build vector of intersection between part columns and aggregation columns */
	for (size_t i = 0; i < parts.size(); i++) {

		/* put part columns to set */
		for (size_t j = 0; j < parts[i]->columnNames().size(); j++) {
			partCols.insert(parts[i]->columnNames()[j]);
		}

		/* initialize intersection result stringSet */
		colIntersect.push_back(stringSet());

		/* make an intersection */
		std::set_intersection(partCols.begin(), partCols.end(), aggregateColumns.begin(),
				aggregateColumns.end(), std::inserter(colIntersect[i], colIntersect[i].begin()));

#ifdef DEBUG
		std::cerr << "Intersection has " << colIntersect[i].size() << " columns" << std::endl;

		std::cerr << "Intersect columns: ";
		for (stringSet::iterator it = colIntersect[i].begin(); it != colIntersect[i].end(); it++) {
			std::cerr << *it << ", ";
		}
		std::cerr << std::endl;
#endif

		/* cleanup for next iteration */
		partCols.clear();
	}

	/* group parts with same intersection to one table */
	ibis::partList pList;
	size_t partsCount = parts.size();
	bool used[partsCount];
	int iterPos = 0;

	/* initialise used array */
	for (size_t i = 0; i < partsCount; i++) {
		used[i] = false;
	}

	/* go over all parts (theirs intersections), empty intersections are ignored */
	for (std::vector<stringSet>::iterator outerIter = colIntersect.begin(); outerIter != colIntersect.end(); outerIter++) {
		/* work with current intersection only if it has not been used before and if it is not empty */
		if (used[iterPos] || outerIter->size() == 0	) {
			iterPos++;
			continue;
		}

		/* add current part */
		used[iterPos] = true;
		pList.push_back(parts.at(iterPos));
		int curPos = iterPos;

		/* add all parts that have same columns as current part and are not already used */
		for (std::vector<stringSet>::iterator it = outerIter; it != colIntersect.end(); it++) {
			if (used[curPos]) {
				curPos++;
				continue;
			}

			/* compute set difference */
			stringSet difference;
			std::set_symmetric_difference(outerIter->begin(), outerIter->end(), it->begin(),
					it->end(), std::inserter(difference, difference.begin()));

			/* When sets are equal, difference is empty */
			if (difference.empty()) {
				/* add table to list */
				pList.push_back(parts.at(curPos));
				used[curPos] = true;
			}
			/* don't forget to increment the counter */
			curPos++;
		}


#ifdef DEBUG
		std::cerr << "Creating table from " << pList.size() << " part(s)" << std::endl;

		std::cerr << "[" << iterPos << "]Aggregate columns: ";
		for (stringSet::iterator it = outerIter->begin(); it != outerIter->end(); it++) {
			std::cerr << *it << ", ";
		}
		std::cerr << std::endl;
#endif

		/* create table for each partList */
		table = new Table(pList);

		/* aggregate the table, use only present aggregation columns */
		table->aggregate(*outerIter, summaryColumns, filter);
		this->tables.push_back(table);

		/* and clear the part list */
		pList.clear();
		iterPos++;
	}
}

void TableManager::filter(Filter &filter)
{
	Table *table;

	stringSet columnNames;
	for (columnVector::iterator it = conf.getColumns().begin(); it != conf.getColumns().end(); it++) {
		/* don't add flows as count(*) */
		if (!(*it)->isSeparator() && (*it)->getSemantics() == "flows") {
			continue;
		}
		stringSet tmp = (*it)->getColumns();
		if (tmp.size() > 0) {
			columnNames.insert(tmp.begin(), tmp.end());
		}
	}

	/* go over all parts */
	for (ibis::partList::iterator it = this->parts.begin(); it != this->parts.end(); it++) {

		/* create table for each part */
		table = new Table(*it);

		/* add to managed tables */
		table->filter(columnNames, filter);
		this->tables.push_back(table);

#ifdef DEBUG
		std::cerr << "Created new table, MB in use: " << ibis::fileManager::bytesInUse()/1000000 << std::endl;
#endif
	}
}

TableManagerCursor *TableManager::createCursor()
{
	/* create cursor only if we have some table */
	if (this->tables.size() == 0) {
		return NULL;
	}

	TableManagerCursor *tableManagerCursor = new TableManagerCursor(*this, this->conf);

	return tableManagerCursor;
}

tableVector& TableManager::getTables()
{
	return tables;
}

void TableManager::removeTable(tableVector::iterator &it)
{
	delete *it;
	tables.erase(it--);
}

TableManager::TableManager(Configuration &conf): conf(conf)
{
	std::string tmp;
	ibis::part *part;

	/* open configured parts */
	for (size_t i = 0; i < this->conf.getPartsNames().size(); i++) {
		tmp = this->conf.getPartsNames()[i];
#ifdef DEBUG
		std::cerr << "Loading table part from: " << tmp << std::endl;
#endif
		part = new ibis::part(tmp.c_str(), NULL, true);
		if (part != NULL) {
			this->parts.push_back(part);

			/* clear reference for next loop */
			part = NULL;
		} else {
			std::cerr << "Cannot open table part: " << tmp << std::endl;
		}
	}
}

TableManager::~TableManager()
{
	/* delete all tables */
	for (tableVector::iterator it = this->tables.begin(); it != this->tables.end(); it++) {
		delete *it;
	}

	/* delete all table parts */
	for (ibis::partList::iterator it = this->parts.begin(); it != this->parts.end(); it++) {
		delete *it;
	}
}

}  // namespace fbitdump


