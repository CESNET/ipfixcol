/* 
 * File:   buffer.h
 * Author: fuf
 *
 * Created on 25. červenec 2013, 15:29
 */

#ifndef BUFFER_H
#define	BUFFER_H

struct s_buffer {
	int count;
	struct s_directory * directory;
};

struct s_directory {
	char * name;
	uint64_t data;
	int depth;
	struct s_directory * next;
};

struct s_directory * buffer_create_dir( char * name );
struct s_buffer * buffer_init( );
struct s_directory * buffer_add_dir( struct s_buffer * buffer, char * name );
int buffer_rm_dir( struct s_buffer * buffer );
void buffer_destruct( struct s_buffer * buffer );
struct s_directory * buffer_get_last( struct s_buffer * buffer );

struct s_directory_inotify {
	char * name;
	int inotify_wd;
	uint64_t date;
	struct s_directory_inotify * parent;
	struct s_directory_inotify * next;
	int depth;
};

struct s_buffer_inotify {
	struct s_directory_inotify * directory;
};

struct s_buffer_inotify * buffer_inotify_init( );
struct s_directory_inotify * buffer_inotify_add_watch(struct s_buffer_inotify * buffer, char * name, int depth, struct s_directory_inotify * parent, int date, int inotify_fd );
struct s_directory_inotify * buffer_inotify_create_dir( char * name );
int buffer_inotify_rm_dir( struct s_buffer_inotify * buffer );
void buffer_inotify_destruct( struct s_buffer_inotify * buffer );
struct s_directory_inotify * buffer_inotify_get_last( struct s_buffer_inotify * buffer );
struct s_directory_inotify * buffer_inotify_find_bywd ( struct s_buffer_inotify * buffer, int wd );
int buffer_inotify_rm_specific ( struct s_buffer_inotify * buffer, struct s_directory_inotify * what, int fd );
struct s_directory_inotify * buffer_inotify_find_oldest( struct s_buffer_inotify * buffer, struct s_directory_inotify * parent );
struct s_directory_inotify * buffer_inotify_find_latest( struct s_buffer_inotify * buffer, struct s_directory_inotify * parent );
void buffer_inotify_rm_recursive( struct s_buffer_inotify * buffer, struct s_directory_inotify * parent, int fd );
#endif	/* BUFFER_H */

