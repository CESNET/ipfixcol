/**
 * \file fbitmerge.cpp
 * \author Michal Kozubik
 * \brief Tool for merging fastbit data
 *
 * Copyright (C) 2011 CESNET, z.s.p.o.
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


#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>

#include <map>
#include <vector>
#include <memory>
#include <iostream>
#include <fstream>

#include <dirent.h>
#include "fbitmerge.h"

#include <fastbit/ibis.h>

#define ARGUMENTS ":hk:b:p:smd"

static uint8_t separated = 0;

/* \brief Erase and clear stringstream
 *
 * \param[in,out] ss stringstream
 * */
inline void clear_ss(std::stringstream *ss) {
	(*ss).str(std::string());
	(*ss).clear();
}


/* \brief Prints help
 */
void usage() {
	std::cout << "\nUsage: fbitmerge [-hsd] -b basedir [-m | -k key] [-p prefix]\n";
	std::cout << "-h\t Show this text\n";
	std::cout << "-b\t Base directory path\n";
	std::cout << "-k\t Merging key (h = hour, d = day...)\n";
	std::cout << "-p\t Prefix of folders with fastbit data (default = none)\n";
	std::cout << "\t !! If there are prefixed folders but prefix is not set, data from these\n";
	std::cout << "\t    folders may be removed, errors may occur!\n";
	std::cout << "-s\t Separate merging - only prefixed folders can be moved and deleted\n";
	std::cout << "\t It means that their parent folders are merged separately, NOT together\n";
	std::cout << "-m\t Move only - don't merge folders, only move all prefixed subdirs into basedir\n";
	std::cout << std::endl;
}

/* \brief Removes folder dirname with all it's files and subfolders
 *
 * \param[in] dirname directory path
 */
void remove_folder_tree(const char *dirname) {
    DIR *dir;
    struct dirent *subdir;
    std::stringstream ss;
    std::string buff;

    dir = opendir(dirname);
	if (dir == NULL) {
		std::cerr << "Error when initializing directory " << dirname << std::endl;
		return;
	}

	/* Go through all files and subfolders */
    while ((subdir = readdir(dir)) != NULL) {
        if (subdir->d_name[0] == '.') {
        	continue;
        }
        ss << dirname << "/" << subdir->d_name;
        buff = ss.str();
        ss.str(std::string());
        ss.clear();

        /* If it is a file, remove it. If not, call remove_folder_tree on subfolder */
        if (subdir->d_type == DT_DIR) {
            remove_folder_tree(buff.c_str());
        } else {
            unlink(buff.c_str());
        }
    }

    closedir(dir);
    rmdir(dirname);
}

/* \brief Checks if the folder could be YYYYMMDDHHmmSS (if prefix is not set)
 *
 * \param[in] dirname directory path
 * \return OK on succes, NOT_OK else
 */
int could_be(char *dirname) {
	if (strlen(dirname) != DIR_NAME_LEN) {
		return NOT_OK;
	}
	char buff[CONTROL_BUFF_LEN];
	char *endp;
	int val = 0;

	memcpy(buff, dirname, YEAR_LEN);
	buff[YEAR_LEN] = '\0';
	val = strtol(buff, &endp, 10);
	if ((*endp != '\0') || (val < 0)) {
		return NOT_OK;
	}

	memcpy(buff, dirname + YEAR_LEN, MONTH_LEN);
	buff[MONTH_LEN] = '\0';
	val = strtol(buff, &endp, 10);
	if ((*endp != '\0') || (val < 0) || (val > MAX_MONTH)) {
		return NOT_OK;
	}

	memcpy(buff, dirname + YEAR_LEN + MONTH_LEN, DAY_LEN);
	buff[DAY_LEN] = '\0';
	val = strtol(buff, &endp, 10);
	if ((*endp != '\0') || (val < 0) || (val > MAX_DAY)) {
		return NOT_OK;
	}

	memcpy(buff, dirname + DATE_LEN, HOUR_LEN);
	buff[HOUR_LEN] = '\0';
	val = strtol(buff, &endp, 10);
	if ((*endp != '\0') || (val < 0) || (val > MAX_HOUR)) {
		return NOT_OK;
	}

	memcpy(buff, dirname + DATE_LEN + HOUR_LEN, MIN_LEN);
	buff[MIN_LEN] = '\0';
	val = strtol(buff, &endp, 10);
	if ((*endp != '\0') || (val < 0) || (val > MAX_MIN)) {
		return NOT_OK;
	}

	memcpy(buff, dirname + DATE_LEN + HOUR_LEN + MIN_LEN, SEC_LEN);
	buff[SEC_LEN] = '\0';
	val = strtol(buff, &endp, 10);
	if ((*endp != '\0') || (val < 0) || (val > MAX_SEC)) {
		return NOT_OK;
	}
	return OK;
}

