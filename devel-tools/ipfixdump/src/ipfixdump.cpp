/**
 * \file ipfixdump.cpp
 * \author Petr Velan <petr.velan@cesnet.cz>
 * \brief Tool for ipfix fastbit format querying
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

#include <ibis.h>
#include <sstream>
#include <set>
#include <string>

#include "configuration.h"
#include "data.h"
#include "printer.h"

using namespace ipfixdump;

int main(int argc, char *argv[])
{
	int ret;
	/* create configuration to work with */
	configuration conf;
	data data;

	tableVector tables;

	/* process configuration and check whether end program */
	ret = conf.init(argc, argv);
	if (ret != 0) return ret;

	/* initialise printer */
	printer print(std::cout, conf);

	/* initialise tables */
	data.init(conf);

	/* do some work */
	if (conf.aggregate) {
		tables = data.aggregate(conf.aggregateColumns.c_str(), conf.filter.c_str());
	} else {
		tables = data.filter(conf.filter.c_str());
	}

	/* print tables */
	print.addTables(tables);
	print.print(conf.maxRecords);

	/* free used tables */
	for (tableVector::iterator it = tables.begin(); it != tables.end(); it++) {
		delete *it;
	}

	return 0;
}
