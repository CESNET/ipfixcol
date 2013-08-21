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

#define ARGUMENTS ":hk:b:p:sm"

static uint8_t separated = 0;
static bool dumpMode = false;


void dbg(std::string text) {
	return;
	std::cout << "debug: " << text << std::endl;
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
	std::cout << "-d\t Enable dump mode\n";
	std::cout << std::endl;
}

/* \brief Removes folder dirname with all it's files and subfolders
 *
 * \param[in] dirname directory path
 */
void remove_folder_tree(const char *dirname) {
	dbg("remove started");
    DIR *dir;
    struct dirent *subdir;
    char buff[DIR_NAME_MAX_LEN];
    memset(buff, 0, DIR_NAME_MAX_LEN);

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
        snprintf(buff, DIR_NAME_MAX_LEN, "%s/%s", dirname, subdir->d_name);

        /* If it is a file, remove it. If not, call remove_folder_tree on subfolder */
        if (subdir->d_type == DT_DIR) {
            remove_folder_tree(buff);
        } else {
            unlink(buff);
        }
    }

    closedir(dir);
    dbg("remove ending");
    rmdir(dirname);
}

/* \brief Checks if the folder could be YYYYMMDDHHmmSS (if prefix is not set)
 *
 * \param[in] dirname directory path
 * \return OK on succes, NOT_OK else
 */
int could_be(char *dirname) {
	dbg("could_be started");
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
	dbg("could_be ending");
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
	dbg("mfS started");
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
	dbg("mfS ending");
}

/* \brief Merges 2 folders containing fastbit data together (into second folder)
 *
 * \param[in] srcDir source folder path
 * \param[in,out] dstDir destination folder path
 * \return OK on succes, NOT_OK else
 */
