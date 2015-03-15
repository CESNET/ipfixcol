/**
 * \file Directory.cpp
 * \author Michal Kozubik <kozubik@cesnet.cz>
 * \brief Directory class for fbitexpire tool
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

#include "Directory.h"
#include "verbose.h"

#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include <stdexcept>
#include <fstream>

static const char *msg_module = "Directory";

namespace fbitexpire {

/**
 * \brief Destructor - remove all children
 */
Directory::~Directory()
{
	for (auto child: _children) {
		delete child;
	}
	
	_children.clear();
}

/**
 * \brief Update directory's age
 */
void Directory::updateAge()
{
	if (!_children.empty()) { 
		_age = _children.front()->getAge(); 
	} else {
		struct stat st;

		if (lstat(_name.c_str(), &st) == -1) {
			MSG_ERROR(msg_module, "Could not determine status of '%s' (%s)", _name.c_str(), strerror(errno));
		}

		_age = st.st_mtime;
	} 
}

/**
 * \brief Get directory name in right format (absolute path)
 *				Also tests if directory exists
 * \param dir Directory path
 * \return Absolute path or empty string
 */
std::string Directory::correctDirName(std::string dir)
{
	char *real = realpath(dir.c_str(), nullptr);
	
	if (!real) {
		MSG_ERROR(msg_module, "directory does not exist: %s", dir.c_str());
		return std::string("");
	}
	
	std::string realdir(real);
	free(real);
	
	return realdir;
}

/**
 * \brief Rescan directory - compute size of all files + subdirs
 */
void Directory::rescan()
{
	if (_children.empty()) {
		setSize(dirSize(_name, true, true));
		return;
	}

	/* Size of files */
	uint64_t size = dirSize(_name, true, false);
	
	/* Size of children */
	for (auto child: _children) {
		child->rescan();
		size += child->getSize();
	}
	
	setSize(size);
}

/**
 * \brief Remove the oldest child from vector
 */
void Directory::removeOldest()
{
	_children.erase(_children.begin());
}

/**
 * \brief Get right directory age (time last modified)
 */
void Directory::detectAge()
{
	struct stat st;

	if (lstat(_name.c_str(), &st) == -1) {
		MSG_ERROR(msg_module, "Could not determine status of '%s' (%s)", _name.c_str(), strerror(errno));
	}
	
	setAge(st.st_mtime);
}

/**
 * \brief Compute directory size
 * 
 * \param path Directory path
 * \param recursive If true, recursively compute size of subfolders
 * \return Directory size in bytes
 */
uint64_t Directory::dirSize(std::string path, bool force, bool recursive, bool writestats)
{
	std::string entry_path, entry_name;
	struct dirent *entry;
	struct stat st;
	uint64_t size{0};
	
	MSG_DEBUG(msg_module, "scanning %s", path.c_str());     
	
	std::string statsfile(path + "/stats.txt");
	if (!force && stat(statsfile.c_str(), &st)) {
		/* stats.txt not exists - force scan*/
		force = true;
	}
	
	if (!force) {
		MSG_DEBUG(msg_module, "reading %s", statsfile.c_str());
		std::ifstream sfile(statsfile, std::ios::in);
		sfile >> size;
		sfile.close();
		return size;
	}
	
	DIR *dir = opendir(path.c_str());
	if (!dir) {
		throw std::invalid_argument(std::string("Cannot open " + path));
	}
	
	/* Size of "." */
	if (lstat(path.c_str(), &st) == -1) {
		MSG_ERROR(msg_module, "Could not determine status of '%s' (%s)", path.c_str(), strerror(errno));
	}
	size += st.st_size;
	
	/* Iterate through files and subdirectories */
	while ((entry = readdir(dir))) {
		/* Get entry name and full path */
		entry_name = entry->d_name;
		entry_path = path + '/' + entry_name;
		
		if (entry_name == "." || entry_name == ".." || lstat(entry_path.c_str(), &st)) {
			continue;
		} else if (S_ISDIR(st.st_mode)) {
			if (recursive) {
				size += dirSize(entry_path, force, recursive, false);
			}
		} else {
			size += st.st_size;
		}
	}
	closedir(dir);
	
	
	if (writestats) {
		MSG_DEBUG(msg_module, "writing %s", statsfile.c_str());
		std::ofstream sfile(statsfile, std::ios::out);
		sfile << size;
		sfile.close();
	}
	
	return size;
}


} /* end of namespace fbitexpire */