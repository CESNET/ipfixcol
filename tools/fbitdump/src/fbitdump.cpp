/**
 * \file fbitdump.cpp
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

/**
 * \mainpage IPFIX Dump Developer's Documentation
 *
 * This documents provides documentation of IPFIX Dump utility (ipfixdump).
 */
#include <fstream>
#include <iostream>

#include "Configuration.h"
#include "TableManager.h"
#include "Printer.h"
#include "Filter.h"
#include "IndexManager.h"
#include "TemplateInfo.h"

using namespace fbitdump;

int main(int argc, char *argv[])
{
	/* raise limit for cache size, when there is more memory available */
	/* default is half of available physical memory */
	//ibis::fileManager::adjustCacheSize(2048000000);

//	ibis::fileManager::instance().printStatus(std::cout);

	//ibis::gVerbose = 7;


	ibis::gParameters().add("fileManager.minMapSize", "50");

	/* create configuration to work with */
	Configuration conf;
//	Configuration::instance = &conf;
	std::ofstream pipe;

	/* process configuration and check whether to end the program */
	try {
		if (conf.init(argc, argv)) {
			return 0; /* standard program end (help requested, ...) */
		}
	} catch (std::exception &e) {
		/* inicialization error: print it and exit */
		std::cerr << e.what() << std::endl;
		return 1;
	}



	try {
		/* create filter */
		if(isatty(fileno(stdout))) {
			std::cout << "Creating filter...                                     \r";
			std::cout.flush();
		}
		Filter filter(conf);
		if(isatty(fileno(stdout))) {
			std::cout << "Initializing printer...                                     \r";
			std::cout.flush();
		}
		/* initialise printer */
		Printer print(std::cout, conf);
		if(isatty(fileno(stdout))) {
			std::cout << "Initializing tables...                                     \r";
			std::cout.flush();
		}
		/* initialise tables */
		TableManager tm(conf);
		/* check whether to delete indexes */
		if (conf.getDeleteIndexes()) {
			if(isatty(fileno(stdout))) {
				std::cout << "Deleting indexes...                                     \r";
				std::cout.flush();
			}
			IndexManager::deleteIndexes(conf, tm);

			if (access (conf.pipe_name.c_str(), F_OK ) == 0 ) {
				ibis::partList parts = tm.getParts();
				pipe.open( conf.pipe_name.c_str() );

				for (ibis::partList::iterator partIt = parts.begin(); partIt != parts.end(); partIt++) {
					pipe << (*partIt)->currentDataDir() << "\n";
				}
				pipe.close();
			}
		}

		/* check whether to build indexes */
		if (conf.getCreateIndexes()) {
			if(isatty(fileno(stdout))) {
				std::cout << "Building indexes...                                     \r";
				std::cout.flush();
			}
			IndexManager::createIndexes(conf, tm);
			if (access (conf.pipe_name.c_str(), F_OK ) == 0 ) {
				ibis::partList parts = tm.getParts();
				pipe.open( conf.pipe_name.c_str() );

				for (ibis::partList::iterator partIt = parts.begin(); partIt != parts.end(); partIt++) {
					pipe << (*partIt)->currentDataDir() << "\n";
				}
				pipe.close();
			}
		}

		/* check whether to print template information */
		if (conf.getTemplateInfo()) {
			if(isatty(fileno(stdout))) {
				std::cout << "Printing templates...                                     \r";
				std::cout.flush();
			}
			TemplateInfo::printTemplates(tm, conf);
		}

		/* index manipulation and template info are separate tasks, exclusive with flow printing */
		if (!conf.getDeleteIndexes() && !conf.getCreateIndexes() && !conf.getTemplateInfo()) {
			/* do the work */
			if (conf.getAggregate()) {
				if(isatty(fileno(stdout))) {
					std::cout << "Aggregating tables...                                                                    \r";
					std::cout.flush();
				}
				tm.aggregate(conf.getAggregateColumns(), conf.getSummaryColumns(), filter);
			} else {
				tm.filter(filter);
			}
			

			/* Remove progressbar */
			std::cout.fill(' ');
			std::cout.width(50);
			std::cout << "\r";
			std::cout.flush();
			
			/* print tables */
			print.print(tm);
		}

	} catch (std::exception &e) {
		std::cerr << e.what() << std::endl;
		return 2;
	}

	return 0;
}
