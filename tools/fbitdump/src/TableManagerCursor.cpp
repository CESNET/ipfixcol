/**
 * \file TableManagerCursor.cpp
 * \author Petr Velan <petr.velan@cesnet.cz>
 * \brief Global cursor over all Tables
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

#include "TableManagerCursor.h"

namespace fbitdump {


TableManagerCursor::TableManagerCursor(TableManager &tableManager, Configuration &conf):
		currentTableIt(tableManager.getTables().begin())
{
	this->tableManager = &tableManager;
	this->conf = &conf;

	this->currentCursor = NULL;
	this->cursorIndex = 0;

	/* build list of all cursors only with m option */
	if (this->conf->getOptionm()) {
		/* get table cursors */
		if (this->getTableCursors() == false) {
			std::cerr << "Unable to get table cursors" << std::endl;
			exit(EXIT_FAILURE);
		}
	}

	this->auxList.resize(this->tableManager->getTables().size(), true);
	this->auxNoMoreRows.resize(this->tableManager->getTables().size(), false);

	this->rowCounter = 0;
}

TableManagerCursor::~TableManagerCursor()
{
	for (unsigned int u = 0; u < this->cursorList.size(); u++) {
		delete(this->cursorList[u]);
	}

	this->cursorList.clear();
	this->auxList.clear();
	this->auxNoMoreRows.clear();
}

bool TableManagerCursor::getTableCursors()
{
	tableVector::const_iterator iter;
	Cursor *cursor;

	if (this->tableManager == NULL) {
		return false;
	}

	this->cursorList.clear();

	/* get table cursors */
	for (iter = this->tableManager->getTables().begin(); iter != this->tableManager->getTables().end(); iter++) {
		cursor = (*iter)->createCursor();
		if (cursor != NULL) {
			this->cursorList.push_back(cursor);
		}
	}

	this->cursorIndex = 0;

	if (this->cursorList.size() == 0) {
		/* no cursors */
		return false;
	}

	return true;
}


bool TableManagerCursor::next()
{
	bool ret_next;
	const Values *minValue = NULL;
	const Values *value = NULL;
	Cursor *minCursor = NULL; /* the name is accurate only when sording in ascending order */
	Cursor *cursor = NULL;
	uint64_t minIndex = 0, u;

	/* check whether we reached limit on number of printed rows */
	if (this->conf->getMaxRecords() && this->rowCounter >= this->conf->getMaxRecords()) {
		/* without option m, cursor list is not used => delete current cursor here  */
		if (!this->conf->getOptionm()) {
			delete this->currentCursor;
		}
		return false;
	}

	if (this->conf->getOptionm()) {
		/* user wants to sort rows according to timestamp */

		/* find record with smallest timestamp */
		for (u = 0; u < this->cursorList.size(); u++) {
			cursor = this->cursorList[u];

			if (this->auxList[u] == true) {
				if (this->auxNoMoreRows[u] == false) {
					/* there should be another rows */
					ret_next = cursor->next();

					if (ret_next == false) {
						this->auxNoMoreRows[u] = true;
					}

					this->auxList[u] = false;
				}
			}

			this->auxList[u] = false;

			/* read timestamp */
			if (this->auxNoMoreRows[u]) {
				/* no more rows in this table */
				cursor = NULL;
				continue;
			} else {
				value = conf->getOrderByColumn()->getValue(cursor);
			}

			if (minValue == NULL) {
				minValue = value;
				minCursor = cursor;
				minIndex = u;
			} else if ((conf->getOrderAsc() && *value < *minValue) || /* ascending order */
					(!conf->getOrderAsc() && *value > *minValue)) { /* descending order */
				minCursor = cursor;
				delete(minValue);
				minValue = value;
				minIndex = u;
			} else {
				delete(value);
			}
		}

		delete(minValue);

		/* check whether we have valid row */
		if (!minCursor) {
			/* looks like there are no data left */
			return false;
		}

		this->auxNoMoreRows[u] = true;

		/* don't forgot to call next() on this table next time */
		this->auxList[minIndex] = true;

		this->currentCursor = minCursor;

		this->rowCounter += 1;

		return true;
	}

	/* no order, just print all rows */

	/* first time we call this method */
	if (this->currentCursor == NULL) {
		this->currentCursor = (*this->currentTableIt)->createCursor();
	}

	/* proceed to the next row */
	while (!this->currentCursor->next()) {
		/* delete the cursor (not in list) */
		delete this->currentCursor;
		/* delete the table to save some memory if the tables are not needed for further processing (like statistics) */
		if (!conf->getExtendedStats()) {
			tableManager->removeTable(this->currentTableIt);
		}

		/* error during fetching new row, try next table */
		this->currentTableIt++;
		if (this->currentTableIt == this->tableManager->getTables().end()) {
			return false;
		}
		this->currentCursor = (*this->currentTableIt)->createCursor();
	}


	this->rowCounter += 1;

	/* we got valid row */
	return true;
}


bool TableManagerCursor::getColumn(const char *name, Values &value, int part)
{
	bool ret;

	if (this->currentCursor == NULL) {
		return false;
	}

	ret = this->currentCursor->getColumn(name, value, part);

	return ret;
}

const Cursor *TableManagerCursor::getCurrentCursor() const
{
	return this->currentCursor;
}

} /* namespace fbitdump */
