/**
 * \file filter.cpp
 * \author Petr Velan <petr.velan@cesnet.cz>
 * \brief Class for management of result filtering
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
#include <iostream>
#include "filter.h"

#include "scanner.h"
/* yylex is not in the header, declare it separately */
extern YY_DECL;

namespace ipfixdump
{

std::string filter::run() {
	std::string input = conf.filter, filter;

	YY_BUFFER_STATE bp;
	int c;
	std::string arg;

	bp = yy_scan_string(input.c_str());
	yy_switch_to_buffer(bp);

	while ((c = yylex(arg)) != 0) {
		switch (c) {
		case COLUMN: {
			/* get real columns */
			pugi::xml_document doc;
			doc.load_file(COLUMNS_XML);
			pugi::xpath_node column = doc.select_single_node(("/columns/column[alias='"+arg+"']").c_str());
			if (column == NULL) {
				std::cout << "Cannot find alias: '"<< arg << "'" << std::endl;
				return "";
			} else if ( column.node().child("value").attribute("type").value() == std::string("plain")) {
				/* replace alias with column */
				filter += column.node().child("value").child_value("element");
				filter += " ";
			//} else if ( column.node().child("value").attribute("type").value() == std::string("group")) {
				/* TODO: this should be somehow done individually for each table
				 * When table has one of the columns, use it. In other case, drop it. */
			} else {
				std::cout << "Column : '"<<  column.node().child_value("name") << "' is not of supported type" << std::endl;
				return "";
			}
		}
			break;
/*		case NUMBER:
			std::cout << "number: ";
			break;
		case OPERATOR:
			std::cout << "operator: ";
			break;
		case RAWCOLUMN:
			std::cout << "raw column: ";
			break;
		case CMP:
			std::cout << "comparison: ";
			break;*/
		case OTHER:
			std::cout << "Wrong filter string: '"<< arg << "'" << std::endl;
			/* end */
			return "";
			break;
		default:
			filter += arg + " ";
		break;
		}
	}

	yy_flush_buffer(bp);
	yy_delete_buffer(bp);

#ifdef DEBUG
	std::cerr << "Using filter: '" << filter << "'" << std::endl;
#endif
	return filter;
}

filter::filter(configuration &conf): conf(conf) {}

}
