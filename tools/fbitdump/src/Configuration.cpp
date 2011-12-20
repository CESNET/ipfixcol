/**
 * \file Configuration.cpp
 * \author Petr Velan <petr.velan@cesnet.cz>
 * \brief Class for managing user input of fbitdump
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
#include <fstream>
#include <libgen.h>
#include <resolv.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netdb.h>


namespace fbitdump {

/* temporary macro that should be removed in full implementation */
#define NOT_SUPPORTED std::cerr << "Not supported" << std::endl; return -2;

int Configuration::init(int argc, char *argv[])
{
	char c;
	stringVector tables;
	std::string filterFile;
	std::string optionM;	/* optarg for option -M */
	std::string optionm;	/* optarg value for option -m */

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
		case 'f':
				filterFile = optarg;
			break;
		case 'n':
			NOT_SUPPORTED
			break;
		case 'c': /* number of records to display */
				this->maxRecords = atoi(optarg);
			break;
		case 'D': {
				char *nameserver;

				nameserver = optarg;

				this->resolver->setNameserver(nameserver);

				/* enable DNS caching - table for 1000 entries */
				this->resolver->enableCache(1000);

			}
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
			optionM = optarg;
			/* this option will be processed later (it depends on -r or -R */

			break;

		case 'r': {/* file to open */
                if (optarg == std::string("")) {
                	help();
                	return -2;
                }

                this->rOptarg = optarg;
                this->sanitizePath(this->rOptarg);
#ifdef DEBUG
                std::cerr << "Adding table " << this->rOptarg << std::endl;
#endif
				tables.push_back(this->rOptarg);
			break;
		}
		case 'm':
			this->optm = true;
			if (optarg != NULL)
				optionm = optarg;
			else optionm = "%ts"; /* default is timestamp column */
			break;

		case 'R':
			this->ROptarg = optarg;

			this->processROption(tables, optarg);

			break;

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
				this->timeWindow = optarg;
			break;
		default:
			help ();
			return -1;
			break;
		}
	}

	if (!optionM.empty()) {
		/* process -M option, this option depends on -r or -R */
		processMOption(tables, optionM.c_str());
	}

	/* allways process option -m, we need to know  whether aggregate or not */
	if (this->optm) {
		this->processmOption(optionm);
	}

	/* read filter */
	if (optind < argc) {
		this->filter = argv[optind];
	} else if (!filterFile.empty()) {
		std::ifstream t(filterFile, std::ifstream::in | std::ifstream::ate);

		if (!t.good()) {
			std::cerr << "Cannot open file '" << filterFile << "'" << std::endl;
			return  -2;
		}

		this->filter.reserve((unsigned int) t.tellg());
		t.seekg(0, std::ios::beg);

		this->filter.assign((std::istreambuf_iterator<char>(t)),
        std::istreambuf_iterator<char>());

	} else {
		/* set default filter */
		this->filter = "1=1";
	}

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
		size_t pos;
		if (this->getAggregate() && ((pos = this->format.find("%fl")) == std::string::npos
				|| isalnum(this->format[pos+3]))) {
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
	DIR *d;
	struct dirent *dent;

	/* do we have any tables (directories) specified? */
	if (tables.size() < 1) {
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

		while((dent = readdir(d)) != NULL) {
			if (dent->d_type == DT_DIR && atoi(dent->d_name) != 0) {
				std::string tablePath = tables[i] + std::string(dent->d_name);
				this->sanitizePath(tablePath);
				this->parts.push_back(tablePath);
			}
		}

		closedir(d);
	}

	return 0;
}

void Configuration::parseFormat(std::string format)
{
	Column *col; /* newly created column pointer */
	regex_t reg; /* the regular expresion structure */
	int err; /* check for regexec error */
	regmatch_t matches[1]; /* array of regex matches (we need only one) */
	bool removeNext = false; /* when removing column, remove the separator after it */

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
			if (matches[0].rm_so != 0 && !removeNext) {
				col = new Column();
				col->setName(format.substr(0, matches[0].rm_so));
				this->columns.push_back(col);
			}

			col = new Column();
			bool ok = true;
			std::string alias = format.substr(matches[0].rm_so, matches[0].rm_eo - matches[0].rm_so);
			/* discard processed part of the format string */
			format = format.substr(matches[0].rm_eo);

			do {
				if ((ok = col->init(doc, alias, this->aggregate)) == false) break;

				/* check that column is aggregable (is a summary column) or is aggregated */

				if (!this->getAggregate()) break; /* not aggregating, no proglem here */
				if (col->getAggregate()) break; /* collumn is a summary column, ok */

				stringSet resultSet, aliasesSet = col->getAliases();
				std::set_intersection(aliasesSet.begin(), aliasesSet.end(), this->aggregateColumnsAliases.begin(),
						this->aggregateColumnsAliases.end(), std::inserter(resultSet, resultSet.begin()));

				if (resultSet.empty()) { /* the column is not and aggregation column it will NOT have any value*/
					ok = false;
					break;
				}
			} while (false);

			/* use the column only if everything was ok */
			if (!ok) {
				delete col;
				removeNext = true;
			}
			else {
				this->columns.push_back(col);
				removeNext = false;
			}
		} else if ( err != REG_NOMATCH ) {
			std::cerr << "Bad output format string" << std::endl;
			break;
		} else if (!removeNext) { /* rest is column separator */
			col = new Column();
			col->setName(format.substr(0, matches[0].rm_so));
			this->columns.push_back(col);
		}
	}
	/* free created regular expression */
	regfree(&reg);
}

