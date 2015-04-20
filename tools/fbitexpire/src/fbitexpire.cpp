/**
 * \file fbitexpire.cpp
 * \author Michal Kozubik <kozubik@cesnet.cz>
 * \brief Main body of the fbitexpire
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

#include <cstdlib>
#include <chrono>
#include <thread>
#include <condition_variable>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <sys/signal.h>
#include <sys/stat.h>
#include <unistd.h>

#include "Cleaner.h"
#include "config.h"
#include "fbitexpire.h"
#include "PipeListener.h"
#include "Scanner.h"
#include "verbose.h"
#include "Watcher.h"

#define OPTSTRING "rfmhVDkocp:d:s:v:w:"
#define DEFAULT_PIPE "./fbitexpire_fifo"
#define DEFAULT_DEPTH 1

static const char *msg_module = "fbitexpire";

using namespace fbitexpire;

static PipeListener *list = nullptr;

/**
 * \brief Print basic help
 */
void print_help()
{
	std::cout << "Usage: " << PACKAGE_NAME << " [-rhVDokmc] [-p pipe] [-d depth] [-w watermark] [-v level] -s size directory\n\n";
	std::cout << "Options:\n";
	std::cout << "  -h             Show this help and exit\n";
	std::cout << "  -V             Show version and exit\n";
	std::cout << "  -r             Instruct daemon to rescan folder (note: daemon has to be running)\n";
	std::cout << "  -f             Force rescan directories when daemon starts (ignores stat files)\n";
	std::cout << "  -p <pipe>      Pipe name (default: " << DEFAULT_PIPE << ")\n";
	std::cout << "  -s <size>      Maximum size of all directories (in MB)\n";
	std::cout << "  -w <watermark> Lower limit when removing folders (in MB)\n";
	std::cout << "  -d <depth>     Depth of watched directories (default: 1)\n";
	std::cout << "  -D             Daemonize\n";
	std::cout << "  -m             Multiple sources on top level directory. Please check fbitexpire(1) for more information\n";
	std::cout << "  -k             Stop fbitexpire daemon listening on pipe specified by -p\n";
	std::cout << "  -o             Only scan and remove old directories, if needed, and don't wait for new folders\n";
	std::cout << "  -v <level>     Set verbosity level\n";
	std::cout << "  -c             Change daemon settings; to be combined with -s and/or -w\n";
	std::cout << "\n";
}

/**
 * \brief Print tool version
 */
void print_version()
{
	std::cout << PACKAGE_STRING << "\n";
}

/**
 * \brief SIGINT handler
 * 
 * \param param
 */
void handle(int param)
{
	(void) param;

	/* Tell listener to stop */
	if (list) {
		list->killAll();
	}
}

/**
 * \brief Write message to named pipe
 * 
 * \param pipe_exists pipe existence flag
 * \param pipe Pipe name
 * \param msg Message to be written
 * \return 0 if everything is OK
 */
int write_to_pipe(bool pipe_exists, std::string pipe, std::string msg)
{
	if (!pipe_exists) {
		/* pipe does not exist */
		MSG_ERROR(msg_module, "pipe (%s) does not exist", pipe.c_str());
		return 1;
	}

	/* We use fopen here instead of std::ofstream.open because of a
	 * problem with hanging 'open' calls when trying to open pipes.
	 * This problem has been described in 
	 * http://stackoverflow.com/questions/12636485/non-blocking-call-to-ofstreamopen
	 */
	FILE *f_pipe = fopen(pipe.c_str(), "w");
	if (!f_pipe) {
		MSG_ERROR(msg_module, "cannot open pipe %s", pipe.c_str());
		return 1;
	}

	/* write message (and remove terminating '\n') */
	MSG_DEBUG(msg_module, "writing '%s' to pipe", msg.erase(msg.length() - 1).c_str());
	fputs(msg.c_str(), f_pipe);

	/* done */
	fclose(f_pipe);
	return 0;
}

