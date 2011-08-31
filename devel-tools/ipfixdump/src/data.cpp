/**
 * \file data.cpp
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

#include <iostream>
#include <cstdlib>
#include <cstring>
#include <ibis.h>
#include "data.h"

namespace ipfixdump
{

void data::init(configuration &conf) {
	std::string tmp;
	ibis::part *part;
	stringSet *cols;

	/* copy default order */
	defaultOrder = conf.order;

	/* open configured parts */
	for (size_t i = 0; i < conf.tables.size(); i++) {
		for (size_t j = 0; j < conf.parts[i]->size(); ++j) {
			tmp = conf.tables[i] + "/" + conf.parts[i]->at(j);
#ifdef DEBUG
			std::cerr << "Loading table part from: " << tmp << std::endl;
#endif
			part = new ibis::part(tmp.c_str(), NULL, true);
			if (part != NULL) {
				parts.push_back(part);

				/* read available columns */
				cols = new stringSet;
				for (size_t i = 0; i < part->nColumns(); i++) {
					cols->insert(std::string(part->columnNames()[i]));
				}
				columns.push_back(cols);

				/* clear reference for next loop */
				part = NULL;
			} else {
				std::cerr << "Cannot open table part: " << tmp << std::endl;
			}
		}
	}
}

data::~data() {
	/* close all table parts */
	for (ibis::partList::iterator it = parts.begin(); it != parts.end(); it++) {
#ifdef DEBUG
		std::cerr << "Removing table: " << (*it)->name() << std::endl;
#endif
		delete *it;
	}

	/* free column names */
	for (std::vector<stringSet*>::iterator it = columns.begin(); it != columns.end(); it++) {
		delete *it;
	}
}

std::string data::trim(std::string str) {
	/* Find the first character position after excluding leading blank spaces*/
	size_t startpos = str.find_first_not_of(" \t");\
    /* Find the first character position from back */
	size_t endpos = str.find_last_not_of(" \t");

	/* if all spaces or empty return an empty string */
	if ((std::string::npos == startpos) || (std::string::npos == endpos)) {
		str = "";
	} else {
		str = str.substr(startpos, endpos - startpos + 1);
	}
	return str;
}

tableVector data::select(std::map<int, stringSet> sel, const char *cond) {
	return select(sel, cond, defaultOrder);
}



/* TODO add management of aggreation functions */
/* TODO add support for multiple parts of one column (ipv6 addr) */
tableVector data::select(std::map<int, stringSet> sel, const char *cond, stringVector &order) {
	ibis::table *table = NULL, *resultTable = NULL;
	std::string select;
	size_t pos;
	tableVector tblv;
	tableContainer *tc = NULL;
	namesColumnsMap nc;

	/* go over all groups */
	for (std::map<int, stringSet>::iterator selIt = sel.begin(); selIt != sel.end(); selIt++) {
		for (size_t partIdx = 0; partIdx < parts.size(); partIdx++) {

			/* create select statement and map of columns */
			pos = 0;
			for (stringSet::iterator it = selIt->second.begin(); it != selIt->second.end(); it++) {

				/* add only columns that are in the table */
				if (columns[partIdx]->find(*it) != columns[partIdx]->end()) {

					/* add column to select statement */
					select += *it;
					/* save mapping of column name to its position */
					nc[*it] = pos;
					if (pos + 1 < selIt->second.size()) {
						select += ", ";
					}
					pos++;
				} else {
#ifdef DEBUG
					std::cerr << "Part " << parts[partIdx]->name() << " does not have column " << *it << std::endl;
#endif
				}
			}

			/* create table from part */
			table = ibis::table::create(*parts[partIdx]);

			/* run select on created table */
			resultTable = table->select(select.c_str(), cond);
			delete table;

			/* check for empty result */
			if (resultTable != NULL) {
				/* order table TODO predelat na string*/
				for (stringVector::iterator it = order.begin(); it != order.end(); it++) {
					resultTable->orderby((*it).c_str());
				}

				tc = new tableContainer();
				tc->table = resultTable;
				tc->namesColumns = nc;
				tblv.push_back(tc);
			}

			/* clear selected columns */
			select.clear();
			nc.clear();
		}
	}

	/* return table with result */
	return tblv;
}

