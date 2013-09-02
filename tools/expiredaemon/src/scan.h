/* 
 * File:   scan.h
 * Author: fuf
 *
 * Created on 26. červenec 2013, 11:41
 */

#ifndef SCAN_H
#define	SCAN_H

#include "expire.h"
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
// file to ignore
/*static const char *ignore[] = {
	"stat.txt"
};*/

void * thread_rescan_func( void * ptr );
int64_t scan_dir( struct s_data * data, char * dir_name );

#endif	/* SCAN_H */

