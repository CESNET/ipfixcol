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

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/types.h>

static const char *msg_module = "PipeListener";

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
	_cv = cv;
	run();
}

/**
 * \brief Main loop
 */
void PipeListener::loop()
{
	prctl(PR_SET_NAME, "fbitexp:PipeList\0", 0, 0, 0);
	
	MSG_DEBUG(msg_module, "started");
	
	while (!_done) {
		reopenPipe();
		while (std::getline(_pipe, _buff)) {
			MSG_DEBUG(msg_module, "read '%s'", _buff.c_str());
			if (!_buff.empty()) {
				switch(_buff[0]) {
				case 'r':
					/* rescan folder */
					MSG_NOTICE(msg_module, "triggered rescan of %s", _buff.substr(1).c_str());
					_buff = _buff.substr(1);
					_scanner->rescan(_buff);
					break;
				case 'k':
					/* Stop daemon */
					MSG_NOTICE(msg_module, "triggered daemon termination");
					_done = true;
					break;
				case 's':
					MSG_NOTICE(msg_module, "setting max. directory size (%s)", _buff.substr(1).c_str());
					_scanner->setMaxSize(_buff.substr(1), true);
					break;
				case 'w':
					MSG_NOTICE(msg_module, "setting lower limit (%s)", _buff.substr(1).c_str());
					_scanner->setWatermark(_buff.substr(1));
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

/**
 * \brief Kill all threads (send "k" into pipe)
 */
void PipeListener::killAll()
{
	_done = true;
	std::ofstream fpipe(_pipename, std::ios::out);
	fpipe << "k\n";
	fpipe.close();
	removePipe();
}

/**
 * \brief Stop all threads
 */
void PipeListener::stopAll()
{
	removePipe();
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

/**
 * \brief Remove the pipe from the file system
 */
void PipeListener::removePipe()
{
	// We don't have to check for file existance here, since
	// existance is implicit from the use of the pipe before.
	if (access(_pipename.c_str(), F_OK) != -1 && remove(_pipename.c_str()) != 0) {
		MSG_ERROR(msg_module, "could not delete pipe");
	}
}

/**
 * \brief Constructor
 */
PipeListener::PipeListener(std::string pipename):
		_watcher(NULL), _scanner(NULL), _cleaner(NULL), _pipename(pipename), _cv(NULL)
{

}

} /* end of namespace fbitexpire */