/* \brief Merge two flowsstats file into one (doesn't remove anything)
 *
 * Opens 2 flowsStats.txt files, reads values of exported, received and lost flows
 * and sum of both saves into second file
 *
 * \param[in] first path to the first file
 * \param[in,out] second path to the second file
 * \return OK on succes, NOT_OK else
 */
void merge_flowStats(const char *first, const char *second) {
	std::fstream file_f;
	std::fstream file_s;

	file_f.open(first, std::fstream::in);
	if (!file_f.is_open()) {
		std::cerr << "Can't open file " << first << " for reading!\n";
	}
	file_s.open(second, std::fstream::in);
	if (!file_s.is_open()) {
		std::cerr << "Can't open file " << second << " for reading!\n";
		file_f.close();
	}
	int expFlows = 0;
	int recFlows = 0;
	int lostFlows = 0;

	std::string buff;

	/* Read data from first file */
	if (file_f.is_open()) {
		std::getline(file_f, buff);
		expFlows += atoi(buff.substr(buff.find(":")+2, buff.length()).c_str());
		std::getline(file_f, buff);
		recFlows += atoi(buff.substr(buff.find(":")+2, buff.length()).c_str());
		std::getline(file_f, buff);
		lostFlows += atoi(buff.substr(buff.find(":")+2, buff.length()).c_str());
		file_f.close();
	}

	/* Add data from second file */
	if (file_s.is_open()) {
		std::getline(file_s, buff);
		expFlows += atoi(buff.substr(buff.find(":")+2, buff.length()).c_str());
		std::getline(file_s, buff);
		recFlows += atoi(buff.substr(buff.find(":")+2, buff.length()).c_str());
		std::getline(file_s, buff);
		lostFlows += atoi(buff.substr(buff.find(":")+2, buff.length()).c_str());
		file_s.close();
	}

	/* Save data into second file */
	file_s.open(second, std::fstream::out | std::fstream::trunc);
	if (!file_s.is_open()) {
		std::cerr << "Can't open file " << second << " for writing!\n";
	} else {
		file_s << "Exported flows: " << expFlows << std::endl;
		file_s << "Received flows: " << recFlows << std::endl;
		file_s << "Lost flows: " << lostFlows << std::endl;
		file_s.close();
	}
}

/* \brief Merges 2 folders containing fastbit data together (into second folder)
 *
 * \param[in] srcDir source folder path
 * \param[in,out] dstDir destination folder path
 * \return OK on succes, NOT_OK else
 */
