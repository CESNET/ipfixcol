/**
 * \file Cleaner.cpp
 * \author Michal Kozubik <kozubik@cesnet.cz>
 * \brief Cleaner class for fbitexpire tool
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

#include "Cleaner.h"
#include "verbose.h"

#include <stdexcept>

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/prctl.h>

static const char *msg_module = "Cleaner";

namespace fbitexpire {

/**
 * \brief Stop cleaner thread
 */
void Cleaner::stop()
{
	_done = true;
	_cv.notify_one();
	
	if (_th.joinable()) {
		_th.join();
	}
}

/**
 * \brief Main loop
 */
void Cleaner::loop()
{	
	prctl(PR_SET_NAME, "fbitexp:Cleaner\0", 0, 0, 0);
	
	MSG_DEBUG(msg_module, "started");
	
	std::mutex mtx;
	std::unique_lock<std::mutex> lock{mtx};
	
	while (!_done) {
		/* Wait for some event */
		_cv.wait(lock, [&]{ return count() > 0 || _done; });
		
		if (_done) {
			break;
		}
		
		/* Remove everything in queue */
		while (count() > 0) {
			try {
				remove(getNextDir());
			} catch (std::exception &e) {
				MSG_ERROR(msg_module, e.what());
			}
		}
	}
	
	MSG_DEBUG(msg_module, "closing thread");
}

/**
 * \brief Remove directory recursively
 */
void Cleaner::remove(std::string path)
{
	struct stat st;
	struct dirent *entry;
	std::string entry_name, entry_path;
	
	/* open directory */
	DIR *dir = opendir(path.c_str());
	
	if (!dir) {
		throw std::invalid_argument(std::string("cannot open directory " + path));
	}
	
	while ((entry = readdir(dir))) {
		entry_name = entry->d_name;
		if (entry_name == "." || entry_name == "..") {
			continue;
		}
		
		/* get full entry path */
		entry_path = path + "/" + entry_name;
		if (!stat(entry_path.c_str(), &st) && S_ISDIR(st.st_mode)) {
			/* remove subdirectory */
			remove(entry_path);
		} else {
			/* remove file */
			unlink(entry_path.c_str());
		}
	}

	/* close and remove directory */
	closedir(dir);
	rmdir(path.c_str());
}

/**
 * \brief Remove directory - add it to queue
 */
void Cleaner::removeDir(std::string path)
{
	/* Lock queue - lock guard releases mutex when out of scope */
	std::lock_guard<std::mutex> lock(_dirs_lock);
	
	_dirs.push(path);
	_count++;
	_cv.notify_one();
}

/**
 * \brief Get directory to remove
 */
std::string Cleaner::getNextDir()
{
	/* Lock queue */
	std::lock_guard<std::mutex> lock(_dirs_lock);
	std::string path = _dirs.front();
	
	_dirs.pop();
	_count--;
	
	return path;
}

} /* end of namespace fbitexpire */
