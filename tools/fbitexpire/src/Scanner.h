/**
 * \file Scanner.h
 * \author Michal Kozubik <kozubik@cesnet.cz>
 * \brief Scanner class for fbitexpire tool
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


#ifndef SCANNER_H_
#define SCANNER_H_

#include "fbitexpire.h"
#include "Cleaner.h"
#include "Directory.h"

#include <algorithm>
#include <atomic>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>

namespace fbitexpire {

/**
 * \brief Main scanner class for scanning directories
 */
class Scanner : public FbitexpireThread {
	using addPair = std::pair<Directory *, Directory *>;
	using FbitexpireThread::run;
public:
	Scanner();
	~Scanner();

	void createDirTree(std::string basedir, int maxdepth = 1, bool force = false);
	void popNewestChild(Directory *parent);
	
	Directory *getRoot() { return _rootdir;   }
	int getMaxDepth()    { return _max_depth; }
	
	void setMaxSize(uint64_t max, bool notify = false)    { _max_size = max; if (_watermark > _max_size) { _watermark = _max_size; } if (notify) { _cv.notify_one(); } }
	void setMaxSize(std::string max, bool notify = false) { setMaxSize(strtoull(max.c_str(), nullptr, 10), notify); }
	
	void setWatermark(uint64_t wm)    { if (wm > _max_size) { wm = _max_size; } _watermark = wm; };
	void setWatermark(std::string wm) { setWatermark(strtoull(wm.c_str(), nullptr, 10)); }
	
	void addDir(Directory *dir, Directory *parent);
	void rescan(std::string dir);
	
	Directory *dirFromPath(std::string path);
	
	void stop();
	void run(Cleaner *cleaner, uint64_t max_size, uint64_t waternark, bool multiple = false);
	
	static std::string sizeToStr(uint64_t size);
	static uint64_t    strToSize(char *arg);
	static uint64_t    strToSize(std::string str) { return strToSize(str.c_str()); }
private:
	void loop();
	
	void createDirTree(Directory *parent);
	int scanCount() { return _scan_count.load(); }
	int addCount() { return  _add_count.load(); }
	
	void addNewDirs();
	void rescanDirs();
	void removeDirs();
	
	void propagateSize(Directory *parent, uint64_t size);
	
	uint64_t totalSize()   { return _rootdir->getSize();   }
	
	std::string getNextScan();
	addPair     getNextAdd();
	
	Directory *getOldestDir(Directory *root);
	Directory *getDirToRemove();
	
	Cleaner   *_cleaner;
	Directory *_rootdir; /** Root directory */
	
	std::thread _th;
	std::mutex _scan_lock;
	std::mutex  _add_lock;
	
	std::atomic<int> _scan_count{0};
	std::atomic<int>  _add_count{0};
	
	std::queue<std::string> _to_scan;
	std::queue<addPair>     _to_add;
	
	int _max_depth;
	
	std::condition_variable _cv;
	
	bool _multiple, _force;
	uint64_t _max_size, _watermark;
};

} /* end of namespace fbitexpire */

#endif /* SCANNER_H_ */
