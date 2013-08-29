/* 
 * File:   watch.h
 * Author: fuf
 *
 * Created on 13. srpen 2013, 10:30
 */

#ifndef WATCH_H
#define	WATCH_H

#include "buffer.h"
#include "expire.h"

void * thread_inotify_func( void * ptr );
void inotify_scan_init(struct s_data * data, char * dir_name, int depth, struct s_directory_inotify * parent);
void inotify_delete_dir( const char * name );
#endif	/* WATCH_H */

