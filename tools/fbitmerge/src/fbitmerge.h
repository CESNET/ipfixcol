/*
 * fbitmerge.h
 *
 *  Created on: 16.8.2013
 *      Author: michal
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

int merge_all(const char *workDir, uint16_t key, char *prefix);

int merge_couple(const char *srcDir, const char *dstDir, const char *workDir);

int merge_dirs(const char *srcDir, const char *dstDir);

void merge_flowStats(const char *first, const char *second);

int move_prefixed_dirs(const char *baseDir, const char *workDir, char *prefix, int key);

void remove_folder_tree(const char *dirname);


#endif /* FBITMERGE_H_ */