int merge_dirs(const char *srcDir, const char *dstDir) {
	dbg("m_dirs started");
    /* Table initialization */
    ibis::part part(srcDir, static_cast<const char*>(0));
    ibis::bitvector *bv;
    ibis::tablex *tablex = NULL;
    uint32_t i;

    /* If there are no rows, we have nothing to do */
    if (part.nRows() == 0) {
    	return OK;
    }

    /* Set all bitmaps to 1 */
    bv = new ibis::bitvector();
    bv->appendFill(1, part.nRows());

    if (!dumpMode) {
    	tablex = ibis::tablex::create();
    }

    for(i = 0; i < part.nColumns(); i++) {
    	unsigned char *s8;
		uint16_t *s16;
		uint32_t *s32;
		uint64_t *s64;
		ibis::column *c;
		char path[DIR_NAME_MAX_LEN];
		FILE *fd;

		c = part.getColumn(i);
		snprintf(path, DIR_NAME_MAX_LEN, "%s/%s", dstDir, c->name());

		switch(c->elementSize()) {
		case BYTES_1: {
			std::auto_ptr< ibis::array_t<unsigned char> > tmp(part.selectUBytes(c->name(), *bv));
			s8 = tmp->begin();
			if (dumpMode) {
				if ((fd = fopen(path, "a")) != NULL) {
					for(uint32_t j = 0; j < part.nRows() - 1; j++) {
						fprintf(fd, "%u\n", s8[j]);
					}
					fclose(fd);
				} else {
					delete tablex;
					return NOT_OK;
				}
			} else {
				tablex->addColumn(c->name(), c->type());
				tablex->append(c->name(), 0, part.nRows() - 1, s8);
			}
			break;}
		case BYTES_2: {
			std::auto_ptr< ibis::array_t<uint16_t> > tmp(part.selectUShorts(c->name(), *bv));
			s16 = tmp->begin();
			if (dumpMode) {
				if ((fd = fopen(path, "a")) != NULL) {
					for(uint32_t j = 0; j < part.nRows() - 1; j++) {
						fprintf(fd, "%u\n", s16[j]);
					}
					fclose(fd);
				} else {
					delete tablex;
					return NOT_OK;
				}
			} else {
				tablex->addColumn(c->name(), c->type());
				tablex->append(c->name(), 0, part.nRows() - 1, s16);
			}
			break;
		}
		case BYTES_4: {
			std::auto_ptr< ibis::array_t<uint32_t> > tmp(part.selectUInts(c->name(), *bv));
			s32 = tmp->begin();
			if (dumpMode) {
				if ((fd = fopen(path, "a")) != NULL) {
					for(uint32_t j = 0; j < part.nRows() - 1; j++) {
						fprintf(fd, "%u\n", s32[j]);
					}
					fclose(fd);
				} else {
					delete tablex;
					return NOT_OK;
				}
			} else {
				tablex->addColumn(c->name(), c->type());
				tablex->append(c->name(), 0, part.nRows()-1, s32);
			}
			break;}
		case BYTES_8: {
			std::auto_ptr< ibis::array_t<uint64_t> > tmp(part.selectULongs(c->name(), *bv));
			s64 = tmp->begin();
			if (dumpMode) {
				if ((fd = fopen(path, "a")) != NULL) {
					for(uint32_t j = 0; j < part.nRows() - 1; j++) {
						fprintf(fd, "%lu\n", s64[j]);
					}
					fclose(fd);
				} else {
					delete tablex;
					return NOT_OK;
				}
			} else {
				tablex->addColumn(c->name(), c->type());
				tablex->append(c->name(), 0, part.nRows()-1, s64);
			}
			break;}
		}
    }

    if (!dumpMode) {
    	tablex->write(dstDir, 0, 0);
    }
    delete tablex;
    dbg("m_dirs ending");
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
	dbg("m_cpl started");
	DIR *sdir = NULL;
	DIR *ddir = NULL;
	char buff1[DIR_NAME_MAX_LEN];
	char buff2[DIR_NAME_MAX_LEN];
	memset(buff1, 0, DIR_NAME_MAX_LEN);
	memset(buff2, 0, DIR_NAME_MAX_LEN);

	/* Get full paths of folders */
	snprintf(buff1, DIR_NAME_MAX_LEN, "%s/%s", workDir, srcDir);
	snprintf(buff2, DIR_NAME_MAX_LEN, "%s/%s", workDir, dstDir);

	sdir = opendir(buff1);
	if (sdir == NULL) {
		std::cerr << "Error while opening " << buff1 << std::endl;
		return NOT_OK;
	}
	ddir = opendir(buff2);
	if (ddir == NULL) {
		std::cerr << "Error while opening " << buff2 << std::endl;
		closedir(sdir);
		return NOT_OK;
	}

	/* Scan folders in dstDir folder and save them to vector */
	struct dirent *subdir = NULL;
	std::vector<char *> subdirs;
	while ((subdir = readdir(ddir)) != NULL) {
		if ((subdir->d_name[0] == '.') || (subdir->d_type != DT_DIR)) {
			continue;
		}
		subdirs.push_back(subdir->d_name);
	}

	/* Scan folders in srcDir and compare names with of dstDir subfolders */
	while ((subdir = readdir(sdir)) != NULL) {
		if ((subdir->d_name[0] == '.') || (subdir->d_type != DT_DIR)) {
			continue;
		}
		uint8_t found = 0;
		for (uint16_t i = 0; i < subdirs.size(); i++) {
			if (!strcmp(subdirs[i], subdir->d_name)) {
				/* Names are the same -> merge them together */
				snprintf(buff1, DIR_NAME_MAX_LEN, "%s/%s/%s", workDir, srcDir, subdirs[i]);
				snprintf(buff2, DIR_NAME_MAX_LEN, "%s/%s/%s", workDir, dstDir, subdir->d_name);
				if (merge_dirs(buff1, buff2) != OK) {
					return NOT_OK;
				}
				found = 1;
				break;
			}
		}

		/* Folder was not found in dstDir - move it there */
		if (found == 0) {
			snprintf(buff1, DIR_NAME_MAX_LEN, "%s/%s/%s", workDir, srcDir, subdir->d_name);
			snprintf(buff2, DIR_NAME_MAX_LEN, "%s/%s/%s", workDir, dstDir, subdir->d_name);
			if (rename(buff1, buff2) != 0) {
				std::cerr << "Can't mvoe folder " << buff1 << std::endl;
			}
		}
	}

	/* Finally merge flowsStats.txt files */
	snprintf(buff1, DIR_NAME_MAX_LEN, "%s/%s/flowsStats.txt", workDir, srcDir);
	snprintf(buff2, DIR_NAME_MAX_LEN, "%s/%s/flowsStats.txt", workDir, dstDir);

	merge_flowStats(buff1, buff2);

	closedir(sdir);
	closedir(ddir);

	dbg("m_cpl ending");
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
	dbg("m_all started");
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
	char buff1[DIR_NAME_MAX_LEN];
	char buff2[DIR_NAME_MAX_LEN];

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
			snprintf(buff1, DIR_NAME_MAX_LEN, "%s/%s", workDir, subdir->d_name);
			remove_folder_tree(buff1);
		}
	}

	/* Rename folders - reset name values after key to 0 */
	for (std::map<uint32_t, char *>::iterator i = dirMap.begin(); i != dirMap.end(); i++) {
		char *last = (i->second + prefix_len + size);
		if (atoi(last) != 0) {
			snprintf(buff1, DIR_NAME_MAX_LEN, "%s/%s", workDir, i->second);
			memset(last, ASCII_ZERO, DIR_NAME_LEN - size);
			memset(last + DIR_NAME_LEN - size, 0, 1);
			snprintf(buff2, DIR_NAME_MAX_LEN, "%s/%s", workDir, i->second);
			if (rename(buff1, buff2) != 0) {
				std::cerr << "Error while moving folder " << buff1 << std::endl;
			}
		}
	}
	closedir(dir);
	dbg("m_all ending");
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
	dbg("moving started");
	DIR *dir;
	struct dirent *subdir;
	char buff1[DIR_NAME_MAX_LEN];
	char buff2[DIR_NAME_MAX_LEN];
	memset(buff1, 0, DIR_NAME_MAX_LEN);

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
			snprintf(buff1, DIR_NAME_MAX_LEN, "%s/%s", workDir, subdir->d_name);

			/* No prefix? So guess if it is the right folder */
			if (prefix == NULL) {
				if (could_be(subdir->d_name) == OK) {
					/* Separate set - don't move with workDir */
					if (separated) {
						merge_all(workDir, key, prefix);
						break;
					}
					/* Separate NOT set - we can move this dir to baseDir */
					snprintf(buff2, DIR_NAME_MAX_LEN, "%s/%s", baseDir, subdir->d_name);

					if (rename(buff1, buff2) != 0) {
						std::cerr << "Error while moving folder " << buff1 << std::endl;
					}
				} else {
					if (move_prefixed_dirs(baseDir, buff1, prefix, key) != OK) {
						return NOT_OK;
					}
				}
			} else if (strstr(subdir->d_name, prefix) == subdir->d_name) {
				if (separated) {
					merge_all(workDir, key, prefix);
					break;
				}
				snprintf(buff2, DIR_NAME_MAX_LEN, "%s/%s", baseDir, subdir->d_name);
				if (rename(buff1, buff2) != 0) {
					std::cerr << "Error while moving folder " << buff1 << std::endl;
				}
			} else {
				if (move_prefixed_dirs(baseDir, buff1, prefix, key) != OK) {
					return NOT_OK;
				}
			}
		/* Some file was found - move it to baseDir too (if separate is not set) */
		} else if (separated == 0) {
			snprintf(buff1, DIR_NAME_MAX_LEN, "%s/%s", workDir, subdir->d_name);
			snprintf(buff2, DIR_NAME_MAX_LEN, "%s/%s", baseDir, subdir->d_name);
			if (rename(buff1, buff2) != 0) {
				std::cerr << "Error while moving file " << buff1 << std::endl;
			}
		}
	}

	closedir(dir);
	if (!separated) {
		rmdir(workDir);
	}
	dbg("moving ended");
	return OK;
}

