/**
 * \file configuration.cpp
 * \author Petr Velan <petr.velan@cesnet.cz>
 * \brief Class for managing user input of ipfixdump
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

#include "configuration.h"

/* temporary macro that should be removed in full implementation */
#define NOT_SUPPORTED std::cerr << "Not supported" << std::endl; return -2;

namespace ipfixdump
{


int configuration::searchForTableParts()
{
	DIR *d;
	struct dirent *dent;

	/* do we have any tables (directories) specified? */
	if (this->tables.size() < 1) {
		std::cerr << "Input file(s) must be specified" << std::endl;
		return -1;
	}

	/* read tables subdirectories(templates) */
	for (size_t i = 0; i < tables.size(); i++) {
		d = opendir(tables[i].c_str());
		if (d == NULL) {
			std::cerr << "Cannot open directory \"" << tables[i] << "\"" << std::endl;
			return -1;
		}

		parts.push_back(new stringVector);
		while((dent = readdir(d)) != NULL) {
			if (dent->d_type == DT_DIR && atoi(dent->d_name) != 0) {
				parts[i]->push_back(std::string(dent->d_name));
			}
		}

		closedir(d);
	}

	return 0;
}

int configuration::init(int argc, char *argv[]) {
	char c;

	/* get program name without execute path */
	progname = ((progname = strrchr (argv[0], '/')) != NULL) ? (progname + 1) : argv[0];
	argv[0] = progname;

	if (argc == 1) {
		help();
		return -1;
	}

	/* parse command line parameters */
	while ((c = getopt (argc, argv, OPTSTRING)) != -1) {
		switch (c) {
		case 'h': /* print help */
			help();
			return 1;
			break;
		case 'V': /* print version */
			std::cout << progname << ": Version: " << version() << std::endl;
			return 1;
			break;
		case 'a': /* aggregate */
			aggregate = true;
			/* set default aggregation columns */
			if (aggregateColumns.size() == 0) {
				aggregateColumns.insert("%sa");
				aggregateColumns.insert("%da");
				aggregateColumns.insert("%sp");
				aggregateColumns.insert("%dp");
				aggregateColumns.insert("%pr");
			}
			break;
		case 'A': /* aggregate on specific columns */
			char *token;
			aggregate = true;
			/* add aggregate columns to set */
			aggregateColumns.clear();
			token = strtok(optarg, ",");
			if (token == NULL) {
				help();
				return -2;
			} else {
				aggregateColumns.insert(token);
				while ((token = strtok(NULL, ",")) != NULL) {
					aggregateColumns.insert(token);
				}
			}
			break;
		case 'r': /* file to open */
                if (optarg == std::string("")) {
                	help();
                	return -2;
                }
#ifdef DEBUG
                std::cerr << "DEBUG: adding table " << optarg << std::endl;
#endif
				tables.push_back(std::string(optarg));
			break;
		case 'f':
			NOT_SUPPORTED
			break;
		case 'n':
			NOT_SUPPORTED
			break;
		case 'c': /* number of records to display */
				maxRecords = atoi(optarg);
			break;
		case 'D':
			NOT_SUPPORTED
			break;
		case 'N': /* print plain numbers */
				plainNumbers = true;
			break;
		case 's':
			NOT_SUPPORTED
			break;
		case 'q':
				quiet = true;
			break;
		case 'I':
			NOT_SUPPORTED
			break;
		case 'M':
			NOT_SUPPORTED
			break;
		case 'm':
			NOT_SUPPORTED
			break;
		case 'R': {
			std::string dirpath;
			dirpath = optarg;

			DIR *dir;
			struct dirent *dent;

			dir = opendir(dirpath.c_str());
			if (dir == NULL) {
				std::cerr << "Cannot open directory \"" << dirpath << "\"" << std::endl;
				break;
			}

			while((dent = readdir(dir)) != NULL) {
				if (dent->d_type == DT_DIR && strcmp(dent->d_name, ".")
				&& strcmp(dent->d_name, "..")) {
					std::string tableDir(dirpath);
					tableDir += dent->d_name;
#ifdef DEBUG
					std::cerr << "DEBUG: adding table " << tableDir << std::endl;
#endif
					this->tables.push_back(std::string(tableDir));
				}
			}

			closedir(dir);
			break;
		}
		case 'o': /* output format */
			format = optarg;
			break;
		case 'v':
			NOT_SUPPORTED
			break;
		case 'Z':
			NOT_SUPPORTED
			break;
		case 't':
			NOT_SUPPORTED
			break;
		default:
			help ();
			return -1;
			break;
		}
	}

	/* read filter */
	if (optind < argc) {
		filter = argv[optind];
	} else {
		/* set default filter */
		filter = "1=1";
	}

	/* set default order (by timestamp) */
	order.push_back("e0id152");

	/* TODO add format to print everything */
	if (format.empty() || format == "line") {
		format = "%ts %td %pr %sa:%sp -> %da:%dp %pkt %byt %fl";
	} else if (format == "long") {
		format = "%ts %td %pr %sa:%sp -> %da:%dp %flg %tos %pkt %byt %fl";
	} else if (format == "extended") {
		format = "%ts %td %pr %sa:%sp -> %da:%dp %flg %tos %pkt %byt %bps %pps %bpp %fl";
	} else if (format == "pipe") {
		format = "%ts|%td|%pr|%sa|%sp|%da|%dp|%pkt|%byt|%fl";
	} else if (format == "csv") {
		format = "%ts,%td,%pr,%sa,%sp,%da,%dp,%pkt,%byt,%fl";
	} else if (format.substr(0,4) == "fmt:") {
		format = format.substr(4);
	} else {
		std::cerr << "Unknown ouput mode: '" << format << "'" << std::endl;
		return -1;
	}

	/* parse output format string */
	parseFormat(format);

	/* search for table parts in specified directories */
	this->searchForTableParts();

	return 0;
}

void configuration::help() {
	std::cout
	<< "usage "<< progname <<" [options] [\"filter\"]" << std::endl
	<< "-h              this text you see right here" << std::endl
	<< "-V              Print version and exit." << std::endl
	<< "-a              Aggregate netflow data." << std::endl
	<< "-A <expr>[/net] How to aggregate: ',' sep list of tags see ipfixdump(1)" << std::endl
	<< "                or subnet aggregation: srcip4/24, srcip6/64." << std::endl
	//<< "-b              Aggregate netflow records as bidirectional flows." << std::endl
	//<< "-B              Aggregate netflow records as bidirectional flows - Guess direction." << std::endl
	<< "-r <dir>        read input tables from directory" << std::endl
	//<< "-w <file>       write output to file" << std::endl
	<< "-f              read netflow filter from file" << std::endl
	<< "-n              Define number of top N. " << std::endl
	<< "-c              Limit number of records to display" << std::endl
	<< "-D <dns>        Use nameserver <dns> for host lookup." << std::endl
	<< "-N              Print plain numbers" << std::endl
	<< "-s <expr>[/<order>]     Generate statistics for <expr> any valid record element." << std::endl
	<< "                and ordered by <order>: packets, bytes, flows, bps pps and bpp." << std::endl
	<< "-q              Quiet: Do not print the header and bottom stat lines." << std::endl
	//<< "-H Add xstat histogram data to flow file.(default 'no')" << std::endl
	//<< "-i <ident>      Change Ident to <ident> in file given by -r." << std::endl
	//<< "-j <file>       Compress/Uncompress file." << std::endl
	//<< "-z              Compress flows in output file. Used in combination with -w." << std::endl
	//<< "-l <expr>       Set limit on packets for line and packed output format." << std::endl
	//<< "                key: 32 character string or 64 digit hex string starting with 0x." << std::endl
	//<< "-L <expr>       Set limit on bytes for line and packed output format." << std::endl
	<< "-I              Print netflow summary statistics info from file, specified by -r." << std::endl
	<< "-M <expr>       Read input from multiple directories." << std::endl
	<< "                /dir/dir1:dir2:dir3 Read the same files from '/dir/dir1' '/dir/dir2' and '/dir/dir3'." << std::endl
	<< "                requests either -r filename or -R firstfile:lastfile without pathnames" << std::endl
	<< "-m              Print netflow data date sorted. Only useful with -M" << std::endl
	<< "-R <expr>       Read input from sequence of files." << std::endl
	<< "                /any/dir  Read all files in that directory." << std::endl
	<< "                /dir/file Read all files beginning with 'file'." << std::endl
	<< "                /dir/file1:file2: Read all files from 'file1' to file2." << std::endl
	<< "-o <mode>       Use <mode> to print out netflow records:" << std::endl
	<< "                 raw      Raw record dump." << std::endl
	<< "                 line     Standard output line format." << std::endl
	<< "                 long     Standard output line format with additional fields." << std::endl
	<< "                 extended Even more information." << std::endl
	<< "                 csv      ',' separated, machine parseable output format." << std::endl
	<< "                 pipe     '|' separated legacy machine parseable output format." << std::endl
	<< "                        mode may be extended by '6' for full IPv6 listing. e.g.long6, extended6." << std::endl
	<< "-v <file>       verify netflow data file. Print version and blocks." << std::endl
	//<< "-x <file>       verify extension records in netflow data file." << std::endl
	//<< "-X              Dump Filtertable and exit (debug option)." << std::endl
	<< "-Z              Check filter syntax and exit." << std::endl
	<< "-t <time>       time window for filtering packets" << std::endl
	<< "                yyyy/MM/dd.hh:mm:ss[-yyyy/MM/dd.hh:mm:ss]" << std::endl;
}

const char* configuration::version() {
	return VERSION;
}

configuration::~configuration() {
	/* go over all vectors with strings */
	for (std::vector<stringVector* >::iterator it = parts.begin(); it != parts.end(); it++) {
		/* free allocated vectors */
		delete *it;
	}

	/* go over all columnFormats */
	for (std::vector<columnFormat*>::iterator it = columnsFormat.begin(); it != columnsFormat.end(); it++) {
		/* free allocated vectors */
		delete *it;
	}
}

configuration::configuration(): maxRecords(0), plainNumbers(false), aggregate(false), quiet(false) {}

void configuration::parseFormat(std::string format) {
	std::string alias;
	columnFormat *cf;
	regex_t reg;
	int err, groupId;
	regmatch_t matches[1];

	/* Open XML configuration file */
	pugi::xml_document doc;
	if (!doc.load_file(COLUMNS_XML)) {
		std::cerr << "XML '"<< COLUMNS_XML << "' with columns configuration cannot be loaded!" << std::endl;
	}

	/* prepare regular expresion */
	regcomp(&reg, "%[a-zA-Z]+", REG_EXTENDED);

	/* go over whole format string */
	while (format.length() > 0) {
		/* run regular expression match */
		if ((err = regexec(&reg, format.c_str(), 1, matches, 0)) == 0) {
			/* check for columns separator */
			if (matches[0].rm_so != 0 ) {
				cf = new columnFormat();
				cf->name = format.substr(0, matches[0].rm_so);
				columnsFormat.push_back(cf);
			}

			/* set alias of column */
			alias = format.substr(matches[0].rm_so, matches[0].rm_eo - matches[0].rm_so);

			/* search xml for an alias */
			pugi::xpath_node column = doc.select_single_node(("/columns/column[alias='"+alias+"']").c_str());
			/* check what we found */
			if (column != NULL) {

				/* create new column */
				if (column.node().child("default-value") != NULL) {
					cf = new columnFormat(column.node().child_value("default-value"));
				} else {
					cf = new columnFormat();
				}

				cf->name = column.node().child_value("name");
#ifdef DEBUG
				std::cerr << "Creating column '" << cf->name << "'" << std::endl;
#endif
				/* set alignment */
				if (column.node().child("alignLeft") != NULL) {
					cf->alignLeft = true;
				}

				/* set width */
				if (column.node().child("width") != NULL) {
					cf->width = atoi(column.node().child_value("width"));
				}

				/* set value according to type */
				if (column.node().child("value").attribute("type").value() == std::string("plain")) {
					/* simple element */
					cf->groups[0] = createValueElement(column.node().child("value").child("element"), doc);
				} else if (column.node().child("value").attribute("type").value() == std::string("group")) {
					/* group of elements */
					pugi::xpath_node_set groups = column.node().child("value").select_nodes("group");
					for (pugi::xpath_node_set::const_iterator it = groups.begin(); it != groups.end(); ++it)
					{
						/* create entry in columns map */
						groupId = atoi((*it).node().attribute("id").value());
						aggregateColumnsDb[groupId] = stringSet();
						/* add element */
						cf->groups[groupId] = createValueElement((*it).node().child("element"), doc);
					}
				} else if (column.node().child("value").attribute("type").value() == std::string("operation")) {
					/* operation */
					cf->groups[0] = createOperationElement(column.node().child("value").child("operation"), doc);
				}

				/* add aliases from XML to column (with starting %) */
				pugi::xpath_node_set aliases = column.node().select_nodes("alias");
				for (pugi::xpath_node_set::const_iterator it = aliases.begin(); it != aliases.end(); ++it) {
					cf->aliases.insert(it->node().child_value());
				}

				columnsFormat.push_back(cf);
			} else {
				std::cerr << "Column '" << alias << "' not defined" << std::endl;
			}

			/* discard processed part of the format string */
			format = format.substr(matches[0].rm_eo);
		} else if ( err != REG_NOMATCH ) {
			std::cerr << "Bad output format string" << std::endl;
			break;
		} else { /* rest is column separator */
			cf = new columnFormat();
			cf->name = format.substr(0, matches[0].rm_so);
			columnsFormat.push_back(cf);
		}
	}
	/* free created regular expression */
	regfree(&reg);

	/* handle aggregation:
	 * create map of colum names to column positions (for names like 'sum(e0id1)')*/
	if (aggregate) {
		/* check whether we have any aggregation groups */
		if (aggregateColumnsDb.size() == 0) {
			/* create one */
			aggregateColumnsDb[0] = stringSet();
		}

		/* create lists of aggregation columns */
		/* go over all columns */
		for (std::vector<columnFormat*>::iterator it = columnsFormat.begin(); it != columnsFormat.end(); it++) {
			/* get DB columns from columnsFormat by group */
			std::map<int, stringSet> colMap;
			stringSet intersection;

			colMap = (*it)->getColumns();
			if (colMap.size() == 0) continue; /* column separator */

			/* check whether to use this column */
			std::set_intersection((*it)->aliases.begin(), (*it)->aliases.end(), aggregateColumns.begin(),
					aggregateColumns.end(), std::inserter( intersection, intersection.begin()));

			if (!(*it)->canAggregate() && intersection.empty()) {
				/* remove columns that cannot be aggregated */
#ifdef DEBUG
				std::cerr << "Removing non-aggregable column '" << (*it)->name << "'" << std::endl;
#endif
				columnsFormat.erase(it);

				/* erase following separator */
				if ((*it)->groups.empty()) {
					columnsFormat.erase(it);
				}

				/* set iterator back to previous element */
				it--;
				continue;
			}

			/* for each aggregation group */
			for (std::map<int, stringSet>::iterator i = aggregateColumnsDb.begin(); i != aggregateColumnsDb.end(); i++) {
				/* if there is specific group, add it */
				if (colMap.find(i->first) != colMap.end()) {
					i->second.insert(colMap[i->first].begin(), colMap[i->first].end());
				} else if ((*it)->groups.size() == 1) { /* else add group 0 (non grouped element (only one group)) */
					i->second.insert(colMap[0].begin(), colMap[0].end());
				}
			}
		}

		/* check that groups are different (elements that differ could have been removed) */
		if (aggregateColumnsDb.size() > 1) {
			/* go over all groups */
			for (std::map<int, stringSet>::iterator outer = aggregateColumnsDb.begin(); outer != aggregateColumnsDb.end(); outer++) {
				/* start inner loop one element further */
				std::map<int, stringSet>::iterator it = outer;
				it++;
				/* go over rest of the elements */
				for (std::map<int, stringSet>::iterator inner = it; inner != aggregateColumnsDb.end(); inner++) {
					/* compute set difference */
					stringSet difference;
					std::set_difference(outer->second.begin(), outer->second.end(), inner->second.begin(),
										inner->second.end(), std::inserter( difference, difference.begin()));

					/* When sets are equal, difference is empty */
					if (difference.empty()) {
						/* delete duplicated group */
						aggregateColumnsDb.erase(inner);
						inner--; /* go to previous element, loop will handle increment */
					}
				}
			}
		}
	}
}

AST* configuration::createValueElement(pugi::xml_node element, pugi::xml_document &doc) {

	/* when we have alias, go down one level */
	if (element.child_value()[0] == '%') {
		pugi::xpath_node el = doc.select_single_node(
				("/columns/column[alias='"
						+ std::string(element.child_value())
						+ "']/value/element").c_str());
		return createValueElement(el.node(), doc);
	}

	/* create the element */
	AST *ast = new AST;

	ast->type = ipfixdump::value;
	ast->value = element.child_value();
	ast->semantics = element.attribute("semantics").value();
	if (element.attribute("parts")) {
		ast->parts = atoi(element.attribute("parts").value());
	}
	if (element.attribute("aggregation")) {
		ast->aggregation = element.attribute("aggregation").value();
	}

	return ast;
}

AST* configuration::createOperationElement(pugi::xml_node operation, pugi::xml_document &doc) {

	AST *ast = new AST;
	pugi::xpath_node arg1, arg2;
	std::string type;

	/* set type and operation */
	ast->type = ipfixdump::operation;
	ast->operation = operation.attribute("name").value()[0];

	/* get argument nodes */
	arg1 = doc.select_single_node(("/columns/column[alias='"+ std::string(operation.child_value("arg1"))+ "']").c_str());
	arg2 = doc.select_single_node(("/columns/column[alias='"+ std::string(operation.child_value("arg2"))+ "']").c_str());

	/* get argument type */
	type = arg1.node().child("value").attribute("type").value();

	/* add argument to AST */
	if (type == "operation") {
		ast->left = createOperationElement(arg1.node().child("value").child("operation"), doc);
	} else if (type == "plain"){
		ast->left = createValueElement(arg1.node().child("value").child("element"), doc);
	} else {
		std::cerr << "Value of type operation contains node of type " << type << std::endl;
	}

	/* same for the second argument */
	type = arg2.node().child("value").attribute("type").value();

	if (type == "operation") {
		ast->right = createOperationElement(arg2.node().child("value").child("operation"), doc);
	} else if (type == "plain"){
		ast->right = createValueElement(arg2.node().child("value").child("element"), doc);
	} else {
		std::cerr << "Value of type operation contains node of type '" << type << "'" << std::endl;
	}

	return ast;
}

}
