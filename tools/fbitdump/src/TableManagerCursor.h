/**
 * \file TableManagerCursor.h
 * \author Petr Velan <petr.velan@cesnet.cz>
 * \brief Global cursor over all Tables
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

#ifndef TABLE_MANAGER_CURSOR_H_
#define TABLE_MANAGER_CURSOR_H_

#include <fastbit/ibis.h>
#include <cstring>
#include "Values.h"
#include "Table.h"
#include "TableManager.h"

namespace fbitdump {

class TableManager;  /* forward declaration */
class Cursor;

/**
 * \brief Global cursor for all Tables
 *
 * It allows us to iterate over all rows in all tables with single cursor.
 * It can limit number of rows, sort output according to given column
 */
class TableManagerCursor {
private:
	TableManager *tableManager;         /**< table manager */
	Configuration *conf;                /**< program configuration */
	std::vector<Cursor *> cursorList;   /**< list of table specific cursors */
	Cursor *currentCursor;              /**< current cursor with actual data */
	tableVector::iterator currentTableIt;/**< index of current table */

	unsigned int cursorIndex;           /**< index of the current table cursor */
	std::vector<bool> auxList;          /**< auxiliary list, true means that on
	                                     *   corresponding cursor should be called next() */
	std::vector<bool> auxNoMoreRows;    /**< auxiliary list, indicates whether we
	                                     *   reached end of the table */
	uint64_t rowCounter;                /**< number of printed rows */


	/** private methods **/

	/**
	 * \brief Get table cursors for all tables
	 *
	 * @return true if there are any cursors, false otherwise
	 */
	bool getTableCursors();



public:
	/**
	 * \brief Constructor
	 *
	 * @param[in] tableManager Table Manager to make global cursor for
	 * @param[in] conf Configuration object
	 */
	TableManagerCursor(TableManager &tableManager, Configuration &conf);

	/**
	 * \brief Destructor
	 */
	~TableManagerCursor();

	/**
	 * \brief Point cursor to the next row
	 *
	 * @return true, if there is another row, false otherwise
	 */
	bool next();

	/**
	 * \brief Get column value and return it as a "values" structure
	 *
	 * @param[in] name Name of the fastbit column to get value for
	 * @param[out] value Values structure with column values
	 * @param[in] part Number of part to write result to
	 * @return true on success, false otherwise
	 */
	bool getColumn(const char *name, Values &value, int part);

	/**
	 * \brief Get cursor to the current row
	 *
	 * @return current cursor
	 */
	const Cursor *getCurrentCursor() const;
};

}


#endif
