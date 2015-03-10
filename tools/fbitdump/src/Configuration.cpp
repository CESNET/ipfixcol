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
#include <dlfcn.h>
#include "Utils.h"
#include "DefaultPlugin.h"
#include "Verbose.h"

/* Module identifier for MSG_* */
static const char *msg_module = "configuration";

namespace fbitdump {

/* temporary macro that should be removed in full implementation */
#define NOT_SUPPORTED std::cerr << "Not supported" << std::endl; return -2;

int Configuration::init(int argc, char *argv[])
{
	char c;
	bool maxCountSet = false;
	stringVector tables;
	std::string filterFile;
	std::string optionM;	/* optarg for option -M */
	std::string optionm;	/* optarg value for option -m */
	std::string optionr;	/* optarg value for option -r */
	std::string indexes;	/* indexes optarg to be parsed later */
	bool print_semantics = false;
	bool print_formats = false;
	bool print_modules = false;

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
		case 'A': /* aggregate on specific columns */
			parseAggregateArg(optarg);
			break;
		case 'f':
			if (optarg == std::string("")) {
				throw std::invalid_argument("-f requires filter file specification");
			}

			filterFile = optarg;
			break;
		case 'n':
			/* same as -c, but -c takes precedence */
			if (!maxCountSet) {
				if (optarg == std::string("")) {
					throw std::invalid_argument("-n requires a number specification");
				}
				this->maxRecords = atoi(optarg);
			}

			maxCountSet = true; /* so that statistics knows whether to change the value */
			break;
		case 'c': /* number of records to display */
			if (optarg == std::string("")) {
				throw std::invalid_argument("-c requires a number specification");
			}

			this->maxRecords = atoi(optarg);
			maxCountSet = true; /* so that statistics knows whether to change the value */
			break;
		case 'D':
			if (optarg == std::string("")) {
				throw std::invalid_argument("-D requires a nameserver specification");
			}

			this->resolver = new Resolver(optarg);
			break;
		case 'N': /* print plain numbers */
			if (optarg == NULL || optarg == std::string("")) {
				/* If the value after '-N' (separated by whitespace) is an integer,
				 * the user has probably used a whitespace mistakenly.
				 */
				if (optind < argc && Utils::strtoi(argv[optind], 10) < INT_MAX) {
					this->plainLevel = Utils::strtoi(argv[optind], 10);

					// Skip to next argument; make sure integer is not parsed as query filter
					++optind;
				} else {
					this->plainLevel = 1;
				}
			} else {
				this->plainLevel = Utils::strtoi(argv[optind], 10);

				if (this->plainLevel == INT_MAX) {
					throw std::invalid_argument("-N requires an integer level specification");
				}
			}

			break;
		case 's': {
			/* Similar to -A option*/
			this->statistics = true;

			/* we support column/order for convenience */
			std::string arg = optarg;
			std::string::size_type pos;
			char parseArg[250];
			if ((pos = arg.find('/')) != std::string::npos) { /* ordering column specified */
				if (pos >= 250) { /* constant char array */
					throw std::invalid_argument("Argument for option -s is too long");
				}
				/* column name is before '/' */
				Utils::strncpy_safe(parseArg, optarg, pos+1);
				parseArg[pos] = '\0';
			} else { /* use whole optarg as column name */
				Utils::strncpy_safe(parseArg, optarg, arg.size()+1);
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
			if (optarg == std::string("")) {
				throw std::invalid_argument("-M requires a directory specification");
			}

			optionM = optarg;
			/* this option will be processed later (it depends on -r or -R) */
			break;
		case 'r': {/* -M help argument */
			if (optarg == std::string("")) {
				throw std::invalid_argument("-r requires a path specification");
			}

			optionr = optarg;
			Utils::sanitizePath(optionr);
			break;
		}
		case 'm':
			this->optm = true;
			if (optarg == NULL || optarg == std::string("")) {
				optionm = "%ts"; /* default is timestamp column */
			} else {
				optionm = optarg;
			}

			break;

		case 'R':
			if (optarg == std::string("")) {
				throw std::invalid_argument("-R requires a path specification");
			}

			this->processROption(tables, optarg);

			break;

		case 'o': /* output format */
			if (optarg == std::string("")) {
				throw std::invalid_argument("-o requires an output path specification");
			}
			
			this->format = optarg;
			break;
		case 'p':
			if (optarg == std::string("")) {
				throw std::invalid_argument("-p requires a path to open, empty string given");
			}
			if (access ( optarg, F_OK ) != 0 ) {
				throw std::invalid_argument("Cannot access pipe");
			}
			this->pipe_name = optarg;

			break;

		case 'v':
			if (optarg == std::string("")) {
				throw std::invalid_argument("-v requires a verbosity level specification");
			}
			
			verbose = atoi(optarg);
			break;
		case 'Z':
			this->checkFilters = true;
			break;
		case 't':
			if (optarg == std::string("")) {
				throw std::invalid_argument("-t requires a time window specification");
			}
			
			this->timeWindow = optarg;
			break;
		case 'i': /* create indexes */
			this->createIndexes = true;
			if (optarg != NULL) {
				indexes = optarg;
			}
			break;
		case 'd': /* delete indexes */
			this->deleteIndexes = true;
			if (optarg != NULL) {
				indexes = optarg;
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
		case 'S': /* Template information */
			print_semantics = true;
			break;
		case 'O':
			print_formats = true;
			break;
		case 'l':
			print_modules = true;
			break;
		case 'P':
			if (optarg == std::string("")) {
				throw std::invalid_argument("-P requires a filter specification");
			}
			
			this->aggregateFilter = optarg;
			break;
		default:
			help ();
			return 1;
			break;
		}
	}

	if (this->pipe_name == std::string("") ) {
		this->pipe_name = "/var/tmp/expiredaemon-queue";
	}

	/* open XML configuration file */
	Utils::printStatus("Parsing configuration");
	if (!this->doc.load_file(this->getXmlConfPath())) {
		std::string err = std::string("XML '") + this->getXmlConfPath() + "' with columns configuration cannot be loaded!";
		throw std::invalid_argument(err);
	}

	if (print_formats) {
		this->printOutputFormats();
		return 1;
	}

	if (print_modules) {
		this->loadModules();
		this->printModules();
		return 1;
	}
	
	if (!optionM.empty()) {
		/* process -M option, this option depends on -r or -R */
		processMOption(tables, optionM.c_str(), optionr);
	}

	/* always process option -m, we need to know whether aggregate or not */
	if (this->optm) {
		this->processmOption(optionm);
	}
	Utils::printStatus( "Parsing column indexes");

	/* parse indexes line */
	this->parseIndexColumns(const_cast<char*>(indexes.c_str()));

	Utils::printStatus( "Loading modules");

	this->loadModules();

	/* read filter */
	if (optind < argc) {
		this->filter = argv[optind];
	} else if (!filterFile.empty()) {
		std::ifstream t(filterFile.c_str(), std::ifstream::in | std::ifstream::ate);

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
	
	if (this->checkFilters) {
		/* Get summary columns for aggregate filter */
		this->loadOutputFormat();
		this->parseFormat(this->format, optionm);
		
		this->instance = this;
		return 0;
	}

	/* Set default plugin functions */
	this->plugins["ipv4"].format = printIPv4;
	this->plugins["ipv6"].format = printIPv6;
	this->plugins["tmstmp64"].format = printTimestamp64;
	this->plugins["tmstmp32"].format = printTimestamp32;
	this->plugins["protocol"].format = printProtocol;
	this->plugins["tcpflags"].format = printTCPFlags;
	this->plugins["duration"].format = printDuration;


	this->plugins["tcpflags"].parse = parseFlags;
	this->plugins["protocol"].parse = parseProto;
	this->plugins["duration"].parse = parseDuration;

	Utils::printStatus( "Preparing output format");

	/* prepare output format string from configuration file */
	this->loadOutputFormat();

	/* parse output format string */
	this->parseFormat(this->format, optionm);
	if( print_semantics ) {
		std::cout << "Available semantics: " <<  std::endl;
		for (pluginMap::iterator it = this->plugins.begin(); it != this->plugins.end(); ++it) {
			std::cout << "\t" << it->first << std::endl;
		}
		return 1;
	}

	Utils::printStatus( "Searching for table parts");

	/* search for table parts in specified directories */
	this->searchForTableParts(tables);
	this->instance = this;

	return 0;
}

void Configuration::printOutputFormats()
{
	pugi::xpath_node output_path = this->getXMLConfiguration().select_single_node("/configuration/output");
	if (output_path == NULL) {
		std::cout << "No output format found\n";
		return;
	}
	pugi::xml_node output = output_path.node();

	/* Print all formats */
	std::cout << "Available output formats:\n";
	for (pugi::xml_node::iterator it = output.begin(); it != output.end(); ++it) {
		std::printf("\t%-15s %s\n\n", it->child_value("formatName"), it->child_value("formatString"));
	}
}

void Configuration::printModules()
{
	std::cout << "                                \n";
	for (pluginMap::iterator it = this->plugins.begin(); it != this->plugins.end(); ++it) {
		std::cout << "[Name] " << it->first << std::endl;
		if (it->second.format) {
			std::cout << "[Plain level] " << it->second.plainLevel << std::endl;
		}
		std::cout << it->second.info() << std::endl << std::endl;
	}
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
			std::cerr << "Cannot open directory \"" << tables[i] << "\"" << std::endl;
			continue;
		}

		while((dent = readdir(d)) != NULL) {
			/* construct directory path */
			std::string tablePath = tables[i] + std::string(dent->d_name);

			if (stat(tablePath.c_str(), &statbuf) == -1) {
				std::cerr << "Cannot stat " << dent->d_name << std::endl;
				continue;
			}

			if (S_ISDIR(statbuf.st_mode) && strcmp(dent->d_name, ".") && strcmp(dent->d_name, "..")) {
				std::string status;
				status = "Searching for table parts in " + tablePath;
				Utils::printStatus( status );
				Utils::sanitizePath(tablePath);

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

void Configuration::parseFormat(std::string format, std::string &orderby)
{
	Column *col; /* newly created column pointer */
	regex_t reg; /* the regular expresion structure */
	int err; /* check for regexec error */
	regmatch_t matches[1]; /* array of regex matches (we need only one) */
	bool removeNext = false; /* when removing column, remove the separator after it */
	bool order_found = false; /* check if output format contains ordering column */

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
				} else {
					if (this->plugins.find(col->getSemantics()) != this->plugins.end()) {
						col->format = this->plugins[col->getSemantics()].format;
						if (this->plugins[col->getSemantics()].init) {
							if (this->plugins[col->getSemantics()].init(col->getSemanticsParams().c_str(), &(col->pluginConf))) {
								throw std::runtime_error("Error in plugin initialization " + col->getSemantics());
							}
						}
					}
					this->columns.push_back(col);
					removeNext = false;

					if (alias == orderby) {
						order_found = true;
					}
				}
			} catch (std::exception &e) {
				std::cerr << e.what() << std::endl;
			}
		} else if ( err != REG_NOMATCH ) {
			std::cerr << "Bad output format string" << std::endl;
			break;
		} else { /* rest is column separator */
			col = new Column(format);
			this->columns.push_back(col);
			/* Nothing more to process */
			format = "";
		}
	}

	if (this->optm && !order_found) {
		std::string existing = orderby;
		for (columnVector::iterator it = this->columns.begin(); it != this->columns.end(); ++it) {
			if (!(*it)->isOperation()) {
				existing = *((*it)->getAliases().begin());
				break;
			}
		}

		if (existing == orderby) {
			MSG_ERROR("configuration", "No suitable column for sorting found in used format!");
		} else {
			MSG_WARNING("configuration", "Sorting column '%s' not found in output format, using '%s'.", orderby.c_str(), existing.c_str());
			orderby = existing;

			delete this->orderColumn;
			this->processmOption(orderby);
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

const columnVector Configuration::getAggregateColumns() const
{
	columnVector aggregateColumns;

	/* go over all aliases */
	for (stringSet::const_iterator aliasIt = this->aggregateColumnsAliases.begin();
			aliasIt != this->aggregateColumnsAliases.end(); aliasIt++) {

		try {
			Column *col = new Column(this->doc, *aliasIt, this->aggregate);
			aggregateColumns.push_back(col);
		} catch (std::exception &e) {
			std::cerr << e.what() << std::endl;
		}
	}

	return aggregateColumns;
}

const columnVector Configuration::getSummaryColumns() const
{
	columnVector summaryColumns, tmp;

	/* go over all columns */
	for (columnVector::const_iterator it = columns.begin(); it != columns.end(); it++) {
		/* if column is aggregable (has summarizable columns) */
		if ((*it)->getAggregate()) {
			summaryColumns.push_back(*it);
		}
	}
	return summaryColumns;
}

const Column *Configuration::getOrderByColumn() const
{
	return this->orderColumn;
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

void Configuration::processmOption(std::string &order)
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

		this->orderColumn = col;
		return;
	} catch (std::exception &e) {
		std::cerr << e.what() << std::endl;
		std::cerr << "Cannot find column '" << order << "' to order by" << std::endl;
	}
}

void Configuration::help() const
{
	/* lines with // at the beginning should be implemented sooner */
	std::cout
	<< "usage "<< PACKAGE <<" [options] [\"filter\"]" << std::endl
	<< "-h              Show this help" << std::endl
	<< "-v <level>      Set verbosity level" << std::endl
	<< "-V              Print version and exit" << std::endl
	<< "-a              Aggregate flow data" << std::endl
	<< "-A[<expr>]     Aggregation fields, separated by ','. Please check fbitdump(1) for a list of supported fields" << std::endl
//	<< "                or subnet aggregation: srcip4/24, srcip6/64." << std::endl
	//<< "-b              Aggregate flow records as bidirectional flows." << std::endl
	//<< "-B              Aggregate flow records as bidirectional flows - Guess direction." << std::endl
//	<< "-w <file>       write output to file" << std::endl
	<< "-f <file>       Read flow filter from file" << std::endl
	<< "-n <number>     Define number of top N. -c option takes precedence over -n" << std::endl
	<< "-c <number>     Limit number of records to display" << std::endl
	<< "-D <dns>        Use nameserver <dns> for host lookup. Does not support IPv6 addresses" << std::endl
	<< "-N[<level>]     Set plain number printing level. Please check fbitdump(1) for detailed information" << std::endl
	<< "-s <column>[/<order>]     Generate statistics for <column> any valid record element" << std::endl
	<< "                and ordered by <order>. Order can be any summarizable column, just as for -m option" << std::endl
	<< "-q              Quiet: do not print statistics" << std::endl
	<< "-e              Extended statistics: also prints summary of statistics columns" << std::endl
	//<< "-H Add xstat histogram data to flow file.(default 'no')" << std::endl
	<< "-i[column1[,column2,...]]	Build indexes for given columns (or all) for specified data" << std::endl
	<< "-d[column1[,column2,...]]	Delete indexes for given columns (or all) for specified data" << std::endl
	//<< "-j <file>       Compress/Uncompress file." << std::endl
	//<< "-z              Compress flows in output file. Used in combination with -w." << std::endl
	//<< "-l <expr>       Set limit on packets for line and packed output format." << std::endl
	//<< "                key: 32 character string or 64 digit hex string starting with 0x." << std::endl
	//<< "-L <expr>       Set limit on bytes for line and packed output format." << std::endl
//	<< "-I              Print flow summary statistics info from file, specified by -r." << std::endl
	<< "-M <expr>       Read input from multiple directories" << std::endl
	<< "                /dir/dir1:dir2:dir3 Read the same files from '/dir/dir1' '/dir/dir2' and '/dir/dir3'" << std::endl
	<< "                requests -r dir or -r firstdir:lastdir without pathnames" << std::endl
	<< "-r <expr>       Specifies subdirectory or subdirectories for -M, usable only with -M" << std::endl
	<< "                expr can be dir, which loads the dir from all directories specified in -M," << std::endl
	<< "				or dir1:dir2, which reads data from subdirectories 'dir1' to 'dir2', in directories from -M" << std::endl
	<< "-m [column]     Print flow data date sorted. Takes optional parameter '%column' to sort by" << std::endl
	<< "-R <expr>       Recursively read input from directory and subdirectories; can be repeated" << std::endl
	<< "                /any/dir        Reads all data from directory 'dir'" << std::endl
	<< "                /dir/dir1:dir2  Reads all data from directory 'dir1' to 'dir2'" << std::endl
	<< "-o <mode>       Use <mode> to print out flow records:" << std::endl
//	<< "                 raw      Raw record dump." << std::endl
	<< "                 line     Standard output line format." << std::endl
	<< "                 long     Standard output line format with additional fields" << std::endl
	<< "                 extended Even more information" << std::endl
	<< "                 extra    More than you want to know..." << std::endl
	<< "                 csv      ',' separated, machine parseable output format" << std::endl
	<< "                 pipe     '|' separated legacy machine parseable output format" << std::endl
	<< "                        modes line, long, extended and extra may be extended by '4' or '6' to display" << std::endl
	<< "                        only IPv4 or IPv6 addresses. Examples: long4, extended6" << std::endl
//	<< "-v <file>       verify flow data file. Print version and blocks." << std::endl
	//<< "-x <file>       verify extension records in flow data file." << std::endl
	//<< "-X              Dump Filtertable and exit (debug option)." << std::endl
	<< "-Z              Check filter syntax and exit" << std::endl
	<< "-t <time>       Time window for filtering packets: yyyy/MM/dd.hh:mm:ss[-yyyy/MM/dd.hh:mm:ss]" << std::endl
	<< "-C <path>       Path to configuration file. Default is " << CONFIG_XML << std::endl
	<< "-T              Print information about templates in directories specified by -R" << std::endl
	<< "-S              Print available semantics" << std::endl
	<< "-O              Print available output formats" << std::endl
	<< "-l              Print plugin list" << std::endl
	<< "-P <filter>     Post-aggregation filter (only supported with -A, containing columns in aggregated table only)" << std::endl
	;
}

bool Configuration::getOptionm() const
{
	return this->optm;
}

Configuration::Configuration(): maxRecords(0), plainLevel(0), aggregate(false), quiet(false),
		optm(false), orderColumn(NULL), resolver(NULL), statistics(false), orderAsc(true), extendedStats(false),
		createIndexes(false), deleteIndexes(false), configFile(CONFIG_XML), templateInfo(false)
{
}

void Configuration::pushCheckDir(std::string &dir, std::vector<std::string> &list)
{
	if (!access(dir.c_str(), F_OK)) {
		list.push_back(dir);
	} else {
		std::cerr << "Cannot open directory \"" << dir << "\"" << std::endl;
	}
}

void Configuration::processMOption(stringVector &tables, const char *optarg, std::string &optionr)
{
	if (optionr.empty()) {
		throw std::invalid_argument("Option -M requires -r to specify subdirectories!");
	}

	std::vector<std::string> dirs;

	std::string arg = optarg;
	size_t pos = arg.find(':');

	if (pos == arg.npos) {
		/* Only one directory */
		Utils::sanitizePath(arg);
		dirs.push_back(arg);
	} else {
		/* Get base directory (first in list) */
		std::string base = arg.substr(0, pos);
		do {
			/* Get next directory in list */
			std::string root = base;
			arg = arg.substr(pos + 1);
			pos = arg.find(':');
			std::string sub = (pos == arg.npos) ? arg : arg.substr(0, pos);
			Utils::sanitizePath(sub);
			
			/* Count directory depth */
			int depth = std::count(sub.begin(), sub.end(), '/');
			while (depth--) {
				/* Base must be the same depth */
				size_t slash = root.find_last_of('/');
				if (slash == root.npos) {
					root = "";
					break;
				}

				/* One level up */
				root = root.substr(0, slash);
			}


			if (!root.empty()) {
				Utils::sanitizePath(root);
			}

			/* Store directory */
			std::string ndir = root +  sub;
			dirs.push_back(root + sub);
		} while (pos != arg.npos);

		/* Store base directory */
		Utils::sanitizePath(base);
		dirs.push_back(base);	
	}
	
	/* process the -r option */
	size_t found = optionr.find(':');

	if (found != std::string::npos) {
		/* rOptarg contains colon */

		/*
		 * "file1:file2 so:
		 * firstOpt = file1
		 * secondOpt = file2
		 */
		std::string firstOpt = optionr.substr(0, found);
		std::string secondOpt = optionr.substr(found+1, optionr.length()-found);
		
		Utils::sanitizePath(secondOpt);
		uint16_t right_depth = std::count(secondOpt.begin(), secondOpt.end(), '/');

		std::string root = firstOpt;
		while (right_depth--) {
			size_t slash = root.find_last_of('/');
			if (slash == root.npos) {
				/* No more parents */
				root = "";
				break;
			}

			/* Go one level up */
			root = root.substr(0, root.find_last_of('/'));
		}

		if (firstOpt == root || root.empty()) { 
			root = "";
		} else {
			firstOpt = firstOpt.substr(root.length() + 1);
			Utils::sanitizePath(root);
		}

		Utils::sanitizePath(firstOpt);

		for (unsigned int u = 0; u < dirs.size(); u++) {
			Utils::loadDirsTree(dirs[u] + root, firstOpt, secondOpt, tables);
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
	std::string arg = optarg;

	/* Find separator */
	size_t pos = arg.find(':');
	if (pos == arg.npos) {
		/* Specified only one directory */
	Utils::sanitizePath(arg);
		tables.push_back(arg);
		return;
	}

	/* Get left and right path */
	std::string left = arg.substr(0, pos);
	std::string right = arg.substr(pos + 1);

	/* Right path is ready and can be sanitized */
	Utils::sanitizePath(right);

	/* Get root directory (== left - depth of right) */
	uint16_t right_depth = std::count(right.begin(), right.end(), '/');

	std::string root = left;
	while (right_depth--) {
	   root = root.substr(0, root.find_last_of('/'));
	}

	/* Get first firectory */
	if (root == left) {
	   root = "./";
	} else {
	   left = left.substr(root.length() + 1);
	}

	Utils::sanitizePath(root);
	Utils::sanitizePath(left);

	/* Load dirs */
	Utils::loadDirsTree(root, left, right, tables);
}

void Configuration::parseAggregateArg(char *arg) throw (std::invalid_argument)
{
	this->aggregate = true;

	/* add aggregate columns to set */
	this->aggregateColumnsAliases.clear();

	/* NULL argument means no args, no further processing */
	if (arg == NULL) return;

	if (!Utils::splitString(arg, this->aggregateColumnsAliases)) {
		throw std::invalid_argument(std::string("Invalid input string '") + arg + std::string("'"));
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

void Configuration::loadOutputFormat()
{
	/* Look out for custom format */
	if (this->format.substr(0,4) == "fmt:") {
		this->format = this->format.substr(4);
		return;
	}

	/* Use 'line' as a default format */
	if (this->format.empty()) {
		this->format = "line";
	}

	/* Try to find the format in configuration XML */
	pugi::xpath_node format = this->getXMLConfiguration().select_single_node(("/configuration/output/format[formatName='"+this->format+"']").c_str());
	/* check what we found */
	if (format == NULL) {
		throw std::invalid_argument(std::string("Format '") + this->format + "' not defined");
	}

	/* set format string */
	if (format.node().child("formatString") != NULL) {
		this->format = format.node().child_value("formatString");
	} else {
		throw std::invalid_argument("Missing format string for '" + this->format + "'");
	}
}

Resolver *Configuration::getResolver() const
{
	return this->resolver;
}

void Configuration::loadModules()
{
	pugi::xpath_node_set nodes = this->getXMLConfiguration().select_nodes("/configuration/plugins/plugin");
	std::string path, plainLevel;
	pluginConf aux_conf;
	
	for (pugi::xpath_node_set::const_iterator ii = nodes.begin(); ii != nodes.end(); ii++) {
		pugi::xpath_node node = *ii;
		if (this->plugins.find(node.node().child_value("name")) != this->plugins.end()) {
			MSG_ERROR(msg_module, "Duplicit module names %s", node.node().child_value("name"));
			continue;
		}

		path = node.node().child_value("path");
		plainLevel = node.node().child_value("plainLevel");
		
		if (!plainLevel.empty()) {
			aux_conf.plainLevel = atoi(plainLevel.c_str());
		} else {
			aux_conf.plainLevel = 1;
		}
		
		if (access(path.c_str(), X_OK) != 0) {
			MSG_WARNING(msg_module, "Cannot access %s", path.c_str());
		}

		aux_conf.handle = dlopen(path.c_str(), RTLD_LAZY);
		if (!aux_conf.handle) {
			std::cerr << dlerror() << std::endl;
			continue;
		}

		/* Look for functions */
		*(void **)(&(aux_conf.init)) = dlsym(aux_conf.handle, "init");
		*(void **)(&(aux_conf.close)) = dlsym(aux_conf.handle, "close");
		*(void **)(&(aux_conf.format)) = dlsym(aux_conf.handle, "format" );
		*(void **)(&(aux_conf.parse)) = dlsym(aux_conf.handle, "parse");
		*(void **)(&(aux_conf.info)) = dlsym(aux_conf.handle, "info");

		/* Check whether plugin has info function */
		if (!aux_conf.info) {
			MSG_ERROR(msg_module, "Plugin without info function, skipping %s", path.c_str());
			dlclose(aux_conf.handle);
			continue;
		}
		
		/* Check whether plugin has at least one data processing function */
		if (!aux_conf.format && !aux_conf.parse) {
			MSG_ERROR(msg_module, "Plugin with no data processing function, skipping %s", path.c_str());
			dlclose(aux_conf.handle);
			continue;
		}
		
		this->plugins[node.node().child_value("name")] = aux_conf;
	}
}

void Configuration::unloadModules() {
	for (pluginMap::iterator it = this->plugins.begin(); it != this->plugins.end(); ++it) {
		if (it->second.handle) {
			dlclose(it->second.handle);
		}
	}
	this->plugins.clear();
}

Configuration::~Configuration()
{
	/* delete columns */
	for (auto col: columns) {
		if (plugins.find(col->getSemantics()) != plugins.end()) {
			if (plugins[col->getSemantics()].close) {
				plugins[col->getSemantics()].close(&(col->pluginConf));
			}
		}

		delete col;
	}

	this->unloadModules();

	delete this->resolver;
	delete this->orderColumn;
}

/* Static variable with Configuration object */
Configuration * Configuration::instance;
} /* end of fbitdump namespace */
