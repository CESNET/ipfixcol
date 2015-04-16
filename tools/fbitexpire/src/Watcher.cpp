/**
 * \file Watcher.cpp
 * \author Michal Kozubik <kozubik@cesnet.cz>
 * \brief Watcher class for fbitexpire tool
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

#include "Watcher.h"
#include "verbose.h"

#include <thread>
#include <stdexcept>
#include <iostream>

#include <sys/prctl.h>
#include <signal.h>

static const char *msg_module = "Watcher";

namespace fbitexpire {

/**
 * \brief Run watcher thread
 * 
 * \param scanner Scanner's instance
 * \param multiple Allow multiple data writers
 */
void Watcher::run(Scanner* scanner, bool multiple = false)
{
	_scanner   = scanner;
	_max_depth = _scanner->getMaxDepth();
	_multiple  = multiple; 
	
	/* prepare inotify, roots etc. */
	setup();
	
	/* run watcher */
	run(); 
}

/**
 * \brief Stop thread
 */
void Watcher::stop()
{
	_done = true;

	/*
	 * Unregister signal handler so the default handler
	 * will be called when watcher sends SIGINT to it's
	 * main loop. This will cause inotify to exit read().
	 */
	signal(SIGINT, SIG_DFL);

	/* Stop waiting on inotify events */
	pthread_kill(_th.native_handle(), SIGINT);
	
	if (_th.joinable()) {
		_th.join();
	}
}

/**
 * \brief Main loop
 */
void Watcher::loop()
{
	prctl(PR_SET_NAME, "fbitexp:Watcher\0", 0, 0, 0);
	
	MSG_DEBUG(msg_module, "started");
	
	size_t count;
	InotifyEvent event;
	bool got_event;
	
	while (!_done) {
		try {
			/* Get new inotify events */
			_inotify.WaitForEvents();
			if (_done) {
				break;
			}
			
			count = _inotify.GetEventCount();

			/* Process all events */
			while (count > 0) {
				got_event = _inotify.GetEvent(&event);
				if (got_event && event.IsCreateDir()) {
					processNewDir(event);
				}

				count--;
			}
		} catch (InotifyException &e) {
			MSG_ERROR(msg_module, "%s", e.GetMessage().c_str());
		}
	}
	
	MSG_DEBUG(msg_module, "closing thread");
}

/**
 * \brief Prepare inotify and set it to watch the newest directory branch
 *			(or branches when multiple is true)
 */
void Watcher::setup()
{
	Directory *root{_scanner->getRoot()};
	RootWatch *rw;
	
	_root_name_len = root->getName().length();
	
	
	if (_multiple) {
		/*
		 * Multiple sources can write to top level directory
		 * Set each it's subdir as separate directory tree root
		 */
		watch(nullptr, root);
		for (auto subRoot: root->getChildren()) {
			rw = new RootWatch(subRoot);
			_roots.push_back(rw);
			
			/* Watch directory subtree */
			watchRootWatch(rw);
		}
	} else {
		/* No multiple sources, only one root directory */
		rw = new RootWatch(root);
		_roots.push_back(rw);
		
		/* Watch directory subtree */
		watchRootWatch(rw);
	}
}

/**
 * \brief Set inotify on new directory for given root
 * 
 * \param rw Root of (sub)tree
 * \param dir Directory to be watched
 */
void Watcher::watch(RootWatch* rw, Directory* dir)
{
	MSG_DEBUG(msg_module, "watch %s", dir->getName().c_str());
	InotifyWatch *watch = new InotifyWatch(dir->getName());
	_inotify.Add(watch);
	
	dir->setActive();
	
	if (rw) {
		rw->watching.push_back(dir);
	}
}

/**
 * \brief Set inotify on the newest branch in RootWatch
 * 
 * \param rw Root of (sub)tree
 */
