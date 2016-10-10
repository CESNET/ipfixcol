/**
 * \file Utils.cpp
 * \author Petr Velan <petr.velan@cesnet.cz>
 * \brief Auxiliary functions definitions
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

#include "Utils.h"
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <sys/ioctl.h>

#define PROGRESSBAR_SIZE 50

namespace fbitdump {
namespace Utils {

void printStatus( std::string status ) {
	static struct winsize w;
	static int ok = 0;
	if(!isatty(fileno(stdout)))
		return;
	if( ok == 0 ) {
		ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
		ok = 1;
	}
	std::cout << status << "...";
	std::cout.width(w.ws_col-(status.size()+3));
	std::cout.fill( ' ');
	std::cout << "\r";
	std::cout.flush();
}

void progressBar(std::string prefix, std::string suffix, int max, int actual) {
	static struct winsize w;
	static int ok = 0;
	if(!isatty(fileno(stdout)))
		return;
	if( ok == 0 ) {
		ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
		ok = 1;
	}
	float progress = ((double)actual/max);

	std::cout << prefix << "[";
	int pos = PROGRESSBAR_SIZE * progress;
	for (int x = 0; x < PROGRESSBAR_SIZE; ++x) {
		if (x < pos) std::cout << "=";
		else if (x == pos) std::cout << ">";
		else std::cout << " ";
	}
	std::cout << "] " << int(progress * 100.0) << " % " << suffix;
	std::cout.width(w.ws_col-(PROGRESSBAR_SIZE+prefix.size()+suffix.size()+7));
	std::cout.fill( ' ');
	std::cout << "\r";
	

//	std::cout << "!!!" << env << "!!!" << std::endl << std::endl;
	std::cout.flush();
}

/**
 * \brief Splits string into different tokens by comma
 *
 * @param str string to split
 * @param result stringSet to put result into
 * @return true on success, false otherwise
 */
bool splitString(char *str, stringSet &result)
{
	char *token;

	if (str == NULL) {
		return false;
	}

	token = strtok(str, ",");
	/* NULL token cannot be added to result */
	if (token == NULL) {
		return true;
	}

	result.insert(token);
	while ((token = strtok(NULL, ",")) != NULL) {
		result.insert(token);
	}

	return true;
}

bool isFastbitPart(std::string dir)
{
	return !access((dir + "-part.txt").c_str(), F_OK);
}

void sanitizePath(std::string &path)
{
	if (path.back() != '/') {
		path += "/";
	}
}

std::string rootDir(std::string dir)
{
	size_t pos = dir.find('/');
	if (pos != dir.npos) {
		/* Eliminate subdirs */
		return dir.substr(0, pos);
	}

	/* No subdirs */
	return dir;
}

void loadDirsTree(std::string basedir, std::string first, std::string last, stringVector &tables)
{
	struct dirent **namelist;
	int dirs_counter;

	sanitizePath(basedir);
	
	/* Find root directories */
	std::string root_first = rootDir(first);
	std::string root_last = rootDir(last);

	/* scan for subdirs */
	dirs_counter = scandir(basedir.c_str(), &namelist, NULL, versionsort);
	if (dirs_counter < 0) {
#ifdef DEBUG
		std::cerr << "Cannot stat directory " << basedir << ": " << strerror(errno) << std::endl;
#endif
		return;
	}

	/* Add all directories into vector */
	for (int i = 0; i < dirs_counter; ++i) {
		std::string entry_name = namelist[i]->d_name;

		/* Ignore . and .. */
		if (entry_name == "." || entry_name == "..") {
			continue;
		}

		/* If first dir was given, ignore entries before it */
		if (!root_first.empty() && strverscmp(entry_name.c_str(), root_first.c_str()) < 0) {
			continue;
		} else if (strverscmp(entry_name.c_str(), root_first.c_str()) == 0) {
			if (root_first == first.substr(0, first.length() - 1)) {
				/* Found first folder */
				std::string tableDir = basedir + entry_name;
				sanitizePath(tableDir);
				tables.push_back(tableDir);
			} else {
				/* Go deeper and find first folder */
				std::string new_basedir = basedir + entry_name;
				std::string new_first = first.substr(root_first.length() + 1);
				loadDirsTree(new_basedir, new_first, "", tables);
			}
		} else if (root_last.empty() || strverscmp(entry_name.c_str(), root_last.c_str()) < 0) {
			/* Entry is between first and last */
			std::string tableDir = basedir + entry_name;
			sanitizePath(tableDir);
			tables.push_back(tableDir);
		} else if (strverscmp(entry_name.c_str(), root_last.c_str()) == 0){
			/* Entry == root_last */
			if (root_last == last.substr(0, last.length() - 1)) {
				/* We're on last level, add last directory to vector */
				std::string tableDir = basedir + entry_name;
				sanitizePath(tableDir);
				tables.push_back(tableDir);
			} else {
				/* Goo deeper */
				std::string new_basedir = basedir + entry_name;
				std::string new_last = last.substr(root_last.length() + 1);
				loadDirsTree(new_basedir, "", new_last, tables);
			}
		}
	}

}

