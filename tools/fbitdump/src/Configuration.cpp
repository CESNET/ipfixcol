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
#if HAVE_CONFIG_H
#include <config.h>
#endif
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
#include "Utils.h"


namespace fbitdump {

/* temporary macro that should be removed in full implementation */
#define NOT_SUPPORTED std::cerr << "Not supported" << std::endl; return -2;

int Configuration::init(int argc, char *argv[]) throw (std::invalid_argument)
{
	char c;
	bool maxCountSet = false;
	stringVector tables;
	std::string filterFile;
	std::string optionM;	/* optarg for option -M */
	std::string optionm;	/* optarg value for option -m */
	std::string optionr;	/* optarg value for option -r */
	char *indexes = NULL;	/* indexes optarg to be parsed later */


	if (argc == 1) {
		help();
		return 1;
	}

	/* parse command line parameters */
	while ((c = getopt (argc, argv, OPTSTRING)) != -1) {
		switch (c) {
		case 'h': /* print help */
			help();
			return 1;
			break;
		case 'V': /* print version */
			std::cout << PACKAGE << ": Version: " << VERSION << std::endl;
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
		case 'A': {/* aggregate on specific columns */
			parseAggregateArg(optarg);
			break;}
		case 'f':
				filterFile = optarg;
			break;
		case 'n':
			/* same as -c, but -c takes precedence */
			if (!maxCountSet)
				this->maxRecords = atoi(optarg);
			maxCountSet = true; /* so that statistics knows whether to change the value */
			break;
		case 'c': /* number of records to display */
			this->maxRecords = atoi(optarg);
			maxCountSet = true; /* so that statistics knows whether to change the value */
			break;
		case 'D': {
				this->resolver = new Resolver(optarg);
			}
			break;
		case 'N': /* print plain numbers */
			this->plainNumbers = true;
			break;
		case 's': {
			/* Similar to -A option*/
			this->statistics = true;

			/* we support column/order for convenience */
			std::string arg = optarg;
			std::string::size_type pos;
			char parseArg[250];
			if ((pos = arg.find('/')) != std::string::npos) { /* ordering column specified */
				if (pos > 250) { /* constant char array */
					throw std::invalid_argument("Argument for option -s is too long");
				}
				/* column name is before '/' */
				strncpy(parseArg, optarg, pos);
			} else { /* use whole optarg as column name */
				strcpy(parseArg, optarg);
			}

			parseAggregateArg(parseArg);

			this->statistics = true;

			/* sets default "-c 10", "-m %fl"*/
			if (!maxCountSet) this->maxRecords = 10;
			if (this->optm == false) {
				if (pos != std::string::npos) /* '/' found in optarg -> we have ordering column */
					optionm = optarg+pos+1;
				else
					optionm = "%fl DESC";

				/* descending ordering for statistics */
				this->orderAsc = false;
				this->optm = true;
			}

			/* Print extended bottom stats since we already have the values */
			this->extendedStats = true;

			break;}
		case 'q':
				this->quiet = true;
			break;
		case 'e':
			this->extendedStats = true;
			break;
		case 'I':
			NOT_SUPPORTED
			break;
		case 'M':
			optionM = optarg;
			/* this option will be processed later (it depends on -r or -R) */
			break;
		case 'r': {/* -M help argument */
                if (optarg == std::string("")) {
                	throw std::invalid_argument("-r requires a path to open, empty string given");
                }

                optionr = optarg;
                Utils::sanitizePath(optionr);
			break;
		}
		case 'm':
			this->optm = true;
			if (optarg != NULL)
				optionm = optarg;
			else optionm = "%ts"; /* default is timestamp column */
			break;

		case 'R':
			if (optarg == std::string("")) {
				throw std::invalid_argument("-R requires a path to open, empty string given");
			}

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
		case 'i': /* create indexes */
			this->createIndexes = true;
			if (optarg != NULL) {
				indexes = strdup(optarg);
			}
			break;
		case 'd': /* delete indexes */
			this->deleteIndexes = true;
			if (optarg != NULL) {
				indexes = strdup(optarg);
			}
			break;
		case 'C': /* Configuration file */
			if (optarg == std::string("")) {
				throw std::invalid_argument("-C requires a path to configuration file, empty string given");
			}
			this->configFile = optarg;
			break;
		case 'T': /* Template information */
			this->templateInfo = true;
			break;
		default:
			help ();
			return 1;
			break;
		}
	}

	/* open XML configuration file */
	if (!this->doc.load_file(this->getXmlConfPath())) {
		std::string err = std::string("XML '") + this->getXmlConfPath() + "' with columns configuration cannot be loaded!";
		throw std::invalid_argument(err);
	}

	if (!optionM.empty()) {
		/* process -M option, this option depends on -r or -R */
		processMOption(tables, optionM.c_str(), optionr);
	}

	/* allways process option -m, we need to know whether aggregate or not */
	if (this->optm) {
		this->processmOption(optionm);
	}

	/* parse indexes line */
	this->parseIndexColumns(indexes);
	free(indexes);

	/* read filter */
	if (optind < argc) {
		this->filter = argv[optind];
	} else if (!filterFile.empty()) {
		std::ifstream t(filterFile, std::ifstream::in | std::ifstream::ate);

		if (!t.good()) {
			throw std::invalid_argument("Cannot open file '" + filterFile + "'");
		}

		this->filter.reserve((unsigned int) t.tellg());
		t.seekg(0, std::ios::beg);

		this->filter.assign((std::istreambuf_iterator<char>(t)),
        std::istreambuf_iterator<char>());

	} else {
		/* set default filter */
		this->filter = "1=1";
	}

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
	} else if (this->format == "long6") {
		this->format = "%ts %td %pr %sa6:%sp -> %da6:%dp %flg %tos %pkt %byt %fl";
	} else if (this->format == "extended6") {
		this->format = "%ts %td %pr %sa6:%sp -> %da6:%dp %flg %tos %pkt %byt %bps %pps %bpp %fl";
	} else if (this->format == "extra6") {
		this->format = "%ts %td %pr %sa6:%sp -> %da6:%dp %flg %tos %pkt %byt %bps %pps %bpp %icmptype %sas %das %in %out %fl";
	} else {
		throw std::invalid_argument("Unknown ouput mode: '" + this->format + "'");
	}