/* add ordering? */
/* TODO add support for multiple parts of one column (ipv6 addr) */
tableVector data::aggregate(std::map<int, stringSet> sel, const char *cond) {
	ibis::table *table = NULL, *resultTable = NULL;
		std::string select;
		ibis::partList selectedParts;
		int numErased;
		size_t pos;
		tableVector tblv;
		tableContainer *tc = NULL;
		namesColumnsMap nc;

		for (std::map<int, stringSet>::iterator setIt = sel.begin(); setIt != sel.end(); setIt++) {


#ifdef DEBUG
		std::cerr << "Used columns: " << std::endl;
#endif
			/* create select statement and map of columns */
			pos = 0;
			for (stringSet::iterator it = setIt->second.begin(); it != setIt->second.end(); it++, pos++) {
				/* add column to select statement */
				select += *it;
				/* save mapping of column name to its position */
				std::string col;
				int begin, end;
				if ((begin = (*it).find('(')) != std::string::npos) {
					end = (*it).find(')');
					col = (*it).substr(begin+1, end-begin-1);
				} else {
					col = *it;
				}
				nc[col] = pos;
				if (pos + 1 < setIt->second.size()) {
					select += ", ";
				}
#ifdef DEBUG
				std::cerr << "  '" << *it << "' -> " << pos << std::endl;
#endif
			}

			/* select only parts that have matching columns */
			/* copy pointers to parts to temporary vector */
			selectedParts.assign(parts.begin(), parts.end());
			numErased = 0;
			/* go over all columns of parts */
			for (size_t i=0; i < columns.size(); i++) {
				/* go over all columns in select clause */
				for (stringSet::iterator it = setIt->second.begin(); it != setIt->second.end(); it ++) {
					/* if part does not contain specified columns, don't use it in query*/
					std::string col;
					int begin, end;
					if ((begin = (*it).find('(')) != std::string::npos) {
						end = (*it).find(')');
						col = (*it).substr(begin+1, end-begin-1);
					} else {
						col = *it;
					}

					/* allow count(*) for flows column */
					if (col == "*") continue;

					if (columns[i]->find(col) == columns[i]->end()) {
#ifdef DEBUG
						std::cerr << "Part " << parts[i]->name() << " ommited (does not have column " << col << ")" << std::endl;
#endif
						selectedParts.erase(selectedParts.begin() + i - numErased);
						numErased++;
						break;
					}
				}
			}
#ifdef DEBUG
			std::cerr << "Using " << selectedParts.size() << " of " << parts.size() << " parts" << std::endl;
#endif

			/* check that we have something to work with */
			if (selectedParts.size() != 0) {

				/* create table of selected parts */
				table = ibis::table::create(selectedParts);

				/* run select on created table */
				resultTable = table->select(select.c_str(), cond);
				delete table;

				/* check for empty result */
				if (resultTable != NULL) {
					tc = new tableContainer();
					tc->table = resultTable;
					tc->namesColumns = nc;
					tblv.push_back(tc);
				}
			}

			/* clear selected columns */
			select.clear();
			nc.clear();
			selectedParts.clear();
		}

		/* return table with result */
		return tblv;
}

/* TODO maybe a version with order by? or put order somewhere else? or maybe use select for plain filtering?*/
tableVector data::filter(const char* cond) {
	tableVector tables;
	ibis::table *table;
	std::string colNames;
	tableContainer *tc;
	namesColumnsMap nc;


	for (ibis::partList::iterator it = parts.begin(); it != parts.end(); it++) {
		table = ibis::table::create(**it);

		for (size_t i = 0; i < table->columnNames().size(); i++) {
			colNames += table->columnNames()[i];
			nc.insert(std::pair<std::string, int>(table->columnNames()[i], i));
			if (i != table->columnNames().size() - 1) {
				colNames += ",";
			}
		}

		tc = new tableContainer;
		tc->namesColumns = nc;
		tc->table = table->select(colNames.c_str(), cond);

		/* use only if something was returned */
		if (tc->table != NULL) {
			tables.push_back(tc);
		}

		delete table;
		colNames.clear();
		nc.clear();
	}

	return tables;
}

}
