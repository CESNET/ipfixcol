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

ibis::table* data::select(const char *sel, const char *cond) {
	return select(sel, cond, defaultOrder);
}

/* this function won't be probably used at all */
/* TODO add management of aggreation functions */
/* TODO add aliases */
/* TODO add support for multiple parts of one column (ipv6 addr) */
ibis::table* data::select(const char *sel, const char *cond, stringVector &order) {
	ibis::table *table = NULL, *resultTable = NULL;
	stringSet selColumns;
	size_t begin = 0, end = 0;
	std::string select(sel), tmp;
	ibis::partList selectedParts;
	int numErased = 0;

	/* parse sel and get list of columns */
	while(end != std::string::npos) {
		end = select.find(',', begin);
		tmp = trim(select.substr(begin, end-begin));
		selColumns.insert(std::string(tmp));
		begin = end + 1;
	}

	/* select only parts that have matching columns */
	/* copy pointers to parts to temporary vector */
	selectedParts.assign(parts.begin(), parts.end());
	/* go over all columns of parts */
	for (size_t i=0; i < columns.size(); i++) {
		/* go over all columns in select clause */
		for (stringSet::iterator it = selColumns.begin(); it != selColumns.end(); it ++) {
			/* if part does not contain specified columns, don't use it in query*/
			if (columns[i]->find(*it) == columns[i]->end()) {
#ifdef DEBUG
				std::cerr << "Part " << parts[i]->name() << " ommited (does not have column " << *it << ")" << std::endl;
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

	/* nothing to work with */
	if (selectedParts.size() == 0) return NULL;

	/* create table of selected parts */
	table = ibis::table::create(selectedParts);

	/* run select on created table */
	resultTable = table->select(sel, cond);
	delete table;

	/* check for empty result */
	if (resultTable == NULL) {
		return NULL;
	}

	/* order table TODO predelat na string*/
	for (stringVector::iterator it = order.begin(); it != order.end(); it++) {
		resultTable->orderby((*it).c_str());
	}

	/* return table with result */
	return resultTable;
}

/* add ordering? */
/* TODO add aliases */
/* TODO add support for multiple parts of one column (ipv6 addr) */
tableVector data::aggregate(const char *sel, const char *cond) {
	ibis::table *table = NULL, *resultTable = NULL;
	tableVector tblv;

	/* create table of selected parts */
	table = ibis::table::create(parts);

	/* run select on created table */
	resultTable = table->select(sel, cond);
	delete table;

	/* check for empty result */
	if (resultTable != NULL) {
		tblv.push_back(resultTable);
	}

	/* return table with result */
	return tblv;
}

/* todo maybe a version with order by? or put order somewhere else? */
tableVector data::filter(const char* cond) {
	tableVector tables;
	ibis::table *table;
	std::string colNames;


	for (ibis::partList::iterator it = parts.begin(); it != parts.end(); it++) {
		table = ibis::table::create(**it);

		for (size_t i = 0; i < table->columnNames().size(); i++) {
			colNames += table->columnNames()[i];
			if (i != table->columnNames().size() - 1) {
				colNames += ",";
			}
		}

		tables.push_back(table->select(colNames.c_str(), cond));
		delete table;
		colNames.clear();
	}

	return tables;
}

}
