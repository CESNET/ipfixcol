/**
 * \file Table.h
 * \author Petr Velan <petr.velan@cesnet.cz>
 * \brief Header of class wrapping ibis::table
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
 *
 * aggregate and filter functions do the main work, converting the names of columns to fastbit names
 * and building the query
 *
 * The select operation on fastbit table is delayed with mechanism of queueQuery and doQuery
 * Table can provide cursor to itself
 * Table can provide copy of itself
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
	 *
	 * @param aggregateColumns vector of columns to aggregate by
	 * @param summaryColumns vector of columns to summarize
	 * @param filter Filter to use
         * @param summary flag indicating summary creation
         * @param select_flows true when flows are required
	 */
        void aggregate(const columnVector &aggregateColumns, const columnVector &summaryColumns, const Filter &filter, bool summary = false, bool select_flows = false);
        
        /**
	 * \brief Run query that aggregates columns and filters the table using aggregation functions
	 *
	 * aggregateColumns must contain only columns that are in this table
	 *
	 * @param aggregateColumns vector of columns to aggregate by
	 * @param summaryColumns vector of columns to summarize
	 * @param filter Filter to use
	 */
        void aggregateWithFunctions(const columnVector &aggregateColumns, const columnVector &summaryColumns, const Filter &filter);
        
        /**
	 * \brief Run query that filters data in this table
	 *
	 * @param columns vector of columns names to use in query
	 * @param filter Filter to use
	 */
        void filter(columnVector columns, Filter &filter);
        
        /**
         * \brief Only apply given filter on whole table
         * 
         * @param filter Filter
         */
        void filter(Filter &filter);

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
	 * \brief Return pointer to used filter
	 * (For cursor)
	 *
	 * @return Table filter reference
	 */
	const Filter* getFilter();

	/**
	 * \brief Specify string set with columns names to order by
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
         * \brief Create select clause from column vector
         * 
         * @param columns column vector
         * @param summary true when creating summary
         * @return select clause
         */
        std::string createSelect(const columnVector columns, bool summary = false, bool has_flows = false);
        
        /**
         * \brief Get list of column names from vector
         * 
         * @param columns column vector
         * @return columns names
         */
        stringSet getColumnsNames(const columnVector &columns);
        
        /**
         * \brief Get columns with same names as in given set
         * 
         * @param columns column vector
         * @param names names to be matched
         * @return matching columns
         */
        columnVector getColumnsByNames(const columnVector columns, const stringSet names);
 
	ibis::table *table; /**< wrapped cursors table */
	const Filter *usedFilter; /**< Saved filter for cursor */
	bool queryDone; /**< Indicates that query was already preformed */
	std::string select; /**< Select string to be used on next query */
	stringSet orderColumns; /**< Set of columns to order by */
	bool orderAsc; /**< Same as in Configuration, true for increasing ordering */
	bool deleteTable;	/**< Is fastbit table managed by us? */
};

} /* end of namespace fbitdump */


#endif /* TABLE_H_ */