int merge_dirs(const char *srcDir, const char *dstDir) {
    /* Table initialization */
    ibis::part part(dstDir, static_cast<const char*>(0));

    /* If there are no rows, we have nothing to do */
    if (part.nRows() == 0) {
    	return OK;
    }

    if (part.append(srcDir) < 0) {
    	return NOT_OK;
    }
    part.commit(dstDir);
    return OK;
}
//
//int merge_dirs(const char *srcDir, const char *dstDir) {
//    /* Table initialization */
//    ibis::part part(srcDir, static_cast<const char*>(0));
//    ibis::bitvector *bv;
//    ibis::tablex *tablex = NULL;
//    uint32_t i;
//
//    /* If there are no rows, we have nothing to do */
//    if (part.nRows() == 0) {
//    	return OK;
//    }
//
////    /* Set all bitmaps to 1 */
////    bv = new ibis::bitvector();
////    bv->appendFill(1, part.nRows());
//
//    ibis::part dpart(dstDir, static_cast<const char*>(0));
//    dpart.append(srcDir);
//    dpart.commit(dstDir);
//    return OK;
//
//   	tablex = ibis::tablex::create();
//
//   	std::stringstream ss;
//
//    for(i = 0; i < part.nColumns(); i++) {
//    	unsigned char *s8;
//		uint16_t *s16;
//		uint32_t *s32;
//		uint64_t *s64;
//		ibis::column *c;
//
//		c = part.getColumn(i);
//		part.append("");
//		std::cout << c->name() << "|" << c->type() << "|" << part.nRows() << "|" << c->elementSize() << std::endl;
//		switch(c->elementSize()) {
//		case BYTES_1: {
//			std::auto_ptr< ibis::array_t<unsigned char> > tmp(part.selectUBytes(c->name(), *bv));
//			s8 = tmp->begin();
//
//			tablex->addColumn(c->name(), c->type());
//			tablex->append(c->name(), 0, part.nRows(), s8);
//			break;}
//		case BYTES_2: {
//			std::auto_ptr< ibis::array_t<uint16_t> > tmp(part.selectUShorts(c->name(), *bv));
//			s16 = tmp->begin();
//
//			tablex->addColumn(c->name(), c->type());
//			tablex->append(c->name(), 0, part.nRows(), s16);
//			break;}
//		case BYTES_4: {
//			std::auto_ptr< ibis::array_t<uint32_t> > tmp(part.selectUInts(c->name(), *bv));
//			s32 = tmp->begin();
//
//			tablex->addColumn(c->name(), c->type());
//			tablex->append(c->name(), 0, part.nRows(), s32);
//			break;}
//		case BYTES_8: {
//			std::auto_ptr< ibis::array_t<uint64_t> > tmp(part.selectULongs(c->name(), *bv));
//			s64 = tmp->begin();
//
//			tablex->addColumn(c->name(), c->type());
//			tablex->append(c->name(), 0, part.nRows(), s64);
//			break;}
//		}
//    }
//
//    /* Write new rows into dstDir part file */
//   	std::cout << "result = " << tablex->write(dstDir, 0, 0) << "\n";
//    delete tablex;
//    return OK;
//}

void scan_dir(const char *dirname,const char *srcDir, DIRMAP *bigMap) {
    ibis::part part(srcDir, static_cast<const char*>(0));

    if (part.nRows() == 0) {
    	return;
    }

    for (uint32_t i = 0; i < part.nColumns(); i++) {
    	ibis::column *c = part.getColumn(i);
    	(*bigMap)[std::string(dirname)][std::string(c->name())] = c->type();
    }
}


int same_data(innerDirMap *first, innerDirMap *second) {
	if ((*first).size() != (*second).size()) {
		return NOT_OK;
	}
	for (innerDirMap::iterator it = (*first).begin(); it != (*first).end(); it++) {
		if ((*second).find((*it).first) == (*second).end()) {
			return NOT_OK;
		}
		if ((*it).second != (*second)[(*it).first]) {
			return NOT_OK;
		}
	}
	return OK;
}

/* \brief Merges 2 folders in format <prefix>YYYYMMDDHHmmSS together
 *
 * Goes through subfolders of destination folder and saves their names. Then
 * goes through subfolders of source folder and compare folder names
 * with first folder. Same folders are merged together (merge_dirs). If
 * folder doesnt exists in dstDir, it is moved there
 *
 * \param[in] srcDir source folder name
 * \param[in,out] dstDir destination folder name
 * \param[in] workDir parent directory
 * \return OK on success, NOT_OK else
 */
