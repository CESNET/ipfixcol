/**
 * \file fbitmerge.cpp
 * \author Michal Kozubik
 * \brief Tool for merging FastBit data
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

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <dirent.h>
#include <errno.h>
#include <getopt.h>
#include <sys/stat.h>
#include <time.h>
#include <utime.h>

#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <fastbit/ibis.h>
#include "fbitmerge.h"

#define OPTSTRING ":hk:b:p:smd"

/** Acceptable command-line parameters (long) */
struct option long_opts[] = {
	{ "help", no_argument, NULL, 'h' },
	{ 0, 0, 0, 0 }
};

static uint8_t separated = 0;

/* \brief Prints help
 */
void usage()
{
	std::cout << "\nUsage: fbitmerge [-hs] -b basedir [-m | -k key] [-p prefix]\n";
	std::cout << "-h\t Show this text\n";
	std::cout << "-b\t Base directory path\n";
	std::cout << "-k\t Merging key (h=hour, d=day, m=month, y=year)\n";
	std::cout << "-p\t Prefix of folders with FastBit data (default = none)\n";
	std::cout << "\t !! If there are prefixed folders but prefix is not set, data from these\n";
	std::cout << "\t    folders may be removed; errors may occur!\n";
	std::cout << "-s\t Separate merging - only prefixed folders can be moved and deleted\n";
	std::cout << "\t It means that their parent folders are merged separately, NOT together\n";
	std::cout << "-m\t Move only - don't merge folders, only move all prefixed subdirs into basedir\n";
	std::cout << std::endl;
}

/* \brief Removes folder dirname with all it's files and subfolders
 *
 * \param[in] dir_name directory path
 */
void remove_folder_tree(std::string dir_name)
{
	DIR *dir;
	struct dirent *subdir;

	dir = opendir(dir_name.c_str());
	if (dir == NULL) {
		std::cerr << "Error while initializing directory '" << dir_name << "'" << std::endl;
		return;
	}

	/* Go through all files and subfolders */
	while ((subdir = readdir(dir)) != NULL) {
		if (subdir->d_name[0] == '.') {
			continue;
		}

		std::string path = dir_name + "/" + subdir->d_name;

		/* If it is a file, remove it. Otherwise, call remove_folder_tree on subfolder */
		if (subdir->d_type == DT_DIR) {
			remove_folder_tree(path);
		} else {
			unlink(path.c_str());
		}
	}

	closedir(dir);
	rmdir(dir_name.c_str());
}

/* \brief Checks if the folder could be YYYYMMDDHHmmSS (if prefix is not set)
 *
 * \param[in] dir_name directory path
 * \return OK on succes, NOT_OK else
 */
int could_be(char *dir_name)
{
	if (strlen(dir_name) != DIR_NAME_LEN) {
		return NOT_OK;
	}

	char buff[CONTROL_BUFF_LEN];
	char *endp;
	int val = 0;

	memcpy(buff, dir_name, YEAR_LEN);
	buff[YEAR_LEN] = '\0';
	val = strtol(buff, &endp, 10);
	if ((*endp != '\0') || (val < 0)) {
		return NOT_OK;
	}

	memcpy(buff, dir_name + YEAR_LEN, MONTH_LEN);
	buff[MONTH_LEN] = '\0';
	val = strtol(buff, &endp, 10);
	if ((*endp != '\0') || (val < 0) || (val > MAX_MONTH)) {
		return NOT_OK;
	}

	memcpy(buff, dir_name + YEAR_LEN + MONTH_LEN, DAY_LEN);
	buff[DAY_LEN] = '\0';
	val = strtol(buff, &endp, 10);
	if ((*endp != '\0') || (val < 0) || (val > MAX_DAY)) {
		return NOT_OK;
	}

	memcpy(buff, dir_name + DATE_LEN, HOUR_LEN);
	buff[HOUR_LEN] = '\0';
	val = strtol(buff, &endp, 10);
	if ((*endp != '\0') || (val < 0) || (val > MAX_HOUR)) {
		return NOT_OK;
	}

	memcpy(buff, dir_name + DATE_LEN + HOUR_LEN, MIN_LEN);
	buff[MIN_LEN] = '\0';
	val = strtol(buff, &endp, 10);
	if ((*endp != '\0') || (val < 0) || (val > MAX_MIN)) {
		return NOT_OK;
	}

	memcpy(buff, dir_name + DATE_LEN + HOUR_LEN + MIN_LEN, SEC_LEN);
	buff[SEC_LEN] = '\0';
	val = strtol(buff, &endp, 10);
	if ((*endp != '\0') || (val < 0) || (val > MAX_SEC)) {
		return NOT_OK;
	}

	return OK;
}

