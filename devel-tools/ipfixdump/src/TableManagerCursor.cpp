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

	/* get table cursors */
	this->getTableCursors();

	pugi::xml_document doc;
	if (!doc.load_file(COLUMNS_XML)) {
		std::cerr << "XML '"<< COLUMNS_XML << "' with columns configuration cannot be loaded!" << std::endl;
	}

	this->timestampColumn = new Column();

	this->timestampColumn->init(doc, "%ts", false);

	this->currentCursor = *(this->cursorListIter); // FIXME
}

bool TableManagerCursor::getTableCursors()
{
	unsigned long ncursors = 0;
	tableVector::iterator iter;

	if (this->tableManager == NULL) {
		return false;
	}

	this->cursorList.clear();

	/* get table cursors */
	for (iter = this->tableManager->getTables().begin(); iter != this->tableManager->getTables().end(); iter++) {
		this->cursorList.push_back((*iter)->createCursor());
	}

	this->cursorListIter = this->cursorList.begin();

	if (ncursors > 0) {
		return true;
	}

	/* no cursors */
	return false;
}


TableManagerCursor::~TableManagerCursor()
{
	/* nothing to do */
	/* TODO - do we really need this constructor? */
}

bool TableManagerCursor::next()
{
	bool next_ret;
	struct values *minValue;
	struct values *value;
	Cursor *minCursor;

	if (this->conf->getOptionm()) {
		/* user wants to sort rows according to timestamp */
		/* TODO */
		if (this->currentCursor != NULL) {
			this->currentCursor->next();
		}

		std::vector<Cursor *>::iterator cursor;
		for (cursor = this->cursorList.begin(); cursor < this->cursorList.end(); cursor++) {
			value = this->timestampColumn->getValue(this->getCurrentCursor());
			if (value->toLong() < minValue->toLong()) {
				minCursor = *cursor;
				free(minValue);
				minValue = value;
			}
		}

		this->currentCursor = minCursor;

		return true;
	}



	/* just print out all rows */
	if (this->currentCursor == NULL) {
		/* this is first time we call this method */
		this->currentCursor = *(this->cursorListIter);
		this->currentCursor->next();
	} else {
		/* proceed to the next row */

		if (this->cursorListIter == this->cursorList.end()) {
			/* we reached end of vector, start from beginning again */
			this->cursorListIter = this->cursorList.begin();
		}

		this->currentCursor = *(this->cursorListIter);
		next_ret = this->currentCursor->next();
		if (!next_ret) {
			/* we have processed all rows */
			this->currentCursor = NULL;
			return false;
		}
	}

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
