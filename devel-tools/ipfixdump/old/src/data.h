/**
 * \file data.h
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

#ifndef DATA_H_
#define DATA_H_

#include "typedefs.h"
#include "configuration.h"

/**
 * \brief Namespace of the ipfixdump utility
 */
namespace ipfixdump {

/**
 * \brief Class managing tables
 *
 * Holds all tables and table parts
 */
class data {
private:

	/**
	 * \brief Trim Both leading and trailing spaces
	 *
	 * @param str String to be trimmed
	 * @return Trimmed string
	 */
	std::string trim(std::string str);

	stringVector defaultOrder;

public:
	ibis::partList parts; /**< table parts to be used*/

	/** vector containing vector of column names for each table part */
	std::vector<stringSet*> columns;

	/**
	 * \brief Configuration class destructor
	 */
	~data();

	/**
	 * \brief Initialise data tables from configuration
	 * @param conf
	 */
	void init (configuration &conf);

	/**
	 * \brief Runs select command on all loaded tables having specified columns
	 *
	 * @param sel  groups of selected columns
	 * @param cond condition passed to ibis::table::select
	 * @return pointer to new ibis::table (must be freed separately!)
	 */
	tableVector select(std::map<int, stringSet> sel, const char* cond);

	/**
	 * \brief Runs select command on all loaded tables having specified columns
	 *
	 * @param sel groups of selected columns
	 * @param cond condition passed to ibis::table::select
	 * @param order sets orderby clausule
	 * @return pointer to new ibis::table (must be freed separately!)
	 */
	tableVector select(std::map<int, stringSet> sel, const char *cond, stringVector &order);

	/**
	 * \brief Aggregate data specified by cond according to select statement
	 *
	 * This function basically runs select query on all tables. It should
	 * manage grouped columns (ipv4 and ipv6) and process them separately.
	 *
	 * The format of columns should be the same as for regular output, but
	 * columns must be summarized.
	 *
	 * @param sel groups of selected columns
	 * @param cond condition
	 * @return vector of aggregated tables
	 */
	tableVector aggregate(std::map<int, stringSet> sel, const char *cond);

	/**
	 * \brief Filter all table part by specified condition
	 *
	 * returns only tables that have specified columns
	 *
	 * @param cond Filter condition
	 * @return vector of filtered tables
	 */
	tableVector filter(const char* cond);

};

}  // namespace ipfixdump


#endif /* DATA_H_ */
