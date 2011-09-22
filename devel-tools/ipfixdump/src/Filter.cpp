/**
 * \file Filter.cpp
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
#include <arpa/inet.h>

#include "Filter.h"
#include "Configuration.h"
#include "typedefs.h"
#include "scanner.h"

/* yylex is not in the header, declare it separately */
extern YY_DECL;

namespace ipfixdump
{

std::string Filter::getFilter() {
	return this->filterString;
}

bool Filter::isValid(Cursor &cur) {
	// TODO add this functiononality
	return true;
}

Filter::Filter(Configuration &conf): conf(conf) {
	std::string input = this->conf.getFilter(), filter;

	YY_BUFFER_STATE bp;
	int c;
	std::string arg;

	bp = yy_scan_string(input.c_str());
	yy_switch_to_buffer(bp);

	while ((c = yylex(arg)) != 0) {
		switch (c) {
		case COLUMN: {
			/* get real columns */
//			pugi::xml_document doc;
//			doc.load_file(COLUMNS_XML);
//			pugi::xpath_node column = doc.select_single_node(("/columns/column[alias='"+arg+"']").c_str());
//			if (column == NULL) {
//				std::cout << "Cannot find alias: '"<< arg << "'" << std::endl;
//				this->filterString = "";
//			} else if ( column.node().child("value").attribute("type").value() == std::string("plain")) {
//				/* replace alias with column */
//				filter += column.node().child("value").child_value("element");
//				filter += " ";
//			} else if ( column.node().child("value").attribute("type").value() == std::string("operation")) {
//				// TODO this should prepare for isValid() function
//			} else {
//				std::cout << "Column : '"<<  column.node().child_value("name") << "' is not of supported type" << std::endl;
//				this->filterString = "";
//			}

			bool found = false;
			/* go over all columns */
			for (columnVector::iterator columnIt = conf.getColumns().begin(); columnIt !=conf.getColumns().end(); columnIt++) {
				/* get column aliases */
				stringSet aliases = (*columnIt)->getAliases();
				/* if current alias matches one of columns aliases */
				if (aliases.find(arg) != aliases.end()) {
					/* add column fastbit columns to final set */
					stringSet cols = (*columnIt)->getColumns();
					if (cols.size() == 1) { /* plain value */
						if (conf.getAggregate() && (*columnIt)->getAggregate()) { /* with aggregation */
							int begin = cols.begin()->find_first_of('(') + 1;
							int end = cols.begin()->find_first_of(')');

							filter += cols.begin()->substr(begin, end-begin) + " ";
						} else { /* without aggregation */
							filter += *cols.begin() + " ";
						}
					} else { /* operation */
						/* TODO */
					}

					found = true;
					break;
				}
			}
			if (!found) {
				std::cerr << "Filter column '" << arg << "' not found!" << std::endl;
			}

		}
			break;
		case IPv4:{
			/* convert ipv4 address to uint32_t */
			struct in_addr addr;
			std::stringstream ss;
			/* convert ipv4 address, returns network byte order */
			inet_pton(AF_INET, arg.c_str(), &addr);
			/* convert to host byte order */
			uint32_t addrNum = ntohl(addr.s_addr);
			/* integer to string */
			ss << addrNum;
			filter += ss.str() + " ";
			break;}
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
			this->filterString = "";
			break;
		default:
			filter += arg + " ";
		break;
		}
	}

	/* free flex allocated resources */
	yy_flush_buffer(bp);
	yy_delete_buffer(bp);
	yylex_destroy();

#ifdef DEBUG
	std::cerr << "Using filter: '" << filter << "'" << std::endl;
#endif
	this->filterString = filter;
}

}
