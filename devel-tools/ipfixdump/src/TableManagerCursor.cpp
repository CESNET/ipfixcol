/**
 * \file TableManagerCursor.cpp
 * \author Michal Srb <michal.srb@cesnet.cz>
 * \brief TODO
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

namespace ipfixdump {


TableManagerCursor::TableManagerCursor(TableManager &tableManager, Configuration &conf)
{
	this->tableManager = &tableManager;
	this->conf = &conf;

	this->currentCursor = NULL;
	this->cursorIndex = 0;

	/* get table cursors */
	if (this->getTableCursors() == false) {
		std::cerr << "Unable to get table cursors" << std::endl;
		exit(EXIT_FAILURE);
	}

	pugi::xml_document doc;
	if (!doc.load_file(COLUMNS_XML)) {
		std::cerr << "XML '"<< COLUMNS_XML << "' with columns configuration cannot be loaded!" << std::endl;
	}

	this->timestampColumn = new Column();

	this->timestampColumn->init(doc, "%ts", false);

	this->auxList.resize(this->tableManager->getTables().size(), true);
	this->auxNoMoreRows.resize(this->tableManager->getTables().size(), false);

	this->rowCounter = 0;
}

TableManagerCursor::~TableManagerCursor()
{
	for (unsigned int u = 0; u < this->cursorList.size(); u++) {
		delete(this->cursorList[u]);
	}

	delete(this->timestampColumn);

	this->cursorList.clear();
	this->auxList.clear();
	this->auxNoMoreRows.clear();
}

bool TableManagerCursor::getTableCursors()
{
	tableVector::iterator iter;
	Cursor *cursor;

	if (this->tableManager == NULL) {
		return false;
	}

	this->cursorList.clear();

	/* get table cursors */
	for (iter = this->tableManager->getTables().begin(); iter != this->tableManager->getTables().end(); iter++) {
		cursor = (*iter)->createCursor();
		this->cursorList.push_back(cursor);
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
	struct values *minValue = NULL;
	struct values *value;
	Cursor *minCursor = NULL;
	Cursor *cursor;
	unsigned int minIndex = 0;
	unsigned int u;

	/* check whether we reached limit on number of printed rows */
	if (this->conf->getMaxRecords() && this->rowCounter >= this->conf->getMaxRecords()) {
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

			this->auxList[u] == false;

			/* read timestamp */
			if (this->auxNoMoreRows[u]) {
				/* no more rows in this table */
				cursor = NULL;
				continue;
			} else {
				value = this->timestampColumn->getValue(cursor);
			}

			if (minValue == NULL) {
				minValue = value;
				minCursor = cursor;
				minIndex = u;
			} else if (value->toLong() < minValue->toLong()) {
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
		if (!cursor) {
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

	/* no filter, just print all rows */
	if (true) {

		if (this->currentCursor == NULL) {
			/* this is first time we call this method */
			this->currentCursor = this->cursorList[0];
		}

		/* proceed to the next row */
fetch_row:
		ret_next = this->currentCursor->next();
		if (ret_next == false) {
			/* error during fetching new row, try next table */
			this->cursorIndex++;
			if (this->cursorIndex < this->cursorList.size()) {
				this->currentCursor = this->cursorList[this->cursorIndex];

				/* fetch row from next table */
				goto fetch_row;
			} else {
				/* no more rows */
				return false;
			}
		}
	}

	this->rowCounter += 1;

	/* we got valid row */
	return true;
}


bool TableManagerCursor::getColumn(const char *name, values &value, int part)
{
	bool ret;

	if (this->currentCursor == NULL) {
		return false;
	}

	ret = this->currentCursor->getColumn(name, value, part);

	return ret;
}

Cursor *TableManagerCursor::getCurrentCursor()
{
	return this->currentCursor;
}

} /* namespace ipfixdump */
