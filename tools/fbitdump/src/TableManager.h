/**
 * \file TableManager.h
 * \author Petr Velan <petr.velan@cesnet.cz>
 * \brief Header of class for managing table parts and tables
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

#ifndef TABLE_MANAGER_H_
#define TABLE_MANAGER_H_

#include "typedefs.h"
#include "Configuration.h"
#include "Filter.h"
#include "Table.h"
#include "TableManagerCursor.h"

/**
 * \brief Namespace of the fbitdump utility
 */
namespace fbitdump {

class TableManagerCursor;
class Filter;
class Configuration;

/**
 * \brief Class managing tables
 *
 * Holds all table parts, encapsulates filter() and aggregate() calls.
 * After filter()/aggregate() call holds resulting tables and returns them on
 * getTables() call.
 */
class TableManager
{
public:
	/**
	 * \brief Data class constructor
	 *
	 * Initialise data tables from configuration
	 *
	 * @param conf
	 */
	TableManager(Configuration &conf);

	/**
	 * \brief Aggregate data specified by cond according to select statement
	 *
	 * This should combine tables that have same columns (in aggregation set)
	 * to one table
	 *
	 * @param aggregateColumns Columns to aggregate by
	 * @param summaryColumns Columns to summarize
	 * @param filter Filter
	 */
	void aggregate(stringSet aggregateColumns, stringSet summaryColumns,
				Filter &filter);

	/**
	 * \brief Filter all table part by specified condition
	 *
	 * This only delegates to each table filter() call
	 *
	 * @param filter Filter
	 */
	void filter(Filter &filter);

	/**
	 * \brief Return vector of managed tables
	 *
	 * @return vector of tables
	 */
	tableVector& getTables();


	void removeTable(tableVector::iterator &it);

	/**
	 * \brief Creates cursor for this Table Manager
	 *
	 * @return Pointer to new TableManagerCursor class instance
	 */
	TableManagerCursor *createCursor();

	/**
	 * \brief Return number of managed parts
	 *
	 * @return number of managed parts
	 */
	uint64_t getNumParts() const;

	/**
	 * \brief Return number of rows present in the initial raw data
	 * @return number of rows in initial data
	 */
	uint64_t getInitRows() const;

	/**
	 * \brief Class destructor
	 */
	~TableManager();

private:

	Configuration &conf;		/**< Program configuration */
	ibis::partList parts;		/**< List of loaded table parts */
	tableVector tables;			/**< List of managed tables */
	stringSet orderColumns; 	/**< String list of order by columns */
	bool orderAsc;				/**< Same as in Configuration, true when columns are to be sorted in increasing order */
};

}  // namespace fbitdump


#endif /* TABLE_MANAGER_H_ */