const stringVector Configuration::getPartsNames() const
{
	return this->parts;
}

std::string Configuration::getFilter() const
{
	return this->filter;
}

const stringSet Configuration::getAggregateColumns() const
{
	stringSet aggregateColumns;
	Column *col;

	/* Open XML configuration file */
	pugi::xml_document doc;
	doc.load_file(this->getXmlConfPath());

	/* go over all aliases */
	for (stringSet::const_iterator aliasIt = this->aggregateColumnsAliases.begin();
			aliasIt != this->aggregateColumnsAliases.end(); aliasIt++) {

		col = new Column();
		if (col->init(doc, *aliasIt, this->aggregate)) {
			stringSet cols = col->getColumns();
			aggregateColumns.insert(cols.begin(), cols.end());
		}
		delete col;
	}

	return aggregateColumns;
}

const stringSet Configuration::getSummaryColumns() const
{
	stringSet summaryColumns, tmp;

	/* go over all columns */
	for (columnVector::const_iterator it = columns.begin(); it != columns.end(); it++) {
		/* if column is aggregable (has summarizable columns) */
		if ((*it)->getAggregate()) {
			tmp = (*it)->getColumns();
			summaryColumns.insert(tmp.begin(), tmp.end());
		}
	}
	return summaryColumns;
}

const Column *Configuration::getOrderByColumn() const
{
	return this->orderColumn;
}

bool Configuration::getPlainNumbers() const
{
	return this->plainNumbers;
}

size_t Configuration::getMaxRecords() const
{
	return this->maxRecords;
}

bool Configuration::getAggregate() const
{
	return this->aggregate;
}

bool Configuration::getQuiet() const
{
	return this->quiet;
}

const columnVector& Configuration::getColumns() const
{
	return this->columns;
}

const std::string Configuration::version() const
{
	std::ifstream ifs;
	ifs.open("VERSION");

	std::string version;
	if (ifs.is_open()) {
		getline (ifs, version);
	}

	return version;
}

const char* Configuration::getXmlConfPath() const
{
	return (char*) COLUMNS_XML;
}

const std::string Configuration::getTimeWindowStart() const
{
	return this->timeWindow.substr(0, this->timeWindow.find('-'));
}

const std::string Configuration::getTimeWindowEnd() const
{
	size_t pos = this->timeWindow.find('-');
	if (pos == std::string::npos) {
		return "";
	}
	return this->timeWindow.substr(pos+1);
}

bool Configuration::isDirectory(std::string dir) const
{
	int ret;
	struct stat st;

	ret = stat(dir.c_str(), &st);
	if (ret != 0) {
		/* it's definitely not a directory */
		return false;
	}

	if (!S_ISDIR(st.st_mode)) {
		return false;
	}

	/* it is a directory */
	return true;
}

void Configuration::sanitizePath(std::string &path)
{
	if (path[path.length()-1] != '/') {
		path += "/";
	}
}

void Configuration::processmOption(std::string order)
{
	/* Open XML configuration file */
	pugi::xml_document doc;
	doc.load_file(this->getXmlConfPath());

	Column *col = new Column();
	do {
		if (!col->init(doc, order, this->getAggregate())) {
			std::cerr << "Cannot find column '" << order << "' to order by" << std::endl;
			break;
		}

		if (col->isOperation()) {
			std::cerr << "Cannot sort by operation column '" << order << "'." << std::endl;
			break;
		}

		this->orderColumn = col;
		return;
	} while (false);


	/* no sorting unset option m and delete the column */
	this->optm = false; /* just in case, should stay false */
	delete col;
}

