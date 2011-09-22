/**
 * \file Configuration.cpp
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

#include "Configuration.h"
#include <getopt.h>
#include <regex.h>
#include <dirent.h>
#include <cstring>

namespace ipfixdump {

/* temporary macro that should be removed in full implementation */
#define NOT_SUPPORTED std::cerr << "Not supported" << std::endl; return -2;

int Configuration::init(int argc, char *argv[])
{
	char c;
	stringVector tables;
	/* get program name without execute path */
	this->appName = ((this->appName = strrchr (argv[0], '/')) != NULL) ? (this->appName + 1) : argv[0];
	argv[0] = this->appName;

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
			std::cout << this->appName << ": Version: " << version() << std::endl;
			return 1;
			break;
		case 'a': /* aggregate */
			this->aggregate = true;
			/* set default aggregation columns */
			if (this->aggregateColumnsAliases.size() == 0) {
				this->aggregateColumnsAliases.insert("%sa4");
				this->aggregateColumnsAliases.insert("%da4");
				this->aggregateColumnsAliases.insert("%sa6");
				this->aggregateColumnsAliases.insert("%da6");
				this->aggregateColumnsAliases.insert("%sp");
				this->aggregateColumnsAliases.insert("%dp");
				this->aggregateColumnsAliases.insert("%pr");
			}
			break;
		case 'A': /* aggregate on specific columns */
			char *token;
			this->aggregate = true;
			/* add aggregate columns to set */
			this->aggregateColumnsAliases.clear();
			token = strtok(optarg, ",");
			if (token == NULL) {
				help();
				return -2;
			} else {
				this->aggregateColumnsAliases.insert(token);
				while ((token = strtok(NULL, ",")) != NULL) {
					this->aggregateColumnsAliases.insert(token);
				}
			}
			break;
		case 'r': /* file to open */
                if (optarg == std::string("")) {
                	help();
                	return -2;
                }
#ifdef DEBUG
                std::cerr << "Adding table " << optarg << std::endl;
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
				this->maxRecords = atoi(optarg);
			break;
		case 'D':
			NOT_SUPPORTED
			break;
		case 'N': /* print plain numbers */
				this->plainNumbers = true;
			break;
		case 's':
			NOT_SUPPORTED
			break;
		case 'q':
				this->quiet = true;
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
			std::string path;
			char *dirname;
			size_t dirname_len;

			dirpath = optarg;

			/* find dirname */
			dirname = strstr(optarg, "/");
			if (dirname == NULL) {
				dirname_len = 0;
			} else {
				dirname_len = dirname - optarg + 1; /* path + '/' */
			}
			if (dirname_len > 0) {
				path = dirpath.substr(0, dirname_len);
			} else {
				path = "./";
			}

			DIR *dir;
			struct dirent *dent;

			/* check whether user specified single directory or multiple directories
			 * in format firstdir:lastdir
			 * NOTE: this will not work correctly if directory name contains colon */
			if (strstr(dirpath.c_str(), ":") != NULL) {
				/* path/firstdir:lastdir - separate these directories */

				this->firstdir = strtok(optarg, ":");
				this->lastdir = strtok(NULL, ":");

				if (this->firstdir.empty()) {
					std::cerr << "Invalid firstdir" << std::endl;
					break;
				}
				if (this->lastdir.empty()) {
					std::cerr << "Invalid lastdir" << std::endl;
					break;
				}

				this->lastdir = path + this->lastdir;

				dir = opendir(path.c_str());
				if (dir == NULL) {
					std::cerr << "Cannot open directory \"" << dirpath << "\"" << std::endl;
					break;
				}

				tables.push_back(std::string(path));

			} else {
				/* user specified parent directory */
				dir = opendir(dirpath.c_str());
				if (dir == NULL) {
					std::cerr << "Cannot open directory \"" << dirpath << "\"" << std::endl;
					break;
				}

				while((dent = readdir(dir)) != NULL) {
					if (dent->d_type == DT_DIR && strcmp(dent->d_name, ".")
					  && strcmp(dent->d_name, "..")) {
						std::string tableDir(path);

						tableDir = tableDir + dent->d_name + "/";
#ifdef DEBUG
						std::cerr << "Adding table " << tableDir << std::endl;
#endif
						tables.push_back(std::string(tableDir));
					}
				}
			}

			closedir(dir);
			break;
		}
		case 'o': /* output format */
				this->format = optarg;
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
		this->filter = argv[optind];
	} else {
		/* set default filter */
		this->filter = "1=1";
	}

	/* set default order (by timestamp) */
	this->order.push_back("%ts");

	/* TODO add format to print everything */
	if (this->format.empty() || this->format == "line") {
		this->format = "%ts %td %pr %sa4:%sp -> %da4:%dp %sa6:%sp -> %da6:%dp %pkt %byt %fl";
	} else if (this->format == "long") {
		this->format = "%ts %td %pr %sa4:%sp -> %da4:%dp %sa6:%sp -> %da6:%dp %flg %tos %pkt %byt %fl";
	} else if (this->format == "extended") {
		this->format = "%ts %td %pr %sa4:%sp -> %da4:%dp %sa6:%sp -> %da6:%dp %flg %tos %pkt %byt %bps %pps %bpp %fl";
	} else if (this->format == "pipe") {
		this->format = "%ts|%td|%pr|%sa4|%sp|%da4|%dp|%pkt|%byt|%fl";
	} else if (this->format == "csv") {
		this->format = "%ts,%td,%pr,%sa4,%sp,%da4,%dp,%pkt,%byt,%fl";
	} else if (this->format.substr(0,4) == "fmt:") {
		this->format = this->format.substr(4);
		/* when aggregating, always add flows */
		if (this->getAggregate() && (this->format.find("%fl") == std::string::npos)) {
			this->format += " %fl";
		}
	} else if (this->format == "extra") {
		this->format = "%ts %td %pr %sa4 -> %da4 %sa6 -> %da6 %sp %dp %flg %tos %pkt %byt %bps %pps %bpp %icmptype %sas %das %in %out %fl";
	} else if (this->format == "line4") {
			this->format = "%ts %td %pr %sa4:%sp -> %da4:%dp %pkt %byt %fl";
	} else if (this->format == "long4") {
		this->format = "%ts %td %pr %sa4:%sp -> %da4:%dp %flg %tos %pkt %byt %fl";
	} else if (this->format == "extended4") {
		this->format = "%ts %td %pr %sa4:%sp -> %da4:%dp %flg %tos %pkt %byt %bps %pps %bpp %fl";
	} else if (this->format == "extra4") {
		this->format = "%ts %td %pr %sa4:%sp -> %da4:%dp %flg %tos %pkt %byt %bps %pps %bpp %icmptype %sas %das %in %out %fl";
	} else if (this->format == "line6") {
		this->format = "%ts %td %pr %sa6:%sp -> %da6:%dp %pkt %byt %fl";
	} else if (this->format == "long4") {
		this->format = "%ts %td %pr %sa6:%sp -> %da6:%dp %flg %tos %pkt %byt %fl";
	} else if (this->format == "extended6") {
		this->format = "%ts %td %pr %sa6:%sp -> %da6:%dp %flg %tos %pkt %byt %bps %pps %bpp %fl";
	} else if (this->format == "extra6") {
		this->format = "%ts %td %pr %sa6:%sp -> %da6:%dp %flg %tos %pkt %byt %bps %pps %bpp %icmptype %sas %das %in %out %fl";
	} else {
		std::cerr << "Unknown ouput mode: '" << this->format << "'" << std::endl;
		return -1;
	}

	/* parse output format string */
	this->parseFormat(this->format);

	/* search for table parts in specified directories */
	if (this->searchForTableParts(tables) < 0) {
		return -1;
	}

	return 0;
}

int Configuration::searchForTableParts(stringVector &tables)
{
	struct dirent *dent;
	struct dirent **namelist;
	int dirs_counter;
	int counter;
	bool firstdir_found;

	/* do we have any tables (directories) specified? */
	if (tables.size() < 1) {
		std::cerr << "Input file(s) must be specified" << std::endl;
		return -1;
	}


	/* read tables subdirectories(templates) */
	for (size_t i = 0; i < tables.size(); i++) {
		dirs_counter = scandir(tables[i].c_str(), &namelist, NULL, alphasort);
		if (dirs_counter < 0) {
			/* oops? try another directory */
			continue;
		}

//		d = opendir(tables[i].c_str());
//		if (d == NULL) {
//			std::cerr << "Cannot open directory \"" << tables[i] << "\"" << std::endl;
//			return -1;
//		}

		counter = 0;
		firstdir_found = false;
		while(dirs_counter--) {
			dent = namelist[counter++];

			if (dent->d_type == DT_DIR && atoi(dent->d_name) != 0) {
				std::string table(tables[i] + std::string(dent->d_name));

				if ((this->firstdir.empty() == false) && (this->lastdir.empty() == false)) {
					if (firstdir_found == false && !table.compare(this->firstdir)) {
						firstdir_found = true;
					}

					if (firstdir_found == true) {
						this->parts.push_back(table);
					}

					if (firstdir_found == true && !table.compare(this->lastdir)) {
						break;
					}
				} else {
					this->parts.push_back(table);
				}
			}
		}

		free(namelist);
	}

	return 0;
}

void Configuration::parseFormat(std::string format) {
	std::string alias;
	Column *col;
	regex_t reg;
	int err;
	regmatch_t matches[1];

	/* Open XML configuration file */
	pugi::xml_document doc;
	if (!doc.load_file(COLUMNS_XML)) {
		std::cerr << "XML '"<< COLUMNS_XML << "' with columns configuration cannot be loaded!" << std::endl;
	}

	/* prepare regular expresion */
	regcomp(&reg, "%[a-zA-Z0-9]+", REG_EXTENDED);

	/* go over whole format string */
	while (format.length() > 0) {
		/* run regular expression match */
		if ((err = regexec(&reg, format.c_str(), 1, matches, 0)) == 0) {
			/* check for columns separator */
			if (matches[0].rm_so != 0 ) {
				col = new Column();
				col->setName(format.substr(0, matches[0].rm_so));
				this->columns.push_back(col);
			}

			/* set alias of column */
			alias = format.substr(matches[0].rm_so, matches[0].rm_eo - matches[0].rm_so);

			/* search xml for an alias */
			pugi::xpath_node column = doc.select_single_node(("/columns/column[alias='"+alias+"']").c_str());
			/* check what we found */
			if (column != NULL) {

				/* create new column */
				if (column.node().child("default-value") != NULL) {
					col = new Column(column.node().child_value("default-value"));
				} else {
					col = new Column();
				}

				col->setName(column.node().child_value("name"));
				col->setAggregation(this->aggregate);
#ifdef DEBUG
				std::cerr << "Creating column '" << col->getName() << "'" << std::endl;
#endif
				/* set alignment */
				if (column.node().child("alignLeft") != NULL) {
					col->setAlignLeft(true);
				}

				/* set width */
				if (column.node().child("width") != NULL) {
					col->setWidth(atoi(column.node().child_value("width")));
				}

				/* set value according to type */
				if (column.node().child("value").attribute("type").value() == std::string("plain")) {
					/* simple element */
					col->setAST(createValueElement(column.node().child("value").child("element"), doc));
				} else if (column.node().child("value").attribute("type").value() == std::string("operation")) {
					/* operation */
					col->setAST(createOperationElement(column.node().child("value").child("operation"), doc));
				}

				/* add aliases from XML to column (with starting %) */
				pugi::xpath_node_set aliases = column.node().select_nodes("alias");
				for (pugi::xpath_node_set::const_iterator it = aliases.begin(); it != aliases.end(); ++it) {
					col->addAlias(it->node().child_value());
				}

				this->columns.push_back(col);
			} else {
				std::cerr << "Column '" << alias << "' not defined" << std::endl;
			}

			/* discard processed part of the format string */
			format = format.substr(matches[0].rm_eo);
		} else if ( err != REG_NOMATCH ) {
			std::cerr << "Bad output format string" << std::endl;
			break;
		} else { /* rest is column separator */
			col = new Column();
			col->setName(format.substr(0, matches[0].rm_so));
			this->columns.push_back(col);
		}
	}
	/* free created regular expression */
	regfree(&reg);


}

AST* Configuration::createValueElement(pugi::xml_node element, pugi::xml_document &doc) {

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

AST* Configuration::createOperationElement(pugi::xml_node operation, pugi::xml_document &doc) {

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

stringVector Configuration::getPartsNames() {
	return this->parts;
}

std::string Configuration::getFilter() {
	return this->filter;
}

stringSet Configuration::getAggregateColumns() {
	stringSet aggregateColumns;

	/* go over all aliases */
	for (stringSet::iterator aliasIt = this->aggregateColumnsAliases.begin();
			aliasIt != this->aggregateColumnsAliases.end(); aliasIt++) {
		bool found = false;
		/* for each alias go over all columns */
		for (columnVector::iterator columnIt = this->columns.begin(); columnIt != columns.end(); columnIt++) {
			/* get column aliases */
			stringSet aliases = (*columnIt)->getAliases();
			/* if current alias matches one of columns aliases */
			if (aliases.find(*aliasIt) != aliases.end()) {
				/* add column fastbit columns to final set */
				stringSet cols = (*columnIt)->getColumns();
				aggregateColumns.insert(cols.begin(), cols.end());
				found = true;
			}
		}
		if (!found) { /* TODO columns can still be in configuration file, but it is not displayed... */
			std::cerr << "Aggregation column '" << *aliasIt << "' not found!" << std::endl;
		}
	}

	return aggregateColumns;
}

stringSet Configuration::getSummaryColumns() {
	stringSet summaryColumns, tmp;

	/* go over all columns */
	for (columnVector::iterator it = columns.begin(); it != columns.end(); it++) {
		/* if column is aggregable (has summarizable columns) */
		if ((*it)->getAggregate()) {
			tmp = (*it)->getColumns();
			summaryColumns.insert(tmp.begin(), tmp.end());
		}
	}
	return summaryColumns;
}

stringVector Configuration::getOrder() {
	return this->order;
}

bool Configuration::getPlainNumbers() {
	return this->plainNumbers;
}

size_t Configuration::getMaxRecords() {
	return this->maxRecords;
}

bool Configuration::getAggregate() {
	return this->aggregate;
}

bool Configuration::getQuiet() {
	return this->quiet;
}

columnVector Configuration::getColumns() {
	return this->columns;
}

std::string Configuration::version() {
	return VERSION;
}

void Configuration::help() {
	std::cout
	<< "usage "<< this->appName <<" [options] [\"filter\"]" << std::endl
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
	<< "                 extra    More than you want to know..." << std::endl
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

Configuration::Configuration(): maxRecords(0), plainNumbers(false), aggregate(false), quiet(false) {}

Configuration::~Configuration() {
	for (columnVector::iterator it = columns.begin(); it != columns.end(); it++) {
		delete *it;
	}
}

} /* end of ipfixdump namespace */