/* \brief Merge two flowsStats files into one (doesn't remove anything)
 *
 * Opens 2 flowsStats.txt files, reads values of exported, received and lost flows
 * and sum of both saves into second file
 *
 * \param[in] first path to the first file
 * \param[in,out] second path to the second file
 * \return OK on succes, NOT_OK else
 */
void merge_flows_stats(std::string first, std::string second)
{
	std::fstream file_f;
	std::fstream file_s;

	file_f.open(first.c_str(), std::fstream::in);
	if (!file_f.is_open()) {
		std::cerr << "Can't open file '" << first << "' for reading\n";
	}

	file_s.open(second.c_str(), std::fstream::in);
	if (!file_s.is_open()) {
		std::cerr << "Can't open file '" << second << "' for reading\n";
		file_f.close();
	}

	int exported_flows = 0;
	int received_flows = 0;
	int lost_flows = 0;
	std::string buff;

	/* Read data from first file */
	if (file_f.is_open()) {
		std::getline(file_f, buff);
		exported_flows += atoi(buff.substr(buff.find(":") + 2, buff.length()).c_str());
		std::getline(file_f, buff);
		received_flows += atoi(buff.substr(buff.find(":") + 2, buff.length()).c_str());
		std::getline(file_f, buff);
		lost_flows += atoi(buff.substr(buff.find(":") + 2, buff.length()).c_str());
		file_f.close();
	}

	/* Add data from second file */
	if (file_s.is_open()) {
		std::getline(file_s, buff);
		exported_flows += atoi(buff.substr(buff.find(":") + 2, buff.length()).c_str());
		std::getline(file_s, buff);
		received_flows += atoi(buff.substr(buff.find(":") + 2, buff.length()).c_str());
		std::getline(file_s, buff);
		lost_flows += atoi(buff.substr(buff.find(":") + 2, buff.length()).c_str());
		file_s.close();
	}

	/* Save data into second file */
	file_s.open(second.c_str(), std::fstream::out | std::fstream::trunc);
	if (!file_s.is_open()) {
		std::cerr << "Cannot open file '" << second << "' for writing\n";
	} else {
		file_s << "Exported flows: " << exported_flows << std::endl;
		file_s << "Received flows: " << received_flows << std::endl;
		file_s << "Lost flows: " << lost_flows << std::endl;
		file_s.close();
	}
}

/* \brief Merges 2 folders containing FastBit data together (into second folder)
 *
 * \param[in] src_dir source folder path
 * \param[in,out] dst_dir destination folder path
 * \return OK on succes, NOT_OK else
 */
int merge_dirs(std::string src_dir, std::string dst_dir)
{
	/* Table initialization */
	ibis::part part(dst_dir.c_str(), static_cast<const char*>(0));

	/* If there are no rows, we have nothing to do */
	if (part.nRows() == 0) {
		return OK;
	}

	if (part.append(src_dir.c_str()) < 0) {
		return NOT_OK;
	}

	part.commit(dst_dir.c_str());
	return OK;
}

void scan_dir(std::string dir_name, std::string src_dir, DIRMAP *bigMap)
{
	ibis::part part(src_dir.c_str(), static_cast<const char*>(0));

	if (part.nRows() == 0) {
		return;
	}

	for (uint32_t i = 0; i < part.nColumns(); i++) {
		ibis::column *c = part.getColumn(i);
		(*bigMap)[dir_name][std::string(c->name())] = c->type();
	}
}