	/* parse output format string */
	this->parseFormat(this->format);

	/* search for table parts in specified directories */
	this->searchForTableParts(tables);

	return 0;
}


void Configuration::searchForTableParts(stringVector &tables) throw (std::invalid_argument)
{
	/* do we have any tables (directories) specified? */
	if (tables.size() < 1) {
		throw std::invalid_argument("Input file(s) must be specified");
	}

	/* go over all requested directories */
	for (size_t i = 0; i < tables.size(); i++) {
		/* check whether it is fastbit part */
		if (Utils::isFastbitPart(tables[i])) {
			this->parts.push_back(tables[i]);
			continue;
		}


		/* read tables subdirectories, uses BFS */
		DIR *d;
		struct dirent *dent;
		struct stat statbuf;

		d = opendir(tables[i].c_str());
		if (d == NULL) {
			throw std::invalid_argument("Cannot open directory \"" + tables[i] + "\"");
		}

		while((dent = readdir(d)) != NULL) {
			/* construct directory path */
			std::string tablePath = tables[i] + std::string(dent->d_name);
			Utils::sanitizePath(tablePath);
			if (stat(tablePath.c_str(), &statbuf) == -1) {
				std::cerr << "Cannot stat " << dent->d_name << std::endl;
				continue;
			}

			if (S_ISDIR(statbuf.st_mode) && strcmp(dent->d_name, ".") && strcmp(dent->d_name, "..")) {
				/* test whether the directory is fastbit part */
				if (Utils::isFastbitPart(tablePath)) {
					this->parts.push_back(tablePath); /* add the part */
				} else {
					tables.push_back(tablePath); /* add the directory for further processing */
				}
			}
		}

		closedir(d);
	}

	/* throw an error if there is no fastbit part */
	if (this->parts.size() == 0) {
		throw std::invalid_argument("No tables found in specified directory");
	}
}

