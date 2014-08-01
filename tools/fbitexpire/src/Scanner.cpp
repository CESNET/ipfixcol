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

#include "Scanner.h"
#include "verbose.h"

#include <dirent.h>
#include <sys/stat.h>
#include <stdexcept>
#include <sys/prctl.h>

#include <mutex>

static const char *msg_module = "Scanner";

namespace fbitexpire {

/**
 * \brief Run Scanner's thread
 * 
 * \param cleaner Cleaner's instance
 * \param max_size Maximal size
 * \param watermark Watermark - lower limit when removing data
 */
void Scanner::run(Cleaner *cleaner, uint64_t max_size, uint64_t watermark, bool multiple)
{
	_cleaner   = cleaner;
	_max_size  = max_size; 
	_watermark = watermark;
	_multiple  = multiple;
	run();
}

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
		
		_cv.wait(lock, [&]{ return scanCount() > 0 || addCount() > 0 || _done; });
		
		if (_done) {
			break;
		}
		
		if (addCount() > 0) {
			addNewDirs();
		}
		
		if (scanCount() > 0) {
			rescanDirs();
		}
	}
	
	MSG_DEBUG(msg_module, "closing thread");
}

Directory *Scanner::getOldestDir(Directory *root)
{
	while (!root->getChildren().empty()) {
		root = root->getOldestChild();
	}
	
	return root;
}

Directory *Scanner::getDirToRemove()
{
	Directory *dir;
	
	if (!_multiple) {
		dir = getOldestDir(_rootdir);
		if (!dir->isActive()) {
			return dir;
		}
		
		return nullptr;
	}
	
	for (auto subRoot: _rootdir->getChildren()) {
		dir = getOldestDir(subRoot);
		if (!dir->isActive()) {
			return dir;
		}
	}
	
	return nullptr;
}

void Scanner::removeDirs()
{
	Directory *dir, *parent;
	
	
	while (totalSize() > _watermark) {
		dir = getDirToRemove();
		
		
		if (!dir) {
			MSG_WARNING(msg_module, "cannot remove any folder (only active directories)");
			return;
		}
		
		MSG_ERROR(msg_module, "total size %u KB, max %u KB, watermark %u KB", totalSize() /1024, _max_size/1024, _watermark/1024);
		MSG_ERROR(msg_module, "remove %s (%u KB)",	dir->getName().c_str(), dir->getSize()/1024);
		
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
		MSG_ERROR(msg_module, "Adding %s", dir->getName().c_str());
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
		
		MSG_ERROR(msg_module, "%s (%u)", dir->getName().c_str(), dir->getSize());
		
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


void Scanner::popNewestChild(Directory *parent)
{
	Directory *dir = parent->getChildren().back();
	parent->getChildren().pop_back();
	
	while (parent) {
		parent->setSize(parent->getSize() - dir->getSize());
		parent = parent->getParent();
	}
}

void Scanner::addDir(Directory* dir, Directory* parent)
{
	std::lock_guard<std::mutex> lock(_add_lock);
	
	_to_add.push(std::make_pair(dir, parent));
	_add_count++;
	_cv.notify_one();
}

Scanner::addPair Scanner::getNextAdd()
{
	std::lock_guard<std::mutex> lock(_add_lock);
	
	addPair pair = _to_add.front();
	_to_add.pop();
	_add_count--;
	
	return pair;
}

void Scanner::rescan(std::string dir)
{
	std::lock_guard<std::mutex> lock(_scan_lock);
	
	_to_scan.push(dir);
	_scan_count++;
	_cv.notify_one();
}

std::string Scanner::getNextScan()
{
	std::lock_guard<std::mutex> lock(_scan_lock);
	
	std::string dir = _to_scan.front();
	_to_scan.pop();
	_scan_count--;
	
	return dir;
}

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
void Scanner::createDirTree(std::string basedir, int maxdepth)
{
	struct stat st;
	if (!lstat(basedir.c_str(), &st) && S_ISDIR(st.st_mode)) {
		/* Create root directory */
		_rootdir = new Directory(basedir, st.st_mtime, Directory::dirDepth(basedir));
		
		/* set right max depth */
		_max_depth = maxdepth + _rootdir->getDepth();
		
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
		parent->setSize(Directory::dirSize(parent->getName()));
		return;
	}
	
	Directory *aux_dir;
	std::string entry_path, entry_name;
	struct dirent *entry;
	struct stat st;
	uint64_t size = 0;
	
	DIR *dir = opendir(parent->getName().c_str());
	if (!dir) {
		throw std::invalid_argument(std::string("Cannot open " + parent->getName()));
	}
	
	lstat(parent->getName().c_str(), &st);
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
	for (std::vector<Directory *>::iterator it = parent->getChildren().begin(); it != parent->getChildren().end(); ++it) {
		createDirTree(*it);
		size += (*it)->getSize();
	}
	
	if (!parent->getChildren().empty()) {
		parent->sortChildren();
		parent->setAge(parent->getChildren()[0]->getAge());
	}
	
	parent->setSize(size);
}


Scanner::~Scanner()
{
	if (_rootdir) {
		delete _rootdir;
	}
}

} /* end of namespace fbitexpire */