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


int configuration::init(int argc, char *argv[]) {
	char c;
	DIR *d;
	struct dirent *dent;

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
			break;
		case 'A': /* aggregate on specific columns */
				aggregate = true;
				aggregateColumns = optarg;
			break;
		case 'r': /* file to open */
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
			NOT_SUPPORTED
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
		case 'R':
			NOT_SUPPORTED
			break;
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

	/* set default select clause - for statistics TODO*/
	/* e0id8, e0id12 ipv4 addresses */
	/* e0id27[p0,p1], e0id28[p0,p1] ipv6 addresses */
	select = "e0id152, e0id153, e0id4, e0id7, e0id11, e0id2, e0id8, e0id12";

	/* set default order (by timestamp) */
	order.push_back("e0id152");

	/* set format according to input */
	/* TODO add other options and custom format*/
	if (format == "all") {
		format.clear();
	} else if (format.length() == 0 || format == "line") {
		format = "e0id152  e0id153 e0id4 (e0id8 e0id27p0e0id27p1):e0id7l -> (e0id12 e0id28p0e0id28p1):(e0id11 e0id32 e0id139 e0id33)l e0id2 e0id1";
	} else if (format == "long") {
		format = "e0id152  e0id153 e0id4 (e0id8 e0id27p0e0id27p1):e0id7l -> (e0id12 e0id28p0e0id28p1):(e0id11 e0id32 e0id139 e0id33)l e0id6 e0id5 e0id2 e0id1";
	} else if (format == "extended") {
		format = "e0id152  e0id153 e0id4 (e0id8 e0id27p0e0id27p1):e0id7l -> (e0id12 e0id28p0e0id28p1):(e0id11 e0id32 e0id139 e0id33)l e0id6 e0id5 e0id2 e0id1";
	} else if (format == "pipe") {
		format = "e0id152|e0id153|e0id4|(e0id8 e0id27p0e0id27p1)|e0id7|(e0id12 e0id28p0e0id28p1)|(e0id11 e0id32 e0id139 e0id33)|e0id2|e0id1";
	} else if (format == "csv") {
		format = "e0id152,e0id153,e0id4,(e0id8 e0id27p0e0id27p1),e0id7,(e0id12 e0id28p0e0id28p1),(e0id11 e0id32 e0id139 e0id33),e0id2,e0id1";
	}

	/* check validity of given values */
	if (tables.size() < 1) {
		/* TODO read from stdin */
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

void configuration::help() {
	std::cout
	<< "usage "<< progname <<" [options] [\"filter\"]" << std::endl
	<< "-h              this text you see right here" << std::endl
	<< "-V              Print version and exit." << std::endl
	<< "-a              Aggregate netflow data." << std::endl
	<< "-A <expr>[/net] How to aggregate: ',' sep list of tags see nfdump(1)" << std::endl
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
}

configuration::configuration(): maxRecords(0), plainNumbers(false), aggregate(false) {}

}
