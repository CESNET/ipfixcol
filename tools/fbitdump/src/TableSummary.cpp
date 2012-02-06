/**
 * \file TableSummary.cpp
 * \author Petr Velan <petr.velan@cesnet.cz>
 * \brief Class handling summary for TableManager
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

#include "TableSummary.h"
#include "Filter.h"

namespace fbitdump {

TableSummary::TableSummary(tableVector const &tables, stringSet const &summaryColumns)
{
	Table *sumTable;
	Filter filter;

	for (tableVector::const_iterator it = tables.begin(); it != tables.end(); it++) {
		sumTable = (*it)->createTableCopy();

		if (sumTable == NULL) continue;

		sumTable->aggregate(stringSet(), summaryColumns, filter);

		Cursor *cur = sumTable->createCursor();
		if (cur->next()) { /* summary table has some lines (is not filtered out) */
			for (stringSet::const_iterator iter = summaryColumns.begin(); iter != summaryColumns.end(); iter++) {

				Values val;
				if (cur->getColumn(*iter, val, 0)) { /* add value if available  */
					this->values[*iter] += val.toDouble(0);
				}
			}
		}

		delete sumTable;
	}
}

double TableSummary::getValue(std::string &column) const
{
	valuesMap::const_iterator it;
	if ((it = this->values.find(column)) != this->values.end()) {
		return it->second;
	}

	return 0;
}

}
