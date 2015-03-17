/**
 * \file Watcher.h
 * \author Michal Kozubik <kozubik@cesnet.cz>
 * \brief Watcher class for fbitexpire tool
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

#ifndef WATCHER_H_
#define WATCHER_H_

#include "fbitexpire.h"
#include "Directory.h"
#include "Scanner.h"
#include "inotify-cxx/inotify-cxx.h"
#include "verbose.h"

#include <iostream>
#include <thread>
#include <algorithm>
#include <condition_variable>

namespace fbitexpire {

/**
 * \brief Subtree root - used when there are multiple data writers
 */
struct RootWatch {
	RootWatch() {}
	RootWatch(Directory *r): root(r) {}
	Directory *root = nullptr;          /* root of watched subtree */
	std::vector<Directory *> watching;   /* vector of watched directories in this subtree */
};

/**
 * \brief Main watcher class for processing inotify events
 */
class Watcher : public FbitexpireThread {
	using FbitexpireThread::run;
public:
	Watcher();
	~Watcher();

	void run(Scanner *scanner, bool multiple);
	void stop();
private:
	void loop();
	void setup();
	void processNewDir(InotifyEvent& event);
	void watch(RootWatch *rw, Directory *dir);
	void watchRootWatch(RootWatch *rw);
	void unWatch(Directory *dir);
	void unWatchLast(RootWatch *rw);
	RootWatch *getRoot(Directory *dir);
	
	Inotify  _inotify;          /**< inotify instance */
	Scanner *_scanner;          /**< scanner's instance */
	
	int  _max_depth;            /**< maximal depth */
	int  _root_name_len;        /**< length of root's name */
	bool _multiple;             /**< multiple data writers flag */
	
	std::vector<RootWatch *> _roots;  /**< subRoots */

};

} /* end of namespace fbitexpire */

#endif /* WATCHER_H_ */