int merge_couple(const char *srcDir, const char *dstDir, const char *workDir) {
	DIR *sdir = NULL;
	DIR *ddir = NULL;
	std::stringstream ss;
	std::string buff;

	/* Get full paths of folders */
	clear_ss(&ss);
	ss << workDir << "/" << srcDir;
	buff = ss.str();

	clear_ss(&ss);
	ss << workDir << "/" << dstDir;

	/* Open them */
	sdir = opendir(buff.c_str());
	if (sdir == NULL) {
		std::cerr << "Error while opening " << buff << std::endl;
		return NOT_OK;
	}
	ddir = opendir(ss.str().c_str());
	if (ddir == NULL) {
		std::cerr << "Error while opening " << ss.str() << std::endl;
		closedir(sdir);
		return NOT_OK;
	}

	/* Scan folders in dstDir folder and save them to DIRMAP */
	struct dirent *subdir = NULL;

	DIRMAP srcMap, dstMap;
	while ((subdir = readdir(ddir)) != NULL) {
		if ((subdir->d_name[0] == '.') || (subdir->d_type != DT_DIR)) {
			continue;
		}
		clear_ss(&ss);
		ss << workDir << "/" << dstDir << "/" << subdir->d_name;
		scan_dir(subdir->d_name, ss.str().c_str(), &dstMap);
	}

	/* Now do the same with srcDir */
	while ((subdir = readdir(sdir)) != NULL) {
		if ((subdir->d_name[0] == '.') || (subdir->d_type != DT_DIR)) {
			continue;
		}
		clear_ss(&ss);
		ss << workDir << "/" << srcDir << "/" << subdir->d_name;
		scan_dir(subdir->d_name, ss.str().c_str(), &srcMap);
	}

	/* Iterate through whole dstMap and srcMap and find folders with same data (and data types) */
	for (DIRMAP::iterator dsti = dstMap.begin(); dsti != dstMap.end(); dsti++) {
		for (DIRMAP::iterator srci = srcMap.begin(); srci != srcMap.end(); srci++) {
			if (same_data(&((*dsti).second), &((*srci).second)) == OK) {
				/* If found, merge them */
				clear_ss(&ss);
				ss << workDir << "/" << srcDir << "/" << (*srci).first;
				buff = ss.str();
				clear_ss(&ss);
				ss << workDir << "/" << dstDir << "/" << (*dsti).first;

				if (merge_dirs(buff.c_str(), ss.str().c_str()) != OK) {
					return NOT_OK;
				}

				/* We don't need this folder anymore */
				srcMap.erase((*srci).first);
			}
		}
	}

	/* If there is some folder that wasn't merged, just move it to dstDir */
	/* But we must find unused folder name for it (we dont wan't to rewrite some other folder */
	for (DIRMAP::iterator srci = srcMap.begin(); srci != srcMap.end(); srci++) {
		clear_ss(&ss);
		ss << workDir << "/" << srcDir << "/" << (*srci).first;
		buff = ss.str();
		clear_ss(&ss);
		ss << workDir << "/" << dstDir << "/" << (*srci).first;
		struct stat st;
		char suffix = 'a';

		/* Add suffix to the name */
		while (stat(ss.str().c_str(), &st) == 0) {
			clear_ss(&ss);
			ss << workDir << "/" << dstDir << "/" << (*srci).first << suffix;
			if (suffix == 'z') {
				suffix = 'A';
			} else if (suffix == 'Z') {
				/* \TODO do it better */
				std::cerr << "Not enough suffixes for folder " << (*srci).first << std::endl;
				break;
			} else {
				suffix++;
			}
		}
		if (rename(buff.c_str(), ss.str().c_str()) != 0) {
			std::cerr << "Can't move folder " << buff << std::endl;
		}
	}

	/* Now merge template directories with same data (and data types) in dstDir */
	for (DIRMAP::iterator dsti = dstMap.begin(); dsti != dstMap.end(); dsti++) {
		for (DIRMAP::iterator it = dsti; it != dstMap.end(); it++) {
			if (it == dsti) {
				continue;
			}
			if (same_data(&((*dsti).second), &((*it).second)) == OK) {
				clear_ss(&ss);
				ss << workDir <<  "/" << dstDir << "/" << (*it).first;
				buff = ss.str();
				clear_ss(&ss);
				ss << workDir << "/" << dstDir << "/" << (*dsti).first;

				if (merge_dirs(buff.c_str(), ss.str().c_str()) != OK) {
					return NOT_OK;
				}
				remove_folder_tree(buff.c_str());
				dstMap.erase((*it).first);
			}
		}
	}

	/* Finally merge flowsStats.txt files */
	clear_ss(&ss);
	ss << workDir << "/" << srcDir << "/" << "flowsStats.txt";
	buff = ss.str();
	clear_ss(&ss);
	ss << workDir << "/" << dstDir << "/" << "flowsStats.txt";

	merge_flowStats(buff.c_str(), ss.str().c_str());

	closedir(sdir);
	closedir(ddir);

	return OK;
}

/* \brief Goes through folder containing prefixed subfolders and merges them together by key
 *
 * Goes through workDir subfolders and looks at key values.
 * Folders with same key values are merged together.
 *
 * \param[in] workDir folder with prefixed subfolders
 * \param[in] key key value
 * \param[in] prefix folder prefix
 * \return OK on success, NOT_OK else
 */
