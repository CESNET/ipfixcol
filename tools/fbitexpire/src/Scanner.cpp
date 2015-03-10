/**
 * \file Scanner.cpp
 * \author Michal Kozubik <kozubik@cesnet.cz>
 * \brief Scanner class for fbitexpire tool
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

#include "fbitexpire.h"
#include "Scanner.h"
#include "verbose.h"

#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdexcept>
#include <sys/prctl.h>
#include <string.h>

#include <iomanip>
#include <mutex>
#include <sstream>

static const char *msg_module = "Scanner";

namespace fbitexpire {

/**
 * \brief Run Scanner's thread
 * 
 * \param cleaner Cleaner's instance
 * \param max_size Maximal size
 * \param watermark Watermark - lower limit when removing data
 * \param multiple True when multiple data writers allowed
 */
void Scanner::run(Cleaner *cleaner, uint64_t max_size, uint64_t watermark, bool multiple)
{
	_cleaner   = cleaner;
	_max_size  = max_size; 
	_watermark = (watermark <= max_size) ? watermark : max_size;
	_multiple  = multiple;
	run();
}

/**
 * \brief Stop scanner's thread
 */
void Scanner::stop()
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
void Scanner::loop()
{
	prctl(PR_SET_NAME, "fbitexp:Scanner\0", 0, 0, 0);
	
	std::mutex mtx;
	std::unique_lock<std::mutex> lock(mtx);
	
	MSG_DEBUG(msg_module, "started");
	
	while (!_done) {
		/* 
		 * This is before waiting because we want to check size on startup before 
		 * any scan or add requests
		 */
		if (totalSize() > _max_size) {
			removeDirs();
		}
		
		MSG_DEBUG(msg_module, "Total size: %s, Max: %s, Watermark: %s", 
			sizeToStr(totalSize()).c_str(), sizeToStr(_max_size).c_str(), sizeToStr(_watermark).c_str());
		_cv.wait(lock, [&]{ return scanCount() > 0 || addCount() > 0 || _done || totalSize() > _max_size; });
		
		if (_done) {
			break;
		}
		
		/* Add dirs from queue */
		if (addCount() > 0) {
			addNewDirs();
		}
		
		/* Scan dirs in queue */
		if (scanCount() > 0) {
			rescanDirs();
		}
	}
	
	MSG_DEBUG(msg_module, "closing thread");
}

/**
 * \brief Get oldest directory of given root
 * 
 * \param root Directory tree root
 * \return The oldest directory in tree
 */
Directory *Scanner::getOldestDir(Directory *root)
{
	while (!root->getChildren().empty()) {
		root = root->getOldestChild();
	}
	
	return root;
}

/**
 * \brief Get directory that can be removed
 *			== oldest directory in (sub)tree
 * \return Directory to remove
 */
Directory *Scanner::getDirToRemove()
{
	Directory *dir;
	
	if (!_multiple) {
		/* Only one root directory - good for us */
		dir = getOldestDir(_rootdir);
		if (!dir->isActive()) {
			return dir;
		}
		
		return nullptr;
	}
	
	/* If directory from first subtree cannot be remoevd (e.g. it is active), try next subtree */
	for (auto subRoot: _rootdir->getChildren()) {
		dir = getOldestDir(subRoot);
		if (!dir->isActive()) {
			return dir;
		}
	}
	
	return nullptr;
}

/**
 * \brief Remove directories until total size > watermark
 */
void Scanner::removeDirs()
{
	Directory *dir, *parent;
	
	while (totalSize() > _watermark) {
		dir = getDirToRemove();
		
		if (!dir) {
			MSG_WARNING(msg_module, "cannot remove any folder (only active directories)");
			return;
		}
		
		MSG_DEBUG(msg_module, "remove %s", dir->getName().c_str());
		_cleaner->removeDir(dir->getName());
		
		/* Remove dir from its parent */
		parent = dir->getParent();
		parent->removeOldest();
		
		/* Update parent's age to the second oldest subdir and correct it's size */
		while (parent) {
			parent->updateAge();
			parent->setSize(parent->getSize() - dir->getSize());
			parent = parent->getParent();
		}
		
		delete dir;
		
		if (_multiple) {
			/* Multiple data witers - sort top dirs so next time we remove the oldest dir again */
			_rootdir->sortChildren();
		}
	}
}