void Watcher::watchRootWatch(RootWatch* rw)
{
	Directory *aux_dir = rw->root;
	
	while (aux_dir && aux_dir->getDepth() < _max_depth) {
		watch(rw, aux_dir);
		if (aux_dir->getChildren().empty() && aux_dir != rw->root) {
			/* 
			 * The newest dir in subtree should removed from dir hierarchy
			 * It will be added when new dir appears
			 */
			_scanner->popNewestChild(aux_dir->getParent());
			return;
		}
		aux_dir = aux_dir->getNewestChild();
	}
}

/**
 * \brief Remove inotify from given directory
 * 
 * \param dir Directory to be unwatched
 */
void Watcher::unWatch(Directory *dir)
{
	MSG_DEBUG(msg_module, "unWatch %s", dir->getName().c_str());
	InotifyWatch *watch = _inotify.FindWatch(dir->getName());
	if (watch) {
		_inotify.Remove(watch);
		delete watch;
	}
	dir->setActive(false);
}

/**
 * \brief Remove inotify from last directory in RootWatch's stack
 * 
 * \param rw Root of (sub)tree
 */
void Watcher::unWatchLast(RootWatch* rw)
{
	unWatch(rw->watching.back());
	rw->watching.pop_back();
}

/**
 * \brief Get root for given directory
 * 
 * \param dir Directory
 * \return  RootWatch instance
 */
RootWatch *Watcher::getRoot(Directory *dir)
{
	if (!_multiple) {
		return _roots.front();
	}
	
	/* 
	 * Get root directory from full path 
	 * it ends at first occurence of / from position at length of basedir + 1 
	 */
	std::string root = dir->getName().substr(0, dir->getName().find("/", _root_name_len + 1));
	
	for (auto x: _roots) {
		if (x->root->getName() == root) {
			return x;
		}
	}
	
	/* New root */
	dir->setParent(_scanner->getRoot());
	RootWatch *rw = new RootWatch(dir);
	_roots.push_back(rw);
	
	return rw;
}

/**
 * \brief Process newly created directory
 * 
 * \param event Inotify event
 */
void Watcher::processNewDir(InotifyEvent& event)
{
	std::string parent_path = event.GetWatch()->GetPath();
	std::string new_path = parent_path + "/" + event.GetName();
	int depth = Directory::dirDepth(new_path);
	
	if (depth >= _max_depth) {
		MSG_DEBUG(msg_module, "%s is too deep", new_path.c_str());
		return;
	}
	
	Directory *newdir = new Directory(new_path, 0, depth, nullptr, true);
	RootWatch *rw = getRoot(newdir);
	
	if (rw->root == newdir) {
		/* We found a new root dir - add it to dir tree*/
		_scanner->addDir(newdir, newdir->getParent());
	} else if (rw->watching.back()->getName() == parent_path) {
		/*
		 * New directory is child of previous watched directory
		 * So parent can be added to dir tree (if it is not root which is already there)
		 * and we can watch new directory
		 */
		if (parent_path != rw->root->getName()) {
			_scanner->addDir(rw->watching.back(), rw->watching.back()->getParent());
		}
	} else {
		/*
		 * New directory is NOT child of recently watched directory
		 * (it can be sibling..). It means that this is the most actual
		 * substree and all other subtrees can be unwatched
		 */
		Directory *oldDir = rw->watching.back();
		
		while (rw->watching.back()->getName() != parent_path && rw->watching.back() != rw->root) {
			unWatchLast(rw);
		}
		
		/* Folder has to be added AFTER unWatch because of active flag */
		_scanner->addDir(oldDir, oldDir->getParent());
	}
	
	if (newdir != rw->root) {
		newdir->setParent(rw->watching.back());
	}
	watch(rw, newdir);
}

/**
 * \brief Constructor
 */
Watcher::Watcher():
		_scanner(NULL), _max_depth(0), _root_name_len(0), _multiple(false)
{
	
}

/**
 * \brief Destructor - remove RootWatches
 */
Watcher::~Watcher()
{
	for (auto rw: _roots) {
		/* Last watched directory was NOT added to scanner (if it is not root) so we need to delete it here */
		if (!rw->watching.empty() && rw->watching.back() != rw->root) {
			delete rw->watching.back();
		}
		delete rw;
	}
	
	_roots.clear();
}

} /* end of namespace fbitexpire */