int main(int argc, char *argv[])
{
	int c, depth{DEFAULT_DEPTH};
	bool rescan{false}, daemonize{false}, pipe_exists{false}, pipe_file_exists{false}, pipe_created{false}, multiple{false};
	bool change{false}, force{false}, wmarkset{false}, size_set{false}, kill_daemon{false}, only_remove{false}, depth_set{false};
	uint64_t watermark{0}, size{0};
	std::string pipe{DEFAULT_PIPE};
	
	while ((c = getopt(argc, argv, OPTSTRING)) != -1) {
		switch (c) {
		case 'h':
			print_help();
			return 0;
		case 'V':
			print_version();
			return 0;
		case 'r':
			rescan = true;
			break;
		case 'f':
			force = true;
			break;
		case 'p':
			pipe = std::string(optarg);
			break;
		case 's':
			size_set = true;
			size = Scanner::strToSize(optarg);
			break;
		case 'w':
			wmarkset = true;
			watermark = Scanner::strToSize(optarg);
			break;
		case 'd':
			depth_set = true;
			depth = std::atoi(optarg);
			break;
		case 'D':
			daemonize = true;
			break;
		case 'm':
			multiple = true;
			break;
		case 'k':
			kill_daemon = true;
			break;
		case 'o':
			only_remove = true;
			break;
		case 'v':
			MSG_SET_VERBOSE(std::atoi(optarg));
			break;
		case 'c':
			change = true;
			break;
		default:
			print_help();
			return 1;
		}
	}

	openlog(0, LOG_CONS | LOG_PID, LOG_USER);
	
	if ((daemonize && rescan) || (daemonize && kill_daemon) || (daemonize && only_remove)
			|| (rescan && only_remove) || (kill_daemon && only_remove)) {
		MSG_ERROR(msg_module, "conflicting arguments");
		return 1;
	}

	if (optind >= argc && !kill_daemon && !change) {
		MSG_ERROR(msg_module, "no directory specified");
		std::cout << "\n";
		print_help();
		return 1;
	}
	
	if (!wmarkset) {
		watermark = size;
	}

	/* does pipe exist? */
	struct stat st;
	pipe_file_exists = (lstat(pipe.c_str(), &st) == 0);
	pipe_exists = (pipe_file_exists && S_ISFIFO(st.st_mode));

	/* When starting fbitexpire, we can identify two situations:
	 *      1. Entirely new instance > pipe should not exist
	 *      2. Second or more instance, used to change parameters
	 *         of or send command to first instance > pipe should exist
	 */
	if (rescan || kill_daemon || change) {
		if (!pipe_exists) {
			MSG_ERROR(msg_module, "no existing pipe/daemon found (%s) for changing parameters", pipe.c_str());
			return 1;
		}
	} else if (pipe_file_exists) {
		if (pipe_exists) {
			MSG_ERROR(msg_module, "active pipe (%s) detected", pipe.c_str());
			MSG_ERROR(msg_module, "fbitexpire supports only a single instance per pipe");
			MSG_NOTICE(msg_module, "please restart using different pipe (-p)");
			return 1;
		} else if (remove(pipe.c_str()) != 0) {
			// Remove invalid pipe
			MSG_ERROR(msg_module, "could not delete pipe");
		}
	}

	std::stringstream msg;
	if (rescan) {
		while (optind < argc) {
			msg << "r" << argv[optind++] << "\n";
		}
	}
	if (kill_daemon) {
		msg << "k\n";
	}
	if (change) {
		if (!size_set && !wmarkset) {
			MSG_WARNING(msg_module, "nothing to be changed by -c");
			return 1;
		}
		if (size_set) {
			msg << "s" << size << "\n";
		}
		if (wmarkset) {
			msg << "w" << watermark << "\n";
		}
	}

	/* Send command to rescan folder or to kill daemon */
	if (rescan || kill_daemon || change) {
		return write_to_pipe(pipe_exists, pipe, msg.str());
	}

	if (!size_set) {
		MSG_ERROR(msg_module, "size (-s) not specified");
		std::cout << "\n";
		print_help();
		return 1;
	}
	
	if (!pipe_exists) {
		/* Create pipe */
		if (mkfifo(pipe.c_str(), S_IRWXU | S_IRWXG | S_IRWXO)) {
			MSG_ERROR(msg_module, "%s", strerror(errno));
			return 1;
		}

		pipe_created = true;
	}
	
	if (!depth_set) {
		MSG_NOTICE(msg_module, "depth not set; using default (%d)", DEFAULT_DEPTH);
	}
	
	std::string basedir{argv[optind]};
	basedir = Directory::correctDirName(basedir);
	if (basedir.empty()) {
		if (pipe_created) {
			remove(pipe.c_str());
		}

		return 1;
	}
	
	if (daemonize) {
		closelog();
		MSG_SYSLOG_INIT(PACKAGE);
		MSG_NOTICE(msg_module, "daemonizing...");
		
		/* and send all following messages to the syslog */
		if (daemon (1, 0)) {
			MSG_ERROR(msg_module, strerror(errno));
		}
	} 
	
	Watcher watcher;
	Cleaner cleaner;
	Scanner scanner;
	PipeListener listener(pipe);
	list = &listener;

	std::mutex mtx;
	std::unique_lock<std::mutex> lock(mtx);
	std::condition_variable cv;
	
	try {
		scanner.createDirTree(basedir, depth, force);
		watcher.run(&scanner, multiple);
		scanner.run(&cleaner, size, watermark, multiple);
		cleaner.run();
		listener.run(&watcher, &scanner, &cleaner, &cv);
		signal(SIGINT, handle);
	} catch (InotifyException &e) {
		MSG_ERROR(msg_module, e.GetMessage().c_str());
		return 1;
	} catch (std::exception &e) {
		MSG_ERROR(msg_module, e.what());
		return 1;
	}
	
	if (only_remove) {
		/*
		 * tell PipeListener to stop other threads
		 */
		sleep(1);
		listener.killAll();
		listener.stop();
		return 0;
	}

	cv.wait(lock, [&listener]{ return listener.isDone(); });
	closelog();
}