/**
 * \brief Rescan directories
 */
void Scanner::rescanDirs()
{
	Directory *dir;
	std::string path;
	
	while (scanCount() > 0) {
		/* Get directory instance from path */
		path = Directory::correctDirName(getNextScan());
		if (path.empty()) {
			continue;
		}

		dir = dirFromPath(path);
		if (!dir) {
			/* Directory not found in our tree */
			MSG_WARNING(msg_module, "Cannot rescan %s, it's not part of this tree of it's too deep", path.c_str());
			continue;
		}

		/* rescan directory */
		dir->rescan();
	}
}

/**
 * \brief Add new directories
 */
void Scanner::addNewDirs()
{
	Directory *dir, *parent;
	uint64_t newSize;
	
	while (addCount() > 0) {
		std::tie(dir, parent) = getNextAdd();
		MSG_DEBUG(msg_module, "Adding %s", dir->getName().c_str());
		parent->addChild(dir);
		
		/* 
		 * If directory is NOT active, it means that it is final tree node (leaf)
		 * and we can count it's size.
		 */
		if (dir->isActive()) {
			continue;
		}
		
		/* Set directory age */
		dir->detectAge();
		
		/* Get directory size */
		dir->setSize(dir->countSize());
		
		newSize = dir->getSize();
		
		/* Propagate size to predecessors */
		while (parent) {
			if (!parent->isActive()) {
				/* If parent is inactive, we can count size of files in it */
				newSize += parent->countFilesSize();
			}
			parent->setSize(parent->getSize() + newSize);
			parent = parent->getParent();
		}
	}
}

/**
 * \brief Remove (but not delete) the newest child from parent's vector
 * 
 * \param parent Parent directory
 */
void Scanner::popNewestChild(Directory *parent)
{
	Directory *dir = parent->getChildren().back();
	parent->getChildren().pop_back();
	
	/* Decrease size of each predecessor */
	while (parent) {
		parent->setSize(parent->getSize() - dir->getSize());
		parent = parent->getParent();
	}
}

/**
 * \brief Add request to add new directory
 * 
 * \param dir New directory
 * \param parent Directory's parent
 */
void Scanner::addDir(Directory* dir, Directory* parent)
{
	std::lock_guard<std::mutex> lock(_add_lock);
	
	_to_add.push(std::make_pair(dir, parent));
	_add_count++;
	_cv.notify_one();
}

/**
 * \brief Get next directory pair from queue
 * 
 * \return Pair - new directory and it's parent
 */
Scanner::addPair Scanner::getNextAdd()
{
	std::lock_guard<std::mutex> lock(_add_lock);
	
	addPair pair = _to_add.front();
	_to_add.pop();
	_add_count--;
	
	return pair;
}

/**
 * \brief Add request to rescan directory
 * 
 * \param dir Directory path
 */
void Scanner::rescan(std::string dir)
{
	std::lock_guard<std::mutex> lock(_scan_lock);
	
	_to_scan.push(dir);
	_scan_count++;
	_cv.notify_one();
}

/**
 * \brief Get next directory from scanning queue
 * 
 * \return Directory path
 */
std::string Scanner::getNextScan()
{
	std::lock_guard<std::mutex> lock(_scan_lock);
	
	std::string dir = _to_scan.front();
	_to_scan.pop();
	_scan_count--;
	
	return dir;
}

/**
 * \brief Convert size to string in appropriate units
 * 
 * \param size Numeric size
 * \return String representing size
 */
std::string Scanner::sizeToStr(uint64_t size)
{
	std::stringstream ss;
	ss << std::fixed << std::setprecision(2);
	if (size < KILOBYTE) {
		ss << size << " B";
	} else if (size < MEGABYTE) {
		ss << BYTES_TO_KB(size) << " KB";
	} else if (size < GIGABYTE) {
		ss << BYTES_TO_MB(size) << " MB";
	} else {
		ss << BYTES_TO_GB(size) << " GB";
	}
	
	return ss.str();
}

/**
 * \brief Convert string to size
 * 
 * \param arg Size in string form
 * \return  size
 */