void Configuration::help() const
{
	/* lines with // at the beginning should be implemented sooner */
	std::cout
	<< "usage "<< this->appName <<" [options] [\"filter\"]" << std::endl
	<< "-h              this text you see right here" << std::endl
	<< "-V              Print version and exit." << std::endl
	<< "-a              Aggregate netflow data." << std::endl
	<< "-A <expr>[/net] How to aggregate: ',' sep list of tags see fbitdump(1)" << std::endl
	<< "                or subnet aggregation: srcip4/24, srcip6/64." << std::endl
	//<< "-b              Aggregate netflow records as bidirectional flows." << std::endl
	//<< "-B              Aggregate netflow records as bidirectional flows - Guess direction." << std::endl
	<< "-r <dir>        read input tables from directory" << std::endl
//	<< "-w <file>       write output to file" << std::endl
	<< "-f              read netflow filter from file" << std::endl
//	<< "-n              Define number of top N. " << std::endl
	<< "-c              Limit number of records to display" << std::endl
	<< "-D <dns>        Use nameserver <dns> for host lookup." << std::endl
	<< "-N              Print plain numbers" << std::endl
//	<< "-s <expr>[/<order>]     Generate statistics for <expr> any valid record element." << std::endl
//	<< "                and ordered by <order>: packets, bytes, flows, bps pps and bpp." << std::endl
	<< "-q              Quiet: Do not print the header and bottom stat lines." << std::endl
	//<< "-H Add xstat histogram data to flow file.(default 'no')" << std::endl
	//<< "-i <ident>      Change Ident to <ident> in file given by -r." << std::endl
	//<< "-j <file>       Compress/Uncompress file." << std::endl
	//<< "-z              Compress flows in output file. Used in combination with -w." << std::endl
	//<< "-l <expr>       Set limit on packets for line and packed output format." << std::endl
	//<< "                key: 32 character string or 64 digit hex string starting with 0x." << std::endl
	//<< "-L <expr>       Set limit on bytes for line and packed output format." << std::endl
//	<< "-I              Print netflow summary statistics info from file, specified by -r." << std::endl
	<< "-M <expr>       Read input from multiple directories." << std::endl
	<< "                /dir/dir1:dir2:dir3 Read the same files from '/dir/dir1' '/dir/dir2' and '/dir/dir3'." << std::endl
	<< "                requests either -r filename or -R firstfile:lastfile without pathnames" << std::endl
	<< "-m              Print netflow data date sorted." << std::endl
	<< "-R <expr>       Read input from sequence of files." << std::endl
	<< "                /any/dir  Read all files in that directory." << std::endl
//	<< "                /dir/file Read all files beginning with 'file'." << std::endl
	<< "                /dir/file1:file2: Read all files from 'file1' to file2." << std::endl
	<< "-o <mode>       Use <mode> to print out netflow records:" << std::endl
//	<< "                 raw      Raw record dump." << std::endl
	<< "                 line     Standard output line format." << std::endl
	<< "                 long     Standard output line format with additional fields." << std::endl
	<< "                 extended Even more information." << std::endl
	<< "                 extra    More than you want to know..." << std::endl
	<< "                 csv      ',' separated, machine parseable output format." << std::endl
	<< "                 pipe     '|' separated legacy machine parseable output format." << std::endl
	<< "                        modes line, long, extended and extra may be extended by '4' '6' to display" << std::endl
	<< "                        only IPv4 or IPv6 addresses. e.g.long4, extended6." << std::endl
//	<< "-v <file>       verify netflow data file. Print version and blocks." << std::endl
	//<< "-x <file>       verify extension records in netflow data file." << std::endl
	//<< "-X              Dump Filtertable and exit (debug option)." << std::endl
//	<< "-Z              Check filter syntax and exit." << std::endl
	<< "-t <time>       time window for filtering packets" << std::endl
	<< "                yyyy/MM/dd.hh:mm:ss[-yyyy/MM/dd.hh:mm:ss]" << std::endl
	;
}

bool Configuration::getOptionm() const
{
	return this->optm;
}

Configuration::Configuration(): maxRecords(0), plainNumbers(false), aggregate(false), quiet(false),
		optm(false), orderColumn(NULL)
{
	this->resolver = new Resolver();
}

