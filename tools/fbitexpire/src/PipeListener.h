/**
 * \file PipeListener.h
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

#ifndef PIPELISTENER_H
#define	PIPELISTENER_H

#include "fbitexpire.h"

#include "Cleaner.h"
#include "Scanner.h"
#include "Watcher.h"

#include "verbose.h"

#include <condition_variable>
#include <fstream>

namespace fbitexpire {

/**
 * \brief PipeListener - reads pipe and decodes rescan/kill commands
 */
class PipeListener : public FbitexpireThread {
	using FbitexpireThread::run;
public:
	PipeListener(std::string pipename);
	~PipeListener() { closePipe(); }
	
	void run(Watcher *watcher, Scanner *scanner, Cleaner *cleaner, std::condition_variable *cv);
	void killAll();
private:
	void loop();
	void openPipe();
	void closePipe();
	void reopenPipe();
	void removePipe();
	void stopAll();
	
	Watcher *_watcher;              /**< Watcher's instance */
	Scanner *_scanner;              /**< Scanner's instance */
	Cleaner *_cleaner;              /**< Cleaner's instance */
	std::string   _buff;            /**< buffer for reading messages from pipe */
	std::string   _pipename;        /**< pipe name */
	std::ifstream _pipe;            /**< pipe file */
	std::condition_variable *_cv;   /**< cond. var indicating end of PipeListener's thread */
};

} /* end of namespace fbitexpire */

#endif	/* PIPELISTENER_H */