int merge_all(const char *workDir, uint16_t key, const char *prefix) {
	DIR *dir = NULL;
	dir = opendir(workDir);
	if (dir == NULL) {
		std::cerr << "Error when initializing directory " << workDir << std::endl;
		return NOT_OK;
	}
	uint16_t prefix_len;
	if (prefix == NULL) {
		prefix_len = 0;
	} else {
		prefix_len = strlen(prefix);
	}

	/* Set size appropriate to key value */
	uint16_t size = 0;
	switch (key) {
	case YEAR:
		size += YEAR_LEN;
		break;
	case MONTH:
		size += YEAR_LEN + MONTH_LEN;
		break;
	case DAY:
		size += DATE_LEN;
		break;
	case HOUR:
		size += DATE_LEN + HOUR_LEN;
		break;
	default:
		std::cerr << "Undefined key value!\n";
		return NOT_OK;
	}

	/* Go through subdirs */
	std::map<uint32_t, char *> dirMap;
	struct dirent *subdir = NULL;
	char key_str[size+1];
	std::string buff;
	std::stringstream ss;

	while ((subdir = readdir(dir)) != NULL) {
		if (subdir->d_name[0] == '.') {
			continue;
		}

		/* Get key value */
		memset(key_str, 0, size+1);
		memcpy(key_str, subdir->d_name + prefix_len, size);
		uint32_t key_int = atoi(key_str);

		/* If it's not the first occurence of key value, merge these 2 folders,
		 * else save it to map */
		if (dirMap.find(key_int) == dirMap.end()) {
			dirMap[key_int] = subdir->d_name;
		} else {

			if (merge_couple(subdir->d_name, dirMap[key_int], workDir) != OK) {
				return NOT_OK;
			}

			/* Remove merged src folder */
			clear_ss(&ss);
			ss << workDir << "/" << subdir->d_name;
			remove_folder_tree(ss.str().c_str());
		}
	}

	/* Rename folders - reset name values after key to 0 */
	for (std::map<uint32_t, char *>::iterator i = dirMap.begin(); i != dirMap.end(); i++) {
		char *last = (i->second + prefix_len + size);
		if (atoi(last) != 0) {
			clear_ss(&ss);
			ss << workDir << "/" << i->second;
			buff = ss.str();
			memset(last, ASCII_ZERO, DIR_NAME_LEN - size);
			memset(last + DIR_NAME_LEN - size, 0, 1);
			clear_ss(&ss);
			ss << workDir << "/" << i->second;
			if (rename(buff.c_str(), ss.str().c_str()) != 0) {
				std::cerr << "Error while moving folder " << buff << std::endl;
			}
		}
	}
	closedir(dir);
	return OK;
}

/* \brief Moves all prefixed subdirs into base dir
 *
 * Recursively goes through all basedir sub folders and looks for prefixed folders.
 * If prefix is not set, it tries to guess if it is the right folder. When found, moves
 * it to basedir. If separated is set, it does't move the folder but merge_all is called.
 *
 * \param[in] baseDir base folder where all prefixed folders will be moved
 * \param[in] workDir actual directory
 * \param[in] prefix prefix
 * \param[in] key key value
 */