bool Configuration::processMOption(stringVector &tables, const char *optarg)
{
	char *optarg_copy = strdup(optarg);
	if (!optarg_copy) {
		std::cerr << "Not enough memory (" << __FILE__ << ":" << __LINE__ << ")" << std::endl;
	}

	std::string dname = dirname(optarg_copy);
	this->sanitizePath(dname);
	free(optarg_copy);
	optarg_copy = NULL;

	optarg_copy = strdup(optarg);
	if (!optarg_copy) {
		std::cerr << "Not enough memory (" << __FILE__ << ":" << __LINE__ << ")" << std::endl;
	}

	std::string bname = basename(optarg_copy);
	free(optarg_copy);

	std::vector<std::string> dirs;

	size_t last_pos = bname.length()-1;
	size_t found;
	/* separate directories (dir1:dir2:dir3) */
	while ((found = bname.rfind(':')) != std::string::npos) {
		std::string dir(dname);

		dir += bname.substr(found + 1, last_pos - found);

		this->sanitizePath(dir);

		dirs.push_back(dir);

		bname.resize(found);

		last_pos = found - 1;
	}
	/* don't forget last directory */
	dirs.push_back(dname + bname.substr(0, found));
	this->sanitizePath(dirs[dirs.size()-1]);

	/* add filename from -r option */
	if (this->rOptarg.empty() == false) {
		char *crOptarg = (char *) malloc (strlen(this->rOptarg.c_str())+1);
		if (!crOptarg) {
			std::cerr << "Not enough memory (" << __FILE__ << ":" << __LINE__ << ")" << std::endl;
			return false;
		}
		memset(crOptarg, 0, strlen(this->rOptarg.c_str())+1);

		this->rOptarg.copy(crOptarg, this->rOptarg.length());

		for (unsigned int u = 0; u < dirs.size(); u++) {
			std::string table(dirs[u] + crOptarg);

#ifdef DEBUG
			std::cerr << "Adding table " << table << std::endl;
#endif
			tables.push_back(table);
		}

		free(crOptarg);

		std::vector<std::string>::iterator iter;

		/* if -M is specified, -r is only auxiliary option. erase table specified by this option */
		for (iter = tables.begin(); iter != tables.end(); iter++) {
			if (!(*iter).compare(this->rOptarg)) {
				/* found it */
				tables.erase(iter);
				return false;
			}
		}
	} else if (this->ROptarg.empty() == false) {
		/* -r option is missing, try -R instead */
		size_t found = this->ROptarg.find(':');

		if (found != std::string::npos) {
			/* ROptarg contains colon */

			/*
			 * "file1:file2 so:
			 * firstOpt = file1
			 * secondOpt = file2
			 */
			std::string firstOpt = this->ROptarg.substr(0, found);
			std::string secondOpt = this->ROptarg.substr(found+1, this->ROptarg.length()-found);

			/* remove trailing slash, if any */
			if (firstOpt[firstOpt.length()-1] == '/') {
				firstOpt.resize(firstOpt.length()-1);
			}
			if (secondOpt[secondOpt.length()-1] == '/') {
				secondOpt.resize(secondOpt.length()-1);
			}

			struct dirent **namelist;

			/* indicates whether we already found first specified dir */
			bool firstDirFound = false;
			int counter;
			struct dirent *dent;
			bool sameLength;


			for (unsigned int u = 0; u < dirs.size(); u++) {

				int dirs_counter = scandir(dirs[u].c_str(), &namelist, NULL, alphasort);
				if (dirs_counter < 0) {
					break;
				}

				if (firstOpt.length() == secondOpt.length()) {
					sameLength = true;
				}

				counter = 0;
				firstDirFound = false;

				while(dirs_counter--) {
					dent = namelist[counter++];

					if (dent->d_type == DT_DIR && strcmp(dent->d_name, ".")
					  && strcmp(dent->d_name, "..")) {
						std::string tableDir(dirs[u].c_str());


						if (firstDirFound || !strcmp(dent->d_name, firstOpt.c_str())) {

							if ((sameLength && strlen(dent->d_name) == firstOpt.length())
							  || (!sameLength)) {
								firstDirFound = true;
								tableDir += dent->d_name;
								this->sanitizePath(tableDir);

#ifdef DEBUG
								std::cerr << "Adding table " << tableDir << std::endl;
#endif

								tables.push_back(std::string(tableDir));
							}

							if (!strcmp(dent->d_name, secondOpt.c_str())) {
								/* this is last directory we are interested in */
								free(namelist[counter-1]);
								break;
							}
						}
					}

					free(namelist[counter-1]);
				}
				free(namelist);

			}
		} else {
			/* no colon */
			for (unsigned int u = 0; u < dirs.size(); u++) {
				std::string table(dirs[u] + this->ROptarg);
				this->sanitizePath(table);

#ifdef DEBUG
				std::cerr << "Adding table " << table << std::endl;
#endif
				tables.push_back(table);
			}
		}
	}

	return true;	/* all ok */
}

