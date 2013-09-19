/**
 * \file Utils.cpp
 * \author Petr Velan <petr.velan@cesnet.cz>
 * \brief Auxiliary functions definitions
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

#include "Utils.h"
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <sys/ioctl.h>

#define PROGRESSBAR_SIZE 50

namespace fbitdump {
namespace Utils {


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
//	std::cout.fill( ' ');
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
	if (path[path.length()-1] != '/') {
		path += "/";
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

	bool firstDirFound = false; /* indicates whether we already found first specified dir */
	int counter = 0;
	struct dirent *dent;
	bool onlyFreeDirs = false; /* call free on all dirents without adding the directories */

	while(dirs_counter--) {
		dent = namelist[counter++];

		if (dent->d_type == DT_DIR && strcmp(dent->d_name, ".")
		&& strcmp(dent->d_name, "..") && !onlyFreeDirs) {
			if (firstDirFound || !strcmp(dent->d_name, firstDir.c_str())) {

				firstDirFound = true;
				std::string tableDir = dir + dent->d_name;
				Utils::sanitizePath(tableDir);
				tables.push_back(std::string(tableDir));

				if (!strcmp(dent->d_name, lastDir.c_str())) {
					/* this is last directory we are interested in */
					onlyFreeDirs = true;
				}
			}
		}

		free(namelist[counter-1]);
	}
	free(namelist);
}

} /* end of namespace utils */

}  /* end of namespace fbitdump */