void loadDirRange(std::string &dir, std::string &firstDir, std::string &lastDir, stringVector &tables)
	throw (std::invalid_argument)
{
	/* remove slash, if any */
	if (firstDir[firstDir.length()-1] == '/') {
		firstDir.resize(firstDir.length()-1);
	}
	if (lastDir[lastDir.length()-1] == '/') {
		lastDir.resize(lastDir.length()-1);
	}

	/* check that first dir comes before last dir */
	if (strverscmp(firstDir.c_str(), lastDir.c_str()) > 0) {
		throw std::invalid_argument(lastDir + " comes before " + firstDir);
	}

	struct dirent **namelist;
	int dirs_counter;

	/* scan for subdirectories */
	dirs_counter = scandir(dir.c_str(), &namelist, NULL, versionsort);
	if (dirs_counter < 0) {
#ifdef DEBUG
		std::cerr << "Cannot scan directory " << dir << ": " << strerror(errno) << std::endl;
#endif
		return;
	}
	/*
	 * namelist now contains dirent structure for every entry in directory.
	 * the structures are sorted according to versionsort, which is ok for most cases
	 */

	int counter = 0;
	struct dirent *dent;

	while(dirs_counter--) {
		dent = namelist[counter++];

		/* Check that the directory is in range and not '.' or '..' */
		if (dent->d_type == DT_DIR && strcmp(dent->d_name, ".") && strcmp(dent->d_name, "..") &&
			strverscmp(dent->d_name, firstDir.c_str()) >= 0 && strverscmp(dent->d_name, lastDir.c_str()) <= 0) {

			std::string tableDir = dir + dent->d_name;
			Utils::sanitizePath(tableDir);
			tables.push_back(std::string(tableDir));
		}

		free(namelist[counter-1]);
	}
	free(namelist);
}

/**
 * \brief Version of strncpy that ensures null-termination.
 */
char *strncpy_safe (char *destination, const char *source, size_t num)
{
	strncpy(destination, source, num);

	// Ensure null-termination
	destination[num - 1] = '\0';

	return destination;
}

/**
 * \brief Version of strtol with proper error-handling.
 *
 * \return Converted integer value of the supplied String, INT_MAX otherwise.
 */
int strtoi(const char* str, int base)
{
	char *end;
	errno = 0;

	if (str == NULL) {
		return INT_MAX;
	}

	const long ret_long = strtol(str, &end, base);
	int ret_int;

	if (end == str) { // String does not feature a valid number
		ret_int = INT_MAX;
	} else if ((ret_long <= INT_MIN || ret_long >= INT_MAX) && errno == ERANGE) { // Number is out of range
		ret_int = INT_MAX;
	} else {
		ret_int = (int) ret_long;
	}

	return ret_int;
}

} /* end of namespace utils */

}  /* end of namespace fbitdump */
