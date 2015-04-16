/**
 * \file TemplateInfo.cpp
 * \author Petr Velan <petr.velan@cesnet.cz>
 * \brief Class for printing information about templates
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

#include "TemplateInfo.h"
#include "Column.h"
#include <iomanip>

namespace fbitdump {

void TemplateInfo::printTemplates(TableManager &tm, Configuration &conf)
{
	/* get all parts */
	ibis::partList parts = tm.getParts();

	/* print each part */
	for (ibis::partList::const_iterator it = parts.begin(); it != parts.end(); it++) {
		TemplateInfo::printPartTemplate(*it, conf);
		std::cout << std::endl;
	}
}

void TemplateInfo::printPartTemplate(ibis::part *part, Configuration &conf)
{
	/* get part information */
	ibis::part::info *info = part->getInfo();

	/* print information about the part */
	std::cout << "Template: " << part->name() << " (" << part->currentDataDir() << ")" << std::endl;
	std::cout << "Description: " << info->description << std::endl;
	std::cout << "Rows: " << part->nRows() << std::endl;
	std::cout << "Columns: " << std::endl;

	/* print information about each column */

	/* header */
	std::cout << std::setiosflags(std::ios_base::left)
			<< "  " << std::setw(11) << "Column name"
			<< "  " << std::setw(8) << "Type"
			<< "  " << std::setw(20) << "Aliases"
			<< "  " << std::setw(20) << "Printed name"
			<< "  " << std::setw(7) << "Default"
			<< "  " << std::setw(10) << "Semantics"
			<< "  " << std::setw(5) << "Width"
			<< "  " << "Description"
			<< std::endl;
	/* columns */
	for (std::vector< ibis::column::info * >::const_iterator it = info->cols.begin(); it != info->cols.end(); it++) {
		/* get information from configuration XML*/
		pugi::xpath_node column;
		std::string elementName = (*it)->name;

		/* strip the part part of the name if necessary */
		size_t pos;
		if ((pos = elementName.find_first_of('p')) != std::string::npos) {
			elementName = elementName.substr(0, pos);
		}

		/* search xml for a plain name */
		column = conf.getXMLConfiguration().select_single_node(
				("/configuration/columns/column[*/element='"+elementName+"']").c_str());

		/* check what we found */
		std::string alias, name, nullStr, semantics, width;

		/* get columns with aliases */
		if (column != NULL && column.node().child("alias")) {
			/* construct Column class instance */
			alias = column.node().child_value("alias");
			Column col = Column(conf.getXMLConfiguration(), alias, false);

			/* pull information from the column */
			name = col.getName();
			alias.clear();
			stringSet aliases = col.getAliases();
			for (stringSet::const_iterator it = aliases.begin(); it != aliases.end();) {
				alias += *it;
				if (++it != aliases.end()) {
					alias += ", ";
				}
			}
			nullStr = col.getNullStr();
			semantics = col.getSemantics();
			if (col.getWidth() != 0) {
				std::stringstream ss;
				ss << col.getWidth();
				width = ss.str();
			}
		}

		std::cout << std::setiosflags(std::ios_base::left)
				<< "  " << std::setw(11) << (*it)->name
				<< "  " << std::setw(8) << ibis::TYPESTRING[(*it)->type]
				<< "  " << std::setw(20) << alias
				<< "  " << std::setw(20) << name
				<< "  " << std::setw(7) << nullStr
				<< "  " << std::setw(10) << semantics
				<< "  " << std::setw(5) << width
				<< "  " << (*it)->description
				<< std::endl;
	}

	delete info;
}

} /* end of fbitdump namespace */
