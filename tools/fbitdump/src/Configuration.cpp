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
#include <fstream>
#include <libgen.h>
#include <resolv.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>


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
		case 'f':
			NOT_SUPPORTED
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
		case 'M': {

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
				std::cout << "DEBUG x" << std::endl;
				char *crOptarg = (char *) malloc (strlen(this->rOptarg.c_str())+1);
				if (!crOptarg) {
					std::cerr << "Not enough memory (" << __FILE__ << ":" << __LINE__ << ")" << std::endl;
					break;
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
						break;
					}
				}
			} else if (this->ROptarg.empty() == false) {
				/* -r option is missing, try -R instead */
				size_t found = this->ROptarg.find(':');

				if (found != std::string::npos) {
					/* ROptarg contains colon */

					/*
					 * "file1:file2
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

			if (this->rOptarg.empty() && this->ROptarg.empty()) {
				std::cerr << "Please specify either \"-r dirname\" or \"-R firstdir:lastdir\" " <<
						"before -M option" << std::endl;
				// FIXME - exit?
			}

			break;
		}
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
			break;
		case 'R': {

			this->ROptarg = optarg;

			/* find dirname() */
			char *optarg_copy = strdup(optarg);
			if (!optarg_copy) {
				/* not enough memory */
#ifdef DEBUG
				std::cerr << "Not enough memory (" << __FILE__ << ":" << __LINE__ << ")" << std::endl;
#endif
			}

			std::string dname(dirname(optarg_copy));
			free(optarg_copy);
			optarg_copy = NULL;

			/* add slash on the end */
			this->sanitizePath(dname);


			/* find basename() */
			optarg_copy = strdup(optarg);
			if (!optarg_copy) {
				/* not enough memory */
#ifdef DEBUG
				std::cerr << "Not enough memory (" << __FILE__ << ":" << __LINE__ << ")" << std::endl;
#endif
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
					break;
				}
				/*
				 * namelist now contains dirent structure for every entry in directory.
				 * the structures are sorted alphabetically, but there is one problem:
				 * ...
				 * data2/
				 * data20/  <== ****! not good for us, if user specifies "data2:data3", he only
				 * data21/            wants data2 and data3 directory.
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
					break;
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
				this->timeWindow = optarg;
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

			col = new Column();
			if (col->init(doc, format.substr(matches[0].rm_so, matches[0].rm_eo - matches[0].rm_so), this->aggregate)) {
				this->columns.push_back(col);
			} else {
				delete col;
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

stringVector Configuration::getPartsNames()
{
	return this->parts;
}

std::string Configuration::getFilter()
{
	return this->filter;
}

stringSet Configuration::getAggregateColumns()
{
	stringSet aggregateColumns;
	Column *col;

	/* Open XML configuration file */
	pugi::xml_document doc;
	doc.load_file(this->getXmlConfPath());

	/* go over all aliases */
	for (stringSet::iterator aliasIt = this->aggregateColumnsAliases.begin();
			aliasIt != this->aggregateColumnsAliases.end(); aliasIt++) {

		col = new Column();
		col->init(doc, *aliasIt, this->aggregate);
		stringSet cols = col->getColumns();
		aggregateColumns.insert(cols.begin(), cols.end());
		delete col;
	}

	return aggregateColumns;
}

stringSet Configuration::getSummaryColumns()
{
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

stringVector Configuration::getOrder()
{
	return this->order;
}

bool Configuration::getPlainNumbers()
{
	return this->plainNumbers;
}

size_t Configuration::getMaxRecords()
{
	return this->maxRecords;
}

bool Configuration::getAggregate()
{
	return this->aggregate;
}

bool Configuration::getQuiet()
{
	return this->quiet;
}

columnVector& Configuration::getColumns()
{
	return this->columns;
}

std::string Configuration::version()
{
	std::ifstream ifs;
	ifs.open("VERSION");

	std::string version;
	if (ifs.is_open()) {
		getline (ifs, version);
	}

	return version;
}

char* Configuration::getXmlConfPath()
{
	return (char*) COLUMNS_XML;
}

std::string Configuration::getTimeWindowStart()
{
	return this->timeWindow.substr(0, this->timeWindow.find('-'));
}

std::string Configuration::getTimeWindowEnd()
{
	size_t pos = this->timeWindow.find('-');
	if (pos == std::string::npos) {
		return "";
	}
	return this->timeWindow.substr(pos+1);
}

bool Configuration::isDirectory(std::string dir)
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

void Configuration::help()
{
	/* lines with // at the beginning should be implemented sooner */
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
//	<< "-f              read netflow filter from file" << std::endl
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
	<< "-m              Print netflow data date sorted. Only useful with -M" << std::endl
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

bool Configuration::getOptionm()
{
	return this->optm;
}

Configuration::Configuration(): maxRecords(0), plainNumbers(false), aggregate(false), quiet(false),
		optm(false)
{
	this->resolver = new Resolver();
}

Configuration::~Configuration()
{
	for (columnVector::iterator it = columns.begin(); it != columns.end(); it++) {
		delete *it;
	}
}

} /* end of ipfixdump namespace */
