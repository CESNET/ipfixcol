/**
 * \file PipeListener.cpp
 * \author Michal Kozubik <kozubik@cesnet.cz>
 * \brief PipeListener class for fbitexpire tool
 *
 * Copyright (C) 2014 CESNET, z.s.p.o.
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

#include "PipeListener.h"
#include "verbose.h"

#include <csignal>

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/types.h>


static const char *msg_module = "PipeListener";

static std::string glob_pipename;

namespace fbitexpire {

/**
 * \brief Run PipeListener's thread
 * 
 * \param watcher Watcher's instance
 * \param scanner Scanner's instance
 * \param cleaner Cleaner's instance
 * \param cv Variable which notifies fbitexpire that job is done
 */
void PipeListener::run(Watcher *watcher, Scanner *scanner, Cleaner *cleaner, std::condition_variable *cv)
{
	_watcher = watcher;
	_scanner = scanner;
	_cleaner = cleaner;
	glob_pipename = _pipename;
	_cv = cv;
	run();
}

/**
 * \brief Main loop
 */
void PipeListener::loop()
{
	prctl(PR_SET_NAME, "fbitexp:PipeList\0", 0, 0, 0);
	signal(SIGINT, PipeListener::handle);
	
	MSG_DEBUG(msg_module, "started");
	
	while (!_done) {
		reopenPipe();
		while (std::getline(_pipe, _buff)) {
			MSG_DEBUG(msg_module, "readed %s", _buff.c_str());
			if (!_buff.empty()) {
				switch(_buff[0]) {
				case 'r':
					/* rescan folder */
					_buff = _buff.substr(1);
					_scanner->rescan(_buff);
					break;
				case 'k':
					/* Stop daemon */
					_done = true;
					break;
				}
			}
			
			if (_done) {
				break;
			}
		}
	}
	
	stopAll();
	MSG_DEBUG(msg_module, "closing thread");
	_cv->notify_one();
}

void PipeListener::killAll()
{
	_done = true;
	if (_th.joinable()) {
		pthread_kill(_th.native_handle(), SIGINT);
	}
}

/**
 * \brief Stop all threads
 */
void PipeListener::stopAll()
{
	_watcher->stop();
	_scanner->stop();
	_cleaner->stop();
	_done = true;
}

/**
 * \brief Open pipe for reading
 */
void PipeListener::openPipe()
{
	_pipe.open(_pipename, std::ios::in);
}

/**
 * \brief Close pipe
 */
void PipeListener::closePipe()
{
	if (_pipe.is_open()) {
		_pipe.close();
	}	
}

/**
 * \brief Reopen pipe to read again
 */
void PipeListener::reopenPipe()
{
	closePipe();
	openPipe();
}

void PipeListener::handle(int param)
{
	int fd = open(glob_pipename.c_str(), O_WRONLY | O_NONBLOCK);
	if (fd < 0) {
		MSG_WARNING(msg_module, "%s", strerror(errno));
	} else {
		std::string msg = "k\n";
		write(fd, msg.c_str(), msg.length());
		close(fd);
	}
}

} /* end of namespace fbitexpire */
