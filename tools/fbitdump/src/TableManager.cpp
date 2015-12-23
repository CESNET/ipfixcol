/**
 * \file TableManager.cpp
 * \author Petr Velan <petr.velan@cesnet.cz>
 * \brief Class for managing table parts and tables
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

#include "TableManager.h"
#include <algorithm>
#include <fastbit/ibis.h>

namespace fbitdump {

void TableManager::aggregate(columnVector aggregateColumns, columnVector summaryColumns, Filter &filter)
{
	std::vector<stringSet> colIntersect;
	stringSet aCols, partCols;
	
	/* get names (eXidYYY) of all aggregating columns */
	for (auto col: aggregateColumns) {
		for (auto name: col->getColumns()) {
			aCols.insert(name);
		}
	}
	
	Table *table;
	ibis::partList parts;
	size_t size = 0;
	
	/* get names (eXidYYY) of all summary columns */
	stringSet sCols;
	for (auto col: summaryColumns) {
		for (auto name: col->getColumns()) {
			int begin = name.find_first_of('(') + 1;
			int end = name.find_first_of(')');
			std::string tmp = name.substr(begin, end-begin);
			if (tmp != "*") { /* ignore column * used for flows aggregation */
				sCols.insert(tmp);
			}
		}
	}

	size = this->parts.size();
	
	/* filter out parts without summary columns */
	for (size_t i = 0; i < this->parts.size(); ++i) {
		stringSet partColumns;
		Utils::progressBar( "Aggregating [1/2]  ", "   ", size, i );
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
			std::cerr << "Ommiting part " << this->parts[i]->currentDataDir() << ", does not have column '" << *difference.begin() << "'" << std::endl;
		}
	}
	
	size = parts.size();
	/* go over all parts and build vector of intersection between part columns and aggregation columns */
	/* put together the parts that have same intersection - this ensures for example that ipv4 and ipv6 are aggregate separately by default */
	for (size_t i = 0; i < parts.size(); i++) {

		/* put part columns to set */
		for (size_t j = 0; j < parts[i]->columnNames().size(); j++) {
			partCols.insert(parts[i]->columnNames()[j]);
		}

		/* initialize intersection result stringSet */
		colIntersect.push_back(stringSet());

		/* make an intersection */
		std::set_intersection(partCols.begin(), partCols.end(), aCols.begin(),
				aCols.end(), std::inserter(colIntersect[i], colIntersect[i].begin()));

#ifdef DEBUG
		std::cerr << "Intersection has " << colIntersect[i].size() << " columns" << std::endl;

		std::cerr << "Intersect columns: ";
		for (stringSet::const_iterator it = colIntersect[i].begin(); it != colIntersect[i].end(); it++) {
			std::cerr << *it << ", ";
		}
		std::cerr << std::endl;
#endif

		/* cleanup for next iteration */
		partCols.clear();

		Utils::progressBar( "Aggregating [2/2]  ", "   ", size, i );
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
	for (std::vector<stringSet>::const_iterator outerIter = colIntersect.begin(); outerIter != colIntersect.end(); outerIter++) {
		/* work with current intersection only if it has not been used before */
		/* empty intersections are allowed - we might want to show sum of data that do not match (or query sum(pkt)) */
		if (used[iterPos]) {
			iterPos++;
			continue;
		}

		/* add current part */
		used[iterPos] = true;
		pList.push_back(parts.at(iterPos));
		int curPos = iterPos;

		/* add all parts that have same columns as current part and are not already used */
		for (std::vector<stringSet>::const_iterator it = outerIter; it != colIntersect.end(); it++) {
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
				/* check that parts have same column types for aggregate columns and issue a warning if not */
				/* go over aggregate columns and check the types */ /* TODO check that this is not needed for summary columns */
				for (stringSet::const_iterator strIter = (*outerIter).begin(); strIter != (*outerIter).end(); strIter++) {
					int pos1 = -1, pos2 = -1;

					/* find index of the column in both parts */
					for (unsigned int i = 0; i< parts.at(iterPos)->columnTypes().size(); i++) {
						if (*strIter == parts.at(iterPos)->columnNames()[i]) {
							pos1 = i;
							break;
						}
					}
					for (unsigned int i = 0; i< parts.at(curPos)->columnTypes().size(); i++) {
						if (*strIter == parts.at(curPos)->columnNames()[i]) {
							pos2 = i;
							break;
						}
					}

					/*
					   The columns with given name always exist, so there is no real need to check for '-1' value.
					   This is merely done to avoid programming errors.
					*/
					if (pos1 < 0 || pos2 < 0) {
						std::cerr << "Error: an unexpected error occurred while verifying data types!" << std::endl;
						break;
					}

					/* test that data types are same */
					if (parts.at(iterPos)->columnTypes()[pos1] != parts.at(curPos)->columnTypes()[pos2]) {
						std::cerr << "Warning: column '" << *strIter << "' has different data types in different parts! ("
								<< parts.at(iterPos)->name() << ", " << parts.at(curPos)->name() << ")" << std::endl;
					}
				}

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
		for (stringSet::const_iterator it = outerIter->begin(); it != outerIter->end(); it++) {
			std::cerr << *it << ", ";
		}
		std::cerr << std::endl;
#endif

		/* create table for each partList */
		if (!outerIter->empty() || aggregateColumns.empty()) {
			table = new Table(pList);

			columnVector aggCols;
			for (auto col: aggregateColumns) {
				bool isThere = true;
				for (auto name: col->getColumns()) {
					if (outerIter->find(name) == outerIter->end()) {
						isThere = false;
						break;
					}
				}
				if (isThere) {
					aggCols.push_back(col);
				}
			}

			/* aggregate the table, use only present aggregation columns */
			table->aggregateWithFunctions(aggCols, summaryColumns, filter);
			table->orderBy(this->orderColumns, this->orderAsc);
			this->tables.push_back(table);
		}

		/* and clear the part list */
		pList.clear();
		iterPos++;
	}
}


void TableManager::postAggregateFilter(Filter& filter)
{
	for (auto table: this->tables) {
		table->filter(filter);
	}
}

void TableManager::filter(Filter &filter, bool postAggregate)
{
	if (postAggregate) {
		this->postAggregateFilter(filter);
		return;
	}
	
	Table *table;
	int size = conf.getColumns().size();
	int i = 0;
	
	columnVector columns;
	
	for (auto col: conf.getColumns()) {
		if (col->isSeparator() || col->getSemantics() == "flows") {
			continue;
		}

		columns.push_back(col);
	}
	
	//Utils::progressBar( "Initializing filter", "DONE", 1, 1 );
	//std::cout.flush();
	
	/* go over all parts */

	i = 0;
	size = this->parts.size();
	for (ibis::partList::const_iterator it = this->parts.begin(); it != this->parts.end(); it++) {
		Utils::progressBar( "Applying filter    ", "   ", size, i );
		i++;
		
		/* create table for each part */
		table = new Table(*it);

		/* add to managed tables */
		table->filter(columns, filter);
		table->orderBy(this->orderColumns, this->orderAsc);
		this->tables.push_back(table);

#ifdef DEBUG
		std::cerr << "Created new table, MB in use: " << ibis::fileManager::bytesInUse()/1000000 << std::endl;
#endif
	}
	//Utils::progressBar( "Applying filter    ", "DONE", size, i );
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

uint64_t TableManager::getNumParts() const
{
	return this->parts.size();
}

uint64_t TableManager::getInitRows() const
{
	uint64_t ret = 0;
	for (ibis::partList::const_iterator it = this->parts.begin(); it != this->parts.end(); it++) {
		ret += (*it)->nRows();
	}
	return ret;
}

TableManager::TableManager(Configuration &conf): conf(conf), orderAsc(false), tableSummary(NULL)
{
	std::string tmp;
	ibis::part *part;
	size_t size = this->conf.getPartsNames().size();
	/* open configured parts */
	for (size_t i = 0; i < this->conf.getPartsNames().size(); i++) {
		
		tmp = this->conf.getPartsNames()[i];
#ifdef DEBUG
		std::cerr << "Loading table part from: " << tmp << std::endl;
#endif
		part = new ibis::part(tmp.c_str(), true);
		if (part != NULL) {
			this->parts.push_back(part);

			Utils::progressBar("Initializing tables", tmp.c_str(), size, i );

			/* clear reference for next loop */
			part = NULL;
		} else {
			std::cerr << "Cannot open table part: " << tmp << std::endl;
		}

		
	}
	/* create order by string list if necessary */
	if (conf.getOptionm()) {
		this->orderColumns.insert(conf.getOrderByColumn()->getSelectName());
		this->orderAsc = conf.getOrderAsc();
	}
}

const TableSummary* TableManager::getSummary()
{
	if (this->tableSummary == NULL) {
		this->tableSummary = new TableSummary(this->tables, conf.getSummaryColumns());
		
//		columnVector columns;
//		columnVector summaryColumns = conf.getSummaryColumns();
		
//		for (auto col: summaryColumns) {
//			if (col->getSemantics() == "flows") {
//				columns.insert(col->getElement());
//			} else {
//				columns.insert(col->getSummaryType() + "(" + col->getSelectName() + ")");
//			}
//		}
		
//		this->tableSummary = new TableSummary(this->tables, columns);
	}
	
	return this->tableSummary;
}

ibis::partList TableManager::getParts()
{
	return this->parts;
}

TableManager::~TableManager()
{
	/* delete all tables */
	for (tableVector::const_iterator it = this->tables.begin(); it != this->tables.end(); it++) {
		delete *it;
	}

	/* delete all table parts */
	for (ibis::partList::const_iterator it = this->parts.begin(); it != this->parts.end(); it++) {
		delete *it;
	}

	if (this->tableSummary != NULL) {
		delete this->tableSummary;
	}
}

}  // namespace fbitdump