void Configuration::parseFormat(std::string format)
{
	Column *col; /* newly created column pointer */
	regex_t reg; /* the regular expresion structure */
	int err; /* check for regexec error */
	regmatch_t matches[1]; /* array of regex matches (we need only one) */
	bool removeNext = false; /* when removing column, remove the separator after it */

		/* prepare regular expresion */
	regcomp(&reg, "%[a-zA-Z0-9]+", REG_EXTENDED);

	/* go over whole format string */
	while (format.length() > 0) {
		/* run regular expression match */
		if ((err = regexec(&reg, format.c_str(), 1, matches, 0)) == 0) {
			/* check for columns separator */
			if (matches[0].rm_so != 0 && !removeNext) {
				col = new Column(format.substr(0, matches[0].rm_so));
				this->columns.push_back(col);
			}

			bool ok = true;
			std::string alias = format.substr(matches[0].rm_so, matches[0].rm_eo - matches[0].rm_so);
			/* discard processed part of the format string */
			format = format.substr(matches[0].rm_eo);

			try {
				col = new Column(this->doc, alias, this->aggregate);

				/* check that column is aggregable (is a summary column) or is aggregated */
				do {
					if (!this->getAggregate()) break; /* not aggregating, no problem here */
					if (col->getAggregate()) break; /* column is a summary column, ok */

					stringSet resultSet, aliasesSet = col->getAliases();
					std::set_intersection(aliasesSet.begin(), aliasesSet.end(), this->aggregateColumnsAliases.begin(),
							this->aggregateColumnsAliases.end(), std::inserter(resultSet, resultSet.begin()));

					if (resultSet.empty()) { /* the column is not and aggregation column it will NOT have any value */
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
			} catch (std::exception &e) {
				std::cerr << e.what() << std::endl;
			}
		} else if ( err != REG_NOMATCH ) {
			std::cerr << "Bad output format string" << std::endl;
			break;
		} else if (!removeNext) { /* rest is column separator */
			col = new Column(format.substr(0, matches[0].rm_so));
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

	/* go over all aliases */
	for (stringSet::const_iterator aliasIt = this->aggregateColumnsAliases.begin();
			aliasIt != this->aggregateColumnsAliases.end(); aliasIt++) {

		try {
			Column col = Column(this->doc, *aliasIt, this->aggregate);
			stringSet cols = col.getColumns();
			aggregateColumns.insert(cols.begin(), cols.end());
		} catch (std::exception &e) {
			std::cerr << e.what() << std::endl;
		}
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

const stringSet& Configuration::getAggregateColumnsAliases() const
{
	return this->aggregateColumnsAliases;
}

const pugi::xml_document& Configuration::getXMLConfiguration() const
{
	return this->doc;
}

columnVector Configuration::getStatisticsColumns() const
{
	columnVector cols;

	for (columnVector::const_iterator it = this->columns.begin(); it != this->columns.end(); it++) {
		if ((*it)->isSummary()) {
			cols.push_back(*it);
		}
	}

	return cols;
}

bool Configuration::getStatistics() const
{
	return this->statistics;
}

bool Configuration::getExtendedStats() const
{
	return this->extendedStats;
}

const char* Configuration::getXmlConfPath() const
{
	return this->configFile.c_str();
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

bool Configuration::getOrderAsc() const
{
	return this->orderAsc;
}

bool Configuration::getCreateIndexes() const
{
	return this->createIndexes;
}


bool Configuration::getDeleteIndexes() const
{
	return this->deleteIndexes;
}


stringSet Configuration::getColumnIndexes() const
{
	return this->indexColumns;
}

bool Configuration::getTemplateInfo() const
{
	return this->templateInfo;
}

void Configuration::processmOption(std::string order)
{
	std::string::size_type pos;
	if ((pos = order.find("ASC")) != std::string::npos) {
		order = order.substr(0, pos); /* trim! */
		this->orderAsc = true;
	} else if ((pos = order.find("DESC")) != std::string::npos) {
		order = order.substr(0, pos);
		this->orderAsc = false;
	}

	/* one more time to trim trailing whitespaces */
	if ((pos = order.find(' ')) != std::string::npos) {
		order = order.substr(0, pos);
	}

	try {
		Column *col = new Column(doc, order, this->getAggregate());

		if (!col->isOperation()) { /* column is not an operation, it is ok to sort by it */
			this->orderColumn = col;
			return;
		}
	} catch (std::exception &e) {
		std::cerr << e.what() << std::endl;
		std::cerr << "Cannot find column '" << order << "' to order by" << std::endl;
	}

	std::cerr << "Cannot sort by operation column '" << order << "'." << std::endl;
	this->optm = false;
}

void Configuration::help() const
{
	/* lines with // at the beginning should be implemented sooner */
	std::cout
	<< "usage "<< PACKAGE <<" [options] [\"filter\"]" << std::endl
	<< "-h              this text you see right here" << std::endl
	<< "-V              Print version and exit." << std::endl
	<< "-a              Aggregate netflow data." << std::endl
	<< "-A <expr>[/net] How to aggregate: ',' sep list of tags see fbitdump(1)" << std::endl
	<< "                or subnet aggregation: srcip4/24, srcip6/64." << std::endl
	//<< "-b              Aggregate netflow records as bidirectional flows." << std::endl
	//<< "-B              Aggregate netflow records as bidirectional flows - Guess direction." << std::endl
//	<< "-w <file>       write output to file" << std::endl
	<< "-f              read netflow filter from file" << std::endl
	<< "-n              Define number of top N. -c option takes precedence over -n." << std::endl
	<< "-c              Limit number of records to display" << std::endl
	<< "-D <dns>        Use nameserver <dns> for host lookup. Does not support IPv6 addresses." << std::endl
	<< "-N              Print plain numbers" << std::endl
	<< "-s <column>[/<order>]     Generate statistics for <column> any valid record element." << std::endl
	<< "                and ordered by <order>. Order can be any summarizable column, just as for -m option." << std::endl
	<< "-q              Quiet: Do not print the header and bottom stat lines." << std::endl
	<< "-e				Extended bottom stats. Print summary of statistics columns." << std::endl
	//<< "-H Add xstat histogram data to flow file.(default 'no')" << std::endl
	<< "-i [column1[,column2,...]]	Build indexes for given columns (or all) for specified data." << std::endl
	<< "-d [column1[,column2,...]]	Delete indexes for given columns (or all) for specified data." << std::endl
	//<< "-j <file>       Compress/Uncompress file." << std::endl
	//<< "-z              Compress flows in output file. Used in combination with -w." << std::endl
	//<< "-l <expr>       Set limit on packets for line and packed output format." << std::endl
	//<< "                key: 32 character string or 64 digit hex string starting with 0x." << std::endl
	//<< "-L <expr>       Set limit on bytes for line and packed output format." << std::endl
//	<< "-I              Print netflow summary statistics info from file, specified by -r." << std::endl
	<< "-M <expr>       Read input from multiple directories." << std::endl
	<< "                /dir/dir1:dir2:dir3 Read the same files from '/dir/dir1' '/dir/dir2' and '/dir/dir3'." << std::endl
	<< "                requests -r dir or -r firstdir:lastdir without pathnames." << std::endl
	<< "-r <expr>       Specifies subdirectory or subdirectories for -M, usable only with -M." << std::endl
	<< "                expr can be dir, which loads the dir from all directories specified in -M," << std::endl
	<< "				or dir1:dir2, which reads data from subdirectories 'dir1' to 'dir2', in directories from -M." << std::endl
	<< "-m [column]             Print netflow data date sorted. Takes optional parameter '%column' to sort by." << std::endl
	<< "-R <expr>       Read input from directory (and subdirectories recursively). Can be repeated." << std::endl
	<< "                /any/dir  Read all data from directory 'dir'." << std::endl
	<< "                /dir/dir1:dir2: Read all data from directories 'dir1' to 'dir2'." << std::endl
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
	<< "-C <path>       path to configuration file. Default is "CONFIG_XML << std::endl
	<< "-T              print information about templates in directories specified by -R" << std::endl
	;
}

bool Configuration::getOptionm() const
{
	return this->optm;
}

Configuration::Configuration(): maxRecords(0), plainNumbers(false), aggregate(false), quiet(false),
		optm(false), orderColumn(NULL), resolver(NULL), statistics(false), orderAsc(true), extendedStats(false),
		createIndexes(false), deleteIndexes(false), configFile(CONFIG_XML)
{
}

void Configuration::processMOption(stringVector &tables, const char *optarg, std::string &optionr)
{
	if (optionr.empty()) {
		throw std::invalid_argument("Option -M requires -r to specify subdirectories!");
	}

	char *optarg_copy = strdup(optarg);
	if (!optarg_copy) {
		std::cerr << "Not enough memory (" << __FILE__ << ":" << __LINE__ << ")" << std::endl;
	}

	std::string dname = dirname(optarg_copy);
	Utils::sanitizePath(dname);
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
		Utils::sanitizePath(dir);
		dirs.push_back(dir);
		bname.resize(found);

		last_pos = found - 1;
	}
	/* don't forget last directory */
	dirs.push_back(dname + bname.substr(0, found));
	Utils::sanitizePath(dirs[dirs.size()-1]);


	/* process the -r option */
	found = optionr.find(':');

	if (found != std::string::npos) {
		/* rOptarg contains colon */

		/*
		 * "file1:file2 so:
		 * firstOpt = file1
		 * secondOpt = file2
		 */
		std::string firstOpt = optionr.substr(0, found);
		std::string secondOpt = optionr.substr(found+1, optionr.length()-found);

		for (unsigned int u = 0; u < dirs.size(); u++) {
			Utils::loadDirRange(dirs[u], firstOpt, secondOpt, tables);
		}
	} else {
		/* no colon */
		for (unsigned int u = 0; u < dirs.size(); u++) {
			std::string table(dirs[u] + optionr);
			Utils::sanitizePath(table);
			tables.push_back(table);
		}
	}
	/* all done */
}

void Configuration::processROption(stringVector &tables, const char *optarg)
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
	Utils::sanitizePath(dname);

	/* find basename() */
	optarg_copy = strdup(optarg);
	if (!optarg_copy) {
		std::cerr << "Not enough memory (" << __FILE__ << ":" << __LINE__ << ")" << std::endl;
		exit(EXIT_FAILURE);
	}

	std::string bname(basename(optarg_copy));
	free(optarg_copy);
	optarg_copy = NULL;

	/* check whether user specified region defined like fromDirX:toDirY */
	/* NOTE: this will not work correctly if directory name contains colon */
	size_t found = bname.find(':');

	if (found != std::string::npos) {
		/* yep, user specified region */
		std::string firstDir = bname.substr(0, found);
		std::string lastDir = bname.substr(found+1, bname.length()-(found+1));

		Utils::loadDirRange(dname, firstDir, lastDir, tables);
	} else {
		/* user specified parent directory only, so he wants to include
		 * all subdirectories */

		std::string parentDir(optarg);
		Utils::sanitizePath(parentDir);
		tables.push_back(std::string(parentDir));
	}
}

void Configuration::parseAggregateArg(char *arg) throw (std::invalid_argument)
{
	this->aggregate = true;

	/* add aggregate columns to set */
	this->aggregateColumnsAliases.clear();
	if (!Utils::splitString(arg, this->aggregateColumnsAliases)) {
		throw std::invalid_argument(std::string("Ivalid input string ") + arg);
	}
}

void Configuration::parseIndexColumns(char *arg)
{
	if (arg != NULL) {
		stringSet aliases;
		Utils::splitString(arg, aliases);
		for (stringSet::const_iterator it = aliases.begin(); it != aliases.end(); it++) {
			Column col = Column(this->getXMLConfiguration(), *it, false);
			stringSet columns = col.getColumns();
			this->indexColumns.insert(columns.begin(), columns.end());
		}
	}
}

Resolver *Configuration::getResolver() const
{
	return this->resolver;
}

Configuration::~Configuration()
{
	/* delete columns */
	for (columnVector::const_iterator it = columns.begin(); it != columns.end(); it++) {
		delete *it;
	}

	delete this->resolver;
	delete this->orderColumn;
}

} /* end of fbitdump namespace */