bool Configuration::processROption(stringVector &tables, const char *optarg)
{

	/* find dirname() */
	char *optarg_copy = strdup(optarg);
	if (!optarg_copy) {
		std::cerr << "Not enough memory (" << __FILE__ << ":" << __LINE__ << ")" << std::endl;
		exit(EXIT_FAILURE);
	}

	std::string dname(dirname(optarg_copy));
	free(optarg_copy);
	optarg_copy = NULL;

	/* add slash on the end */
	this->sanitizePath(dname);

	/* find basename() */
	optarg_copy = strdup(optarg);
	if (!optarg_copy) {
		std::cerr << "Not enough memory (" << __FILE__ << ":" << __LINE__ << ")" << std::endl;
		exit(EXIT_FAILURE);
	}

	std::string bname(basename(optarg_copy));
	free(optarg_copy);
	optarg_copy = NULL;

	/* add slash on the end */
	this->sanitizePath(bname);

	/* check whether user specified region defined like fromDirX:toDirY */
	/* NOTE: this will not work correctly if directory name contains colon */
	size_t found = bname.find(':');
	DIR *dir;
	struct dirent *dent;

	if (found != std::string::npos) {
		/* yep, user specified region */
		std::string firstDir = bname.substr(0, found);
		/* remove slash, if any */
		if (firstDir[firstDir.length()-1] == '/') {
			firstDir.resize(firstDir.length()-1);
		}

		std::string lastDir = bname.substr(found+1, bname.length()-(found+1));
		/* remove slash, if any */
		if (lastDir[lastDir.length()-1] == '/') {
			lastDir.resize(lastDir.length()-1);
		}

		/* dirty hack, see below for more information */
		bool sameLength = (firstDir.length() == lastDir.length()) ? true : false;

		/* indicates whether we already found first specified dir */
		bool firstDirFound = false;

		struct dirent **namelist;
		int dirs_counter;
		int counter;

		/* scan for subdirectories */
		dirs_counter = scandir(dname.c_str(), &namelist, NULL, alphasort);
		if (dirs_counter < 0) {
			return false;
		}
		/*
		 * namelist now contains dirent structure for every entry in directory.
		 * the structures are sorted alphabetically, but there is one problem:
		 * ...
		 * data2/
		 * data20/  <== ****! not good for us, if user specifies "data2:data3", he only
		 * data21/            wants data2 and data3 directory, not data20/
		 * ...
		 * data29/
		 * data3/
		 * data30/
		 * ...
		 *
		 * so we will use auxiliary variable sameLength as workaround for this issue
		 */

		counter = 0;

		while(dirs_counter--) {
			dent = namelist[counter++];

			if (dent->d_type == DT_DIR && strcmp(dent->d_name, ".")
			  && strcmp(dent->d_name, "..")) {
				std::string tableDir;


				if (firstDirFound || !strcmp(dent->d_name, firstDir.c_str())) {

					if ((sameLength && strlen(dent->d_name) == firstDir.length())
					  || (!sameLength)) {
						firstDirFound = true;
						tableDir = dname + dent->d_name;
						this->sanitizePath(tableDir);

#ifdef DEBUG
						std::cerr << "Adding table " << tableDir << std::endl;
#endif

						tables.push_back(std::string(tableDir));
					}

					if (!strcmp(dent->d_name, lastDir.c_str())) {
						/* this is last directory we are interested in */
						free(namelist[counter-1]);
						break;
					}
				}
			}

			free(namelist[counter-1]);
		}
		free(namelist);

	} else {
		/* user specified parent directory only, so he wants to include
		 * all subdirectories */

		std::string parentDir(optarg);
		this->sanitizePath(parentDir);

		dir = opendir(parentDir.c_str());
		if (dir == NULL) {
			std::cerr << "Cannot open directory \"" << parentDir << "\"" << std::endl;
			return false;
		}

		while((dent = readdir(dir)) != NULL) {
			if (dent->d_type == DT_DIR && strcmp(dent->d_name, ".")
			  && strcmp(dent->d_name, "..")) {
				std::string tableDir;

				tableDir = parentDir + dent->d_name;
				this->sanitizePath(tableDir);

#ifdef DEBUG
				std::cerr << "Adding table " << tableDir << std::endl;
#endif

				tables.push_back(std::string(tableDir));
			}
		}

		closedir(dir);
	}

	return true;
}

Resolver *Configuration::getResolver() const
{
	return this->resolver;
}

Configuration::~Configuration()
{
	for (columnVector::const_iterator it = columns.begin(); it != columns.end(); it++) {
		delete *it;
	}

	delete this->resolver;
	delete this->orderColumn;
}

} /* end of fbitdump namespace */
