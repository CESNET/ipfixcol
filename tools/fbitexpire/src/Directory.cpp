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
#include <limits.h>
#include <stdlib.h>

#include <stdexcept>

static const char *msg_module = "Directory";

namespace fbitexpire {

Directory::~Directory()
{
	for (auto child: _children) {
		delete child;
	}
	
	_children.clear();
}

std::string Directory::correctDirName(std::string dir)
{
	char *real = realpath(dir.c_str(), nullptr);
	
	if (!real) {
		MSG_ERROR(msg_module, "wrong path %s", dir.c_str());
		return std::string("");
	}
	
	std::string realdir(real);
	free(real);
	
	return realdir;
}

void Directory::rescan()
{
	if (_children.empty()) {
		setSize(dirSize(_name));
		return;
	}

	uint64_t size = dirSize(_name, false);
	
	for (auto child: _children) {
		child->rescan();
		size += child->getSize();
	}
	
	setSize(size);
}

void Directory::removeOldest()
{
	_children.erase(_children.begin());
}

void Directory::detectAge()
{
	struct stat st;
	lstat(_name.c_str(), &st);
	
	setAge(st.st_mtime);
}

/**
 * \brief Create directory tree
 * 
 * \param parent Actual directory
 * \param maxdepth Maximal depth
 */
uint64_t Directory::dirSize(std::string path, bool recursive)
{
	std::string entry_path, entry_name;
	struct dirent *entry;
	struct stat st;
	uint64_t size{0};
	
	DIR *dir = opendir(path.c_str());
	if (!dir) {
		throw std::invalid_argument(std::string("Cannot open " + path));
	}
	
	lstat(path.c_str(), &st);
	size += st.st_size;
	
	/* Iterate through subdirectories */
	while ((entry = readdir(dir))) {
		/* Get entry name and full path */
		entry_name = entry->d_name;
		entry_path = path + '/' + entry_name;
		
		if (entry_name == "." || entry_name == ".." || lstat(entry_path.c_str(), &st)) {
			continue;
		} else if (S_ISDIR(st.st_mode)) {
			if (recursive) {
				size += dirSize(entry_path);
			}
		} else {
			size += st.st_size;
		}
	}
	closedir(dir);
	
	return size;
}


} /* end of namespace fbitexpire */