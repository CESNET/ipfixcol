/**
 * \file Directory.h
 * \author Michal Kozubik <kozubik@cesnet.cz>
 * \brief Directory class for fbitexpire tool
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

#ifndef DIRECTORY_H
#define	DIRECTORY_H

#include <iostream>
#include <vector>
#include <algorithm>

namespace fbitexpire {

/**
 * \brief Class representing directory on filesystem
 */
class Directory {
public:
    using dirVec = std::vector<Directory *>;
    
    Directory() {}
    
    Directory(std::string name, int age, int depth, Directory *parent = nullptr, bool active = false): 
    _name{name}, _age{age}, _depth{depth}, _parent{parent}, _active{active} {}
    
    ~Directory();

    bool isActive()                    { return   _active; }    
    void setActive(bool active = true) { _active = active; }
     
    dirVec &getChildren() { return _children; }
    
    Directory *getOldestChild() { return _children.empty() ? nullptr : _children.front(); }
    Directory *getNewestChild() { return _children.empty() ? nullptr : _children.back(); }
    
    std::string getName() { return _name; }
    
    Directory *getParent()            { return   _parent; }
    void setParent(Directory *parent) { _parent = parent; }
    
    int  getAge()        { return _age; }
    void setAge(int age) { _age  = age; }
    
    int  getDepth()          { return  _depth; }
    void setDepth(int depth) { _depth = depth; }
    
    uint64_t getSize()              { return _size; }
    void     setSize(uint64_t size) { _size = size; }
    
    void addChild(Directory *child) { _children.push_back(child); child->setParent(this); }
    void sortChildren() { std::sort(_children.begin(), _children.end(), cmpDirDate); }
    
    void removeOldest();
    void detectAge();
    
    void rescan();
    void updateAge();
    
    uint64_t countSize()      { return dirSize(_name, false, true, true);   }
    uint64_t countFilesSize() { return dirSize(_name, false, false, false); }
    
    static uint64_t dirSize(std::string path, bool force, bool recursive = true, bool writestats = true);
    static uint64_t dirSize(Directory *dir);
    
    static int  dirDepth(std::string dir) { return std::count(dir.begin(), dir.end(), '/'); }
    static bool cmpDirDate(Directory *first, Directory *second) { return first->_age < second->_age; }
    static std::string correctDirName(std::string dir);
private:
    std::string _name;     /**< name (full absolute path) */
    int _age;              /**< age (time last modified) */
    int _depth;            /**< depth */
    Directory *_parent;    /**< parent directory */
    bool _active = false;  /**< activity flag - true if data writer writes into this folder */
    
    dirVec   _children;    /**< children vector */
    uint64_t _size = 0;    /**< directory size in bytes */
};

} /* end of namespace fbitexpire */

#endif	/* DIRECTORY_H */