int main(int argc, char *argv[]) {
	dbg("program started");
	if (argc <= 1) {
		usage();
		return OK;
	}
	/* Process arguments */
	int option;
	int key = -1;
	int moveOnly = 0;
	char basedir[DIR_NAME_MAX_LEN];
	std::stringstream ss;
	std::string prefix;
	memset(basedir, 0, DIR_NAME_MAX_LEN);
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
			memcpy(basedir, optarg, DIR_NAME_MAX_LEN);
			break;
		case 'p':
			ss << optarg;
			prefix = ss.str();
			break;
		case 's':
			separated = 1;
			break;
		case 'd':
			dumpMode = true;
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
	if (strlen(basedir) == 0) {
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
		if (move_prefixed_dirs(basedir, basedir, (prefix.empty() ? NULL : prefix.c_str()), key) != OK) {
			std::cerr << "Moving folders failed\n";
			return NOT_OK;
		}
		return OK;
	}

	if (key == -1) {
		std::cerr << "\nUndefined key argument!\n\n";
		return NOT_OK;
	}

	if (move_prefixed_dirs(basedir, basedir, (prefix.empty() ? NULL : prefix.c_str()), key) != OK) {
		std::cerr << "Moving folders failed!\n";
		return NOT_OK;
	}
	dbg("directories moved");
	/* if separate not set merge all in basedir */
	if (!separated) {
		dbg("!separated - merge_all");
		merge_all(basedir, key, prefix.c_str());
		dbg("!separated - merged");
	}
	dbg("program ending");
	return OK;
}
