/**
 * \file TableSummary.h
 * \author Petr Velan <petr.velan@cesnet.cz>
 * \brief Header of class handling summary for TableManager
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

#ifndef TABLESUMMARY_H_
#define TABLESUMMARY_H_

#include "typedefs.h"
#include "Values.h"

namespace fbitdump {

typedef std::map<std::string, double> valuesMap;
typedef std::map<std::string, int> occurenceMap;

/**
 * \brief Computes table summary on given columns
 *
 * Creates copies of given tables, runs a query that sums  up given columns and combines the results
 * When given table does not have required summary column, stores zero
 */
class TableSummary
{
public:

	/**
	 * \brief Constructor takes Tables in vector and reads summary data from them
	 *
	 * @param tables Tables to create summary data for
	 * @param summaryColumns vector of columns to create summaries for
	 */
        TableSummary(tableVector const &tables, columnVector const &summaryColumns);

	/**
	 * \brief Returns summary value for specified column
	 * @param column Column to return the summary value for
	 * @return double summary value
	 */
	double getValue(std::string &column) const;

private:
	valuesMap values; /**< Map of column names and Values */
        occurenceMap occurences; /**< Map of column names and their occurences in tables */
};

} /* end of fbitdump namespace */


#endif /* TABLESUMMARY_H_ */
