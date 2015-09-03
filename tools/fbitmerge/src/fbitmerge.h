/**
 * \file fbitmerge.h
 * \author Michal Kozubik
 * \brief Tool for merging fastbit data
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

#ifndef FBITMERGE_H_
#define FBITMERGE_H_

typedef std::map<std::string, int> innerDirMap;
typedef std::map<std::string, innerDirMap> DIRMAP;

enum {
	MAX_SEC = 59,
	MAX_MIN = 59,
	MAX_HOUR = 23,
	MAX_DAY = 31,
	MAX_MONTH = 12,
	CONTROL_BUFF_LEN = 5
};
enum {
	SEC_LEN = 2,
	MIN_LEN = 2,
	HOUR_LEN = 2,
	DAY_LEN = 2,
	MONTH_LEN = 2,
	YEAR_LEN = 4,
	DATE_LEN = 8
};

enum {
	DIR_NAME_LEN = 14,
};

enum {
	ASCII_ZERO = 48
};

enum key {
	YEAR = 0,
	MONTH = 1,
	DAY = 2,
	HOUR = 3
};

enum status {
	OK,
	NOT_OK
};

enum size {
	BYTES_1 = 1,
	BYTES_2 = 2,
	BYTES_4 = 4,
	BYTES_8 = 8
};

void usage();

int merge_all(std::string workDir, uint16_t key, std::string prefix);

int merge_couple(std::string srcDir, std::string dstDir, std::string workDir);

int merge_dirs(std::string srcDir, std::string dstDir);

void merge_flowStats(std::string first, std::string second);

int move_prefixed_dirs(std::string baseDir, std::string workDir, std::string prefix, int key);

void remove_folder_tree(std::string dirname);

#endif /* FBITMERGE_H_ */