uint64_t Scanner::strToSize(char *arg)
{
	uint64_t size = strtoull(arg, nullptr, 10);
	
	switch (arg[strlen(arg) - 1]) {
	case 'b':
	case 'B':
		return size;
	case 'k':
	case 'K':
		return KB_TO_BYTES(size);
		break;
	case 'm':
	case 'M':
		return MB_TO_BYTES(size);
		break;
	case 'g':
	case 'G':
		return GB_TO_BYTES(size);
		break;
	default:
		return MB_TO_BYTES(size);
		break;
	}
}

/**
 * \brief Convert path to directory class
 * 
 * \param path Directory path
 * \return instance of directory class from dirtree
 */
Directory *Scanner::dirFromPath(std::string path)
{
	Directory *aux_dir = _rootdir;
	bool next = false;
	
	while (!aux_dir->getChildren().empty()) {
		next = false;
		for (auto child: aux_dir->getChildren()) {
			if (path.find(child->getName()) == 0) {
				aux_dir = child;
				next = true;
				break;
			}
		}
		
		if (!next) {
			break;
		}
	}
	
	if (aux_dir->getName() != path) {
		/* Path is not from this root or is too deep */
		return nullptr;
	}
	
	return aux_dir;
}

/**
 * \brief Create directory tree
 * 
 * \param basedir Root directory
 * \param maxdepth maximal depth
 */
void Scanner::createDirTree(std::string basedir, int maxdepth, bool force)
{
	struct stat st;
	if (!lstat(basedir.c_str(), &st) && S_ISDIR(st.st_mode)) {
		/* Create root directory */
		_rootdir = new Directory(basedir, st.st_mtime, Directory::dirDepth(basedir));
		
		/* set right max depth */
		_max_depth = maxdepth + _rootdir->getDepth();
		_force = force;
		
		/* Add subdirectories (recursively) */
		createDirTree(_rootdir);
	} else {
		throw std::invalid_argument(std::string("Cannot acces directory " + basedir));
	}
}

/**
 * \brief Create directory tree
 * 
 * \param parent Actual directory
 */
void Scanner::createDirTree(Directory* parent)
{
	int depth = parent->getDepth() + 1;
	if (depth >= _max_depth) {
		parent->setSize(Directory::dirSize(parent->getName(), _force));
		parent->detectAge();
		return;
	}
	
	MSG_DEBUG(msg_module, "scanning %s", parent->getName().c_str());
	Directory *aux_dir;
	std::string entry_path, entry_name;
	struct dirent *entry;
	struct stat st;
	uint64_t size = 0;
	
	DIR *dir = opendir(parent->getName().c_str());
	if (!dir) {
		throw std::invalid_argument(std::string("Cannot open " + parent->getName()));
	}
	
	if (lstat(parent->getName().c_str(), &st) == -1) {
		MSG_ERROR(msg_module, "Could not determine status of '%s' (%s)", parent->getName().c_str(), strerror(errno));
	}
	size += st.st_size;
	
	/* Iterate through subdirectories */
	while ((entry = readdir(dir))) {
		/* Get entry name and full path */
		entry_name = entry->d_name;
		entry_path = parent->getName() + '/' + entry_name;
		
		if (entry_name == "." || entry_name == ".." || lstat(entry_path.c_str(), &st)) {
			continue;
		} else if (S_ISDIR(st.st_mode)) {
			aux_dir = new Directory(entry_path, st.st_mtime, depth, parent);
			parent->addChild(aux_dir);
		} else {
			size += st.st_size;
		}
	}
	closedir(dir);
	
	/* Create subtrees */
	for (auto child: parent->getChildren()) {
		createDirTree(child);
		size += child->getSize();
	}
	
	/* Sort children so the oldest will always be on index 0 */
	if (!parent->getChildren().empty()) {
		parent->sortChildren();
	}
	
	parent->updateAge();
	parent->setSize(size);
}

/**
 * \brief Constructor
 */
Scanner::Scanner():
		_cleaner(NULL), _rootdir(NULL), _max_depth(0), _multiple(false), _force(false), _max_size(0), _watermark(0)
{

}

/**
 * \brief Destructor - delete directory tree
 */
Scanner::~Scanner()
{
	if (_rootdir) {
		delete _rootdir;
	}
}

} /* end of namespace fbitexpire */