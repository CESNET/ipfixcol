/**
 * \file Table.h
 * \author Petr Velan <petr.velan@cesnet.cz>
 * \brief Header of class wrapping ibis::table
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

#ifndef TABLE_H_
#define TABLE_H_

#include "typedefs.h"
#include "Filter.h"
#include "Cursor.h"

namespace fbitdump {

/* Table and Cursor(Filter) classes are dependent on each other */
class Cursor;
class Filter;

typedef std::pair<std::string, std::string> stringPair;
typedef std::vector<stringPair> stringPairVector;

/**
 * \brief Class Table wrapping ibis::table
 */
class Table
{
public:
	/**
	 * \brief Cursor class constructor from one part
	 *
	 * @param part Part which should be wrapped
	 */
	Table(ibis::part *part);

	/**
	 * \brief Cursor class constructor from list of parts
	 *
	 * @param partList List of parts that should be wrapped
	 */
	Table(ibis::partList &partList);

	/**
	 * \brief Creates cursor for this table
	 * When the table is filtered out, cursor might be NULL
	 *
	 * @return Pointer to new Cursor class instance
	 */
	Cursor* createCursor();

	/**
	 * \brief Run query that aggregates columns and filters the table
	 *
	 * aggregateColumns must contain only columns that are in this table
	 * Function creates namesColumnsMap
	 *
	 * @param aggregateColumns Set of column to aggregate by
	 * @param summaryColumns Set of columns to summarize
	 * @param filter Filter to use
	 */
	void aggregate(const stringSet &aggregateColumns, const stringSet &summaryColumns,
			const Filter &filter);

	/**
	 * \brief Run query that filters data in this table
	 *
	 * @param columnNames Set of column names to use in query
	 * @param filter Filter to use
	 */
	void filter(stringSet columnNames, Filter &filter);


	/**
	 * \brief Get number of rows
	 * @return number of rows
	 */
	uint64_t nRows();

	/**
	 * \brief Returns ibis::table (mainly for cursor)
	 *
	 * @return ibis::table
	 */
	const ibis::table* getFastbitTable();

	/**
	 * \brief Returns map of column names to column numbers
	 * (Mainly for cursor)
	 *
	 * @return Map of column names to column numbers
	 */
	const namesColumnsMap& getNamesColumns();

	/**
	 * \brief Return pointer to used filter
	 * (For cursor)
	 *
	 * @return Table filter reference
	 */
	const Filter* getFilter();

	/**
	 * \brief Specify string set with columns names to order by
	 *
	 * More strings will be used when column has mutiple parts
	 * Strings must be put through namesColumnsMap,
	 * so this function is valid only after the map exists
	 *
	 * @param orderColumns list of strings to order by
	 * @param orderAsc true implies increasing order
	 */
	void orderBy(stringSet orderColumns, bool orderAsc);

	/**
	 * \brief Returns pointer to table with same fastbit table as this one
	 *
	 * This can be used to run multiple queries on same table.
	 * The queries on Table are destructive, so when there is need to preserve
	 * the original table, run the query on the copy (for e.g. summary of the table)
	 *
	 * Note that original fastbit table will be destroyed by deleting or querying the original table.
	 *
	 * @return Table that works on same fastbit table as this one. Can return NULL when table is filtered out
	 */
	Table* createTableCopy();

	/**
	 * \brief Table class destructor
	 */
	~Table();

private:

	/**
	 * \brief Constructor to create table from table
	 *
	 * @param table
	 */
	Table(Table *table);

	/**
	 * \brief Run query specified by queueQuery
	 */
	void doQuery();

	/**
	 * \brief Enquque query to be performed when table is used
	 *
	 * @param select Select part of the query
	 * @param filter Where part of the query
	 */
	void queueQuery(std::string select, const Filter &filter);

	/**
	 * \brief Translate column names to table columns
	 *
	 * With summary columns strips the function from name, does translation
	 * and puts the function back
	 *
	 * @param columns Set of column names to translate
	 * @param summary True when given columns are summary, default is false
	 * @return New set with translated columns
	 */
	stringPairVector translateColumns(const stringSet &columns, bool summary=false);

	ibis::table *table; /**< wrapped cursors table */
	const Filter *usedFilter; /**< Saved filter for cursor */
	namesColumnsMap namesColumns; /**< Map of column names to column numbers */
	bool queryDone; /**< Indicates that query was already preformed */
	std::string select; /**< Select string to be used on next query */
	stringSet orderColumns; /**< Set of columns to order by */
	bool orderAsc; /**< Same as in Configuration, true for increasing ordering */
	bool deleteTable;	/**< Is fastbit table managed by us? */
};

} /* end of namespace fbitdump */


#endif /* TABLE_H_ */