int same_data(innerDirMap *first, innerDirMap *second)
{
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

time_t get_file_atime(std::string path)
{
	struct stat file_stat;
	if (stat(path.c_str(), &file_stat) < 0) {
		std::cerr << "Could not retrieve file attributes for '" << path << "'" << std::endl;
		return 0;
	}

	return file_stat.st_atime;
}

time_t get_file_mtime(std::string path)
{
	struct stat file_stat;
	if (stat(path.c_str(), &file_stat) < 0) {
		std::cerr << "Could not retrieve file attributes for '" << path << "'" << std::endl;
		return 0;
	}

	return file_stat.st_mtime;
}

/* \brief Merges 2 folders in format <prefix>YYYYMMDDHHmmSS together
 *
 * Goes through subfolders of destination folder and saves their names. Then
 * goes through subfolders of source folder and compare folder names
 * with first folder. Same folders are merged together (merge_dirs). If
 * folder doesn't exist in dst_dir, it is moved there.
 *
 * \param[in] src_dir source folder name
 * \param[in,out] dst_dir destination folder name
 * \param[in] work_dir parent directory
 * \return OK on success, NOT_OK else
 */
int merge_couple(std::string src_dir, std::string dst_dir, std::string work_dir)
{
	DIR *sdir = NULL;
	DIR *ddir = NULL;

	/* Get full paths of folders */
	std::string src_dir_path = work_dir + "/" + src_dir;
	std::string dst_dir_path = work_dir + "/" + dst_dir;

	/* Open folders */
	sdir = opendir(src_dir_path.c_str());
	if (sdir == NULL) {
		std::cerr << "Error while opening '" << src_dir_path << "': " << strerror(errno) << std::endl;
		return NOT_OK;
	}

	ddir = opendir(dst_dir_path.c_str());
	if (ddir == NULL) {
		std::cerr << "Error while opening '" << dst_dir_path << "': " << strerror(errno) << std::endl;
		closedir(sdir);
		return NOT_OK;
	}

	/* Scan folders in dst_dir folder and save them to DIRMAP */
	struct dirent *subdir = NULL;
	DIRMAP src_map, dst_map;
	while ((subdir = readdir(ddir)) != NULL) {
		if ((subdir->d_name[0] == '.') || (subdir->d_type != DT_DIR)) {
			continue;
		}

		scan_dir(subdir->d_name, dst_dir_path + "/" + subdir->d_name, &dst_map);
	}

	/* Now do the same with src_dir */
	while ((subdir = readdir(sdir)) != NULL) {
		if ((subdir->d_name[0] == '.') || (subdir->d_type != DT_DIR)) {
			continue;
		}

		scan_dir(subdir->d_name, src_dir_path + "/" + subdir->d_name, &src_map);
	}

	/* Iterate through whole dst_map and src_map and find folders with same data (and data types) */
	for (DIRMAP::iterator dst_i = dst_map.begin(); dst_i != dst_map.end(); ++dst_i) {
		for (DIRMAP::iterator src_i = src_map.begin(); src_i != src_map.end(); ) {
			if (same_data(&((*dst_i).second), &((*src_i).second)) == OK) {
				/* If found, merge it */
				if (merge_dirs(src_dir_path + "/" + (*src_i).first,
						dst_dir_path + "/" + (*dst_i).first) != OK) {
					closedir(sdir);
					closedir(ddir);
					return NOT_OK;
				}

				/* We don't need this folder anymore */
				src_i = src_map.erase(src_i);
			} else {
				++src_i;
			}
		}
	}

	/* If there is some folder that wasn't merged, just move it to dst_dir */
	/* But we must find unused folder name for it (we dont wan't to rewrite some other folder */
	for (DIRMAP::iterator src_i = src_map.begin(); src_i != src_map.end(); ++src_i) {
		struct stat st;
		char suffix = 'a';

		/* Add suffix to the name */
		while (stat((dst_dir_path + "/" + (*src_i).first).c_str(), &st) == 0) {
			if (suffix == 'z') {
				suffix = 'A';
			} else if (suffix == 'Z') {
				/* \TODO do it better */
				std::cerr << "Not enough suffixes for folder '" << (*src_i).first << "'" << std::endl;
				break;
			} else {
				suffix++;
			}
		}

		if (rename((src_dir_path + "/" + (*src_i).first).c_str(),
				(dst_dir_path + "/" + (*src_i).first).c_str()) != 0) {
			std::cerr << "Cannot rename folder '" << (src_dir_path + "/" + (*src_i).first) << "'" << std::endl;
		}
	}

	/* Now merge template directories with same data (and data types) in dst_dir */
	for (DIRMAP::iterator dst_i = dst_map.begin(); dst_i != dst_map.end(); ) {
		for (DIRMAP::iterator it = dst_i; it != dst_map.end(); ) {
			if (it == dst_i) {
				++it;
				continue;
			}

			if (same_data(&((*dst_i).second), &((*it).second)) == OK) {
				if (merge_dirs(dst_dir_path + "/" + (*it).first,
						dst_dir_path + "/" + (*dst_i).first) != OK) {
					closedir(sdir);
					closedir(ddir);
					return NOT_OK;
				}

				remove_folder_tree((dst_dir_path + "/" + (*it).first).c_str());
				it = dst_map.erase(it);
			}

			++it;
		}

		++dst_i;
	}

	/* Finally merge flowsStats.txt files */
	merge_flows_stats(src_dir_path + "/" + "flowsStats.txt",
			dst_dir_path + "/" + "flowsStats.txt");

	closedir(sdir);
	closedir(ddir);

	return OK;
}

/* \brief Goes through folder containing prefixed subfolders and merges them together by key
 *
 * Goes through work_dir subfolders and looks at key values.
 * Folders with same key values are merged together.
 *
 * \param[in] work_dir folder with prefixed subfolders
 * \param[in] key key value
 * \param[in] prefix folder prefix
 * \return OK on success, NOT_OK else
 */
int merge_all(std::string work_dir, uint16_t key, std::string prefix)
{
	DIR *dir = NULL;
	dir = opendir(work_dir.c_str());
	if (dir == NULL) {
		std::cerr << "Error while initializing directory '" << work_dir << "'" << std::endl;
		return NOT_OK;
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
		std::cerr << "Undefined key value\n";
		closedir(dir);
		return NOT_OK;
	}

	/* Go through subdirs */
	std::map<uint32_t, std::string> dir_map;
	std::map<uint32_t, time_t> dir_map_max_mtime;
	struct dirent *subdir = NULL;
	char key_str[size + 1];
	std::string full_subdir_path;

	while ((subdir = readdir(dir)) != NULL) {
		if (subdir->d_name[0] == '.') {
			continue;
		}

		/* Get key value */
		memset(key_str, 0, size + 1);
		memcpy(key_str, subdir->d_name + prefix.length(), size);
		uint32_t key_int = atoi(key_str);

		/* Get mtime */
		full_subdir_path = work_dir + "/" + subdir->d_name;
		time_t dir_mtime = get_file_mtime(full_subdir_path);

		/* If it is the first occurrence of the key, store it in the map.
		 * Otherwise, merge the two folders. */
		if (dir_map.find(key_int) == dir_map.end()) {
			dir_map[key_int] = subdir->d_name;
			dir_map_max_mtime[key_int] = dir_mtime;
		} else {
			/* Check whether mtime is larger than the one stored in map. If so,
			 * update it. */
			if (dir_mtime > dir_map_max_mtime[key_int]) {
				dir_map_max_mtime[key_int] = dir_mtime;
			}

			/* Merge data */
			if (merge_couple(subdir->d_name, dir_map[key_int], work_dir) != OK) {
				closedir(dir);
				return NOT_OK;
			}

			/* Remove merged src folder */
			remove_folder_tree(full_subdir_path);
		}
	}

	/* Rename folders, if necessary - reset name values after key to 0. Also
	 * update folder mtime. */
	for (std::map<uint32_t, std::string>::iterator i = dir_map.begin(); i != dir_map.end(); i++) {
		if (prefix.length() + size > i->second.length()) {
			std::cerr << "Error while preparing to rename folder '" << i->second << \
					"': folder name shorther than expected" << std::endl;
			continue;
		}

		std::string first = i->second.substr(0, prefix.length() + size);
		std::string last = i->second.substr(prefix.length() + size, std::string::npos);

		std::string from_path = work_dir + "/" + i->second;
		std::string to_path = work_dir + "/" + first + last;
		
		if (stoi(last) != 0) {
			/* Update to_path */
			last.assign(last.length(), '0');
			to_path = work_dir + "/" + first + last;

			if (rename(from_path.c_str(), to_path.c_str()) != 0) {
				std::cerr << "Error while renaming folder '" << (first + last) << "'" << std::endl;
				continue;
			}
		}

		/* Set mtime back in time, to the last (maximum) mtime of subfolders */
		struct utimbuf new_file_times;
		new_file_times.actime = get_file_atime(to_path);

		/* Check whether entry is present in dir_map_max_mtime. This check should
		 * never fail, since dir_map and dir_map_max_mtime should always be in sync */
		if (dir_map_max_mtime.find(i->first) == dir_map_max_mtime.end()) {
			std::cerr << "No maximum mtime known for '" << to_path << "'" << std::endl;
			continue;
		}

		new_file_times.modtime = dir_map_max_mtime[i->first];
		if (utime(to_path.c_str(), &new_file_times) < 0) {
			std::cerr << "Could not update mtime of '" << to_path << "'" << std::endl;
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
 * \param[in] base_dir base folder where all prefixed folders will be moved
 * \param[in] work_dir actual directory
 * \param[in] prefix prefix
 * \param[in] key key value
 */
int move_prefixed_dirs(std::string base_dir, std::string work_dir, std::string prefix, int key)
{
	DIR *dir;
	struct dirent *subdir;

	dir = opendir(work_dir.c_str());
	if (dir == NULL) {
		std::cerr << "Error while initializing directory '" << work_dir << "'" << std::endl;
		return NOT_OK;
	}

	/* Cycle through all files subfolders */
	while ((subdir = readdir(dir)) != NULL) {
		if (subdir->d_name[0] == '.') {
			continue;
		}

		std::string src_dir_path = work_dir + "/" + subdir->d_name;
		std::string dst_dir_path = base_dir + "/" + subdir->d_name;

		if (subdir->d_type == DT_DIR) {
			/* No prefix? So guess if it is the right folder */
			if (prefix.empty()) {
				if (could_be(subdir->d_name) == OK) {
					/* Separate set - don't move with work_dir */
					if (separated) {
						if (merge_all(work_dir, key, prefix) != OK) {
							closedir(dir);
							return NOT_OK;
						}

						break;
					}

					/* Separate NOT set - we can move this dir to base_dir */
					if (rename(src_dir_path.c_str(), dst_dir_path.c_str()) != 0) {
						std::cerr << "Error while moving folder '" << src_dir_path << "'" << std::endl;
					}
				} else {
					if (move_prefixed_dirs(base_dir, src_dir_path, prefix, key) != OK) {
						closedir(dir);
						return NOT_OK;
					}
				}
			} else if (strstr(subdir->d_name, prefix.c_str()) == subdir->d_name) {
				if (separated) {
					if (merge_all(work_dir, key, prefix) != OK) {
						closedir(dir);
						return NOT_OK;
					}

					break;
				}

				if (rename(src_dir_path.c_str(), dst_dir_path.c_str()) != 0) {
					std::cerr << "Error while moving folder '" << src_dir_path << "'" << std::endl;
				}
			} else {
				if (move_prefixed_dirs(base_dir, src_dir_path, prefix, key) != OK) {
					closedir(dir);
					return NOT_OK;
				}
			}

		/* Some file was found - move it to base_dir too (if separate is not set) */
		} else if (separated == 0) {
			if (rename(src_dir_path.c_str(), dst_dir_path.c_str()) != 0) {
				std::cerr << "Error while moving file '" << src_dir_path << "'" << std::endl;
			}
		}
	}

	closedir(dir);
	if (!separated) {
		rmdir(work_dir.c_str());
	}

	return OK;
}

int main(int argc, char *argv[])
{
	if (argc <= 1) {
		usage();
		return OK;
	}

	ibis::gVerbose = -10;

	/* Process arguments */
	int option;
	int key = -1;
	int moveOnly = 0;

	std::string base_dir;
	std::stringstream ss;
	std::string prefix;

	while ((option = getopt_long(argc, argv, OPTSTRING, long_opts, NULL)) != -1) {
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
			base_dir = ss.str();
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
			std::cerr << "Unknown argument: " << (char) optopt << std::endl;
			usage();
			return NOT_OK;
		case ':':
			std::cerr << "Missing parameter for argument '" << (char) optopt << "'" << std::endl;
			return NOT_OK;
		default:
			std::cerr << "Unknown error\n";
			break;
		}
	}

	if (base_dir.empty()) {
		std::cerr << "\nBase directory path not set\n\n";
		return NOT_OK;
	}

	if (prefix.empty()) {
		std::cout << "\nWarning: Prefix not set\n\n";
	}

	/* Move all prefixed subdirs into base_dir (or merge_all if separated set) */
	if (moveOnly != 0) {
		if (separated) {
			std::cerr << "-s and -m arguments can't be used together\n";
			return NOT_OK;
		}

		if (move_prefixed_dirs(base_dir, base_dir, prefix, key) != OK) {
			std::cerr << "Moving folders failed\n";
			return NOT_OK;
		}

		return OK;
	}

	if (key == -1) {
		std::cerr << "\nUndefined key argument\n\n";
		return NOT_OK;
	}

	if (move_prefixed_dirs(base_dir, base_dir, prefix, key) != OK) {
		std::cerr << "Moving folders failed\n";
		return NOT_OK;
	}

	/* if separate not set merge all in base_dir */
	if (!separated) {
		if (merge_all(base_dir, key, prefix) != OK) {
			return NOT_OK;
		}
	}

	return OK;
}