int move_prefixed_dirs(const char *baseDir, const char *workDir, const char *prefix, int key) {
	DIR *dir;
	struct dirent *subdir;
	std::string buff;
	std::stringstream ss;

	dir = opendir(workDir);
	if (dir == NULL) {
		std::cerr << "Error when initializing directory " << workDir << std::endl;
		return NOT_OK;
	}

	/* Cycle through all files subfolders */
	while ((subdir = readdir(dir)) != NULL) {
		if (subdir->d_name[0] == '.') {
			continue;
		}

		if (subdir->d_type == DT_DIR) {
			clear_ss(&ss);
			ss << workDir << "/" << subdir->d_name;
			buff = ss.str();

			/* No prefix? So guess if it is the right folder */
			if (prefix == NULL) {
				if (could_be(subdir->d_name) == OK) {
					/* Separate set - don't move with workDir */
					if (separated) {
						merge_all(workDir, key, prefix);
						break;
					}
					/* Separate NOT set - we can move this dir to baseDir */
					clear_ss(&ss);
					ss << baseDir << "/" << subdir->d_name;

					if (rename(buff.c_str(), ss.str().c_str()) != 0) {
						std::cerr << "Error while moving folder " << buff << std::endl;
					}
				} else {
					if (move_prefixed_dirs(baseDir, buff.c_str(), prefix, key) != OK) {
						return NOT_OK;
					}
				}
			} else if (strstr(subdir->d_name, prefix) == subdir->d_name) {
				if (separated) {
					merge_all(workDir, key, prefix);
					break;
				}
				clear_ss(&ss);
				ss << baseDir << "/" << subdir->d_name;
				if (rename(buff.c_str(), ss.str().c_str()) != 0) {
					std::cerr << "Error while moving folder " << buff << std::endl;
				}
			} else {
				if (move_prefixed_dirs(baseDir, buff.c_str(), prefix, key) != OK) {
					return NOT_OK;
				}
			}
		/* Some file was found - move it to baseDir too (if separate is not set) */
		} else if (separated == 0) {
			clear_ss(&ss);
			ss << workDir << "/" << subdir->d_name;
			buff = ss.str();
			clear_ss(&ss);
			ss << baseDir << "/" << subdir->d_name;
			if (rename(buff.c_str(), ss.str().c_str()) != 0) {
				std::cerr << "Error while moving file " << buff << std::endl;
			}
		}
	}

	closedir(dir);
	if (!separated) {
		rmdir(workDir);
	}
	return OK;
}

int main(int argc, char *argv[]) {
	if (argc <= 1) {
		usage();
		return OK;
	}
	ibis::gVerbose = -10;

	/* Process arguments */
	int option;
	int key = -1;
	int moveOnly = 0;

	std::string basedir;
	std::stringstream ss;
	std::string prefix;

	while ((option = getopt(argc, argv, ARGUMENTS)) != -1) {
		switch (option) {
		case 'h':
			usage();
			return OK;
		case 'k':
			if (!strcasecmp(optarg, "hour") || !strcasecmp(optarg, "h")) {
				key = HOUR;
			} else if (!strcasecmp(optarg, "day") || !strcasecmp(optarg, "d")) {
				key = DAY;
			} else if (!strcasecmp(optarg, "month") || !strcasecmp(optarg, "m")) {
				key = MONTH;
			} else if (!strcasecmp(optarg, "year") || !strcasecmp(optarg, "y")) {
				key = YEAR;
			}
			break;
		case 'b':
			ss << optarg;
			basedir = ss.str();
			ss.str(std::string());
			ss.clear();
			break;
		case 'p':
			ss << optarg;
			prefix = ss.str();
			ss.str(std::string());
			ss.clear();
			break;
		case 's':
			separated = 1;
			break;
		case 'm':
			moveOnly = 1;
			break;
		case '?':
			std::cerr << "Unkwnown argument " << (char) optopt << std::endl;
			return NOT_OK;
		case ':':
			std::cerr << "Missing parameter for argument " << (char) optopt << std::endl;
			return NOT_OK;
		default:
			std::cerr << "Unknown error\n";
			break;
		}
	}
	if (basedir.empty()) {
		std::cerr << "\nBase directory path not set!\n\n";
		return NOT_OK;
	}
	if (prefix.empty()) {
		std::cout << "\nWarning: Prefix not set!\n\n";
	}

	/* Move all prefixed subdirs into basedir (or merge_all if separated set) */
	if (moveOnly != 0) {
		if (separated) {
			std::cerr << "-s and -m arguments can't be set together!\n";
			return NOT_OK;
		}
		if (move_prefixed_dirs(basedir.c_str(), basedir.c_str(), (prefix.empty() ? NULL : prefix.c_str()), key) != OK) {
			std::cerr << "Moving folders failed\n";
			return NOT_OK;
		}
		return OK;
	}

	if (key == -1) {
		std::cerr << "\nUndefined key argument!\n\n";
		return NOT_OK;
	}

	if (move_prefixed_dirs(basedir.c_str(), basedir.c_str(), (prefix.empty() ? NULL : prefix.c_str()), key) != OK) {
		std::cerr << "Moving folders failed!\n";
		return NOT_OK;
	}
	/* if separate not set merge all in basedir */
	if (!separated) {
		merge_all(basedir.c_str(), key, prefix.c_str());
	}
	return OK;
}
