#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/inotify.h>
#include <limits.h>
#include <stdint.h>

#include "buffer.h"
/**
 * Initializes new empty buffer queue
 * @return pointer for new buffer
 */
struct s_buffer * buffer_init( ) {
	struct s_buffer * buffer;
	
	buffer = malloc( sizeof( struct s_buffer ) );
	if( buffer == NULL ) {
		fprintf( stderr, "Malloc error" );
		return NULL;
	}
	
	buffer->count = 0;
	buffer->directory = NULL;
	
	return buffer;
}

/**
 * Add directory to the end of queue
 * @param buffer pointer for buffer
 * @param name Directory name
 * @return pointer on added directory
 */
struct s_directory * buffer_add_dir(struct s_buffer * buffer, char * name)
{
	struct s_directory * dir;
	struct s_directory * idir;

	dir = buffer_create_dir(name);
	
	if (buffer->count == 0) {
		buffer->directory = dir;
	} else {
		idir = buffer->directory;
		
		while (idir->next != NULL) {
			idir = idir->next;
		}
		
		idir->next = dir;
	}
	
	buffer->count++;
	return dir;
}


/**
 * Allocate directory entry
 * @param name
 * @return pointer do entry
 */
struct s_directory * buffer_create_dir( char * name ) {
	struct s_directory * dir;
    
	dir = malloc( sizeof( struct s_directory ) );
	if( dir == NULL ) {
		fprintf( stderr, "Malloc error" );
		return NULL;
	}
	
	dir->name = strdup( name );
	dir->next = NULL;
	dir->data = 0;

	return dir;
}

/**
 * Removes directory from begin of queue
 * @param buffer pointer for buffer
 * @return 0 if there are no directories in queue
 */
int buffer_rm_dir( struct s_buffer * buffer ) {
	struct s_directory * dir = buffer->directory;
	
	if( buffer->directory == NULL ) return 0;
	buffer->directory = dir->next;
	free( dir->name );
	free( dir );
	buffer->count--;
	
	return 1;
}

/**
 * Frees buffer
 * @param buffer
 */
void buffer_destruct( struct s_buffer * buffer ) {
	while( buffer_rm_dir( buffer ) );
	
	free( buffer );
}

struct s_directory * buffer_get_last( struct s_buffer * buffer ) {
	struct s_directory * dir = buffer->directory;
	
	if( dir == NULL ) return NULL;
	
	while( dir->next != NULL ) {
		dir = dir->next;
	}
	
	return dir;
}


/**
 * Buffer functions for inotify
 */
struct s_buffer_inotify * buffer_inotify_init( ) {
	struct s_buffer_inotify * buffer;
	
	buffer = malloc( sizeof( struct s_buffer_inotify ) );
	if( buffer == NULL ) {
		fprintf( stderr, "Malloc error" );
		return NULL;
	}

	buffer->directory = NULL;
	
	return buffer;
}

struct s_directory_inotify * buffer_inotify_add_watch(struct s_buffer_inotify * buffer, char * name, int depth, struct s_directory_inotify * parent, int date, int inotify_fd )
{
	struct s_directory_inotify * dir = NULL;
	struct s_directory_inotify * idir = NULL;

	dir = buffer_inotify_create_dir(name);
	dir->depth = depth;
	dir->parent = parent;
	dir->date = date;
	dir->inotify_wd = inotify_add_watch( inotify_fd, name, IN_CREATE );
	
	if (buffer->directory == NULL) {
		buffer->directory = dir;
	} else {
		idir = buffer->directory;
		
		while (idir->next != NULL) {
			idir = idir->next;
		}
		
		idir->next = dir;
	}
	
	return dir;
}

struct s_directory_inotify * buffer_inotify_create_dir( char * name ) {
	struct s_directory_inotify * dir;
    
	dir = malloc( sizeof( struct s_directory_inotify ) );
	if( dir == NULL ) {
		fprintf( stderr, "Malloc error" );
		return NULL;
	}
	
	dir->name = strdup( name );
	dir->next = NULL;

	return dir;
}


int buffer_inotify_rm_dir( struct s_buffer_inotify * buffer ) {
	struct s_directory_inotify * dir = buffer->directory;
	
	if( buffer->directory == NULL ) return 0;
	buffer->directory = dir->next;
	free( dir->name );
	free( dir );
	
	return 1;
}

void buffer_inotify_destruct( struct s_buffer_inotify * buffer ) {
	while( buffer_inotify_rm_dir( buffer ) );
	
	free( buffer );
}

struct s_directory_inotify * buffer_inotify_get_last( struct s_buffer_inotify * buffer ) {
	struct s_directory_inotify * dir = buffer->directory;
	if( dir == NULL) return dir;
	while( dir->next != NULL ) {
		dir = dir->next;
	}
	
	return dir;
}

struct s_directory_inotify * buffer_inotify_find_bywd ( struct s_buffer_inotify * buffer, int wd ) {
	struct s_directory_inotify * dir;
	
	dir = buffer->directory;
	while( dir != NULL ) {
		if( dir->inotify_wd == wd ) break;
		
		dir = dir->next;
	}
	
	return dir;
}

int buffer_inotify_rm_specific ( struct s_buffer_inotify * buffer, struct s_directory_inotify * what, int fd ) {
	struct s_directory_inotify * dir;
	
	dir = buffer->directory;
	
	while( dir != NULL ) {
		if( dir->next == what ) break;
		dir = dir->next;
	}
	
	if( dir == NULL ) {
		return 0;
	}
	
	dir->next = what->next;

	inotify_rm_watch( fd, what->inotify_wd );
	
	free( what->name );
	free( what );
	
	return 1;
}
// find oldest directory of parent
struct s_directory_inotify * buffer_inotify_find_oldest( struct s_buffer_inotify * buffer, struct s_directory_inotify * parent ) {
	struct s_directory_inotify * dir;
	struct s_directory_inotify * oldest = NULL;
	uint64_t date = ULLONG_MAX;
	
	dir = buffer->directory;
	if( dir == NULL ) return NULL;
	while( dir != NULL ) {
		if( (dir->parent == parent ) && ( dir->date < date ) ) {
			date = dir->date;
			oldest = dir;
		}
		dir = dir->next;
	}
	
	return oldest;
	
}

// find latest directory of parent
struct s_directory_inotify * buffer_inotify_find_latest( struct s_buffer_inotify * buffer, struct s_directory_inotify * parent ) {
	struct s_directory_inotify * dir;
	struct s_directory_inotify * oldest;
	
	dir = buffer->directory;
	oldest = dir;
	while( dir != NULL ) {
		if( ( dir->parent == parent ) && ( dir->date > oldest->date ) ) oldest = dir;
		dir = dir->next;
	}
	
	return oldest;
	
}

void buffer_inotify_rm_recursive( struct s_buffer_inotify * buffer, struct s_directory_inotify * parent, int fd ) {
	struct s_directory_inotify * dir;
	struct s_directory_inotify * next_dir;
	dir = buffer->directory;
	
	while( dir != NULL ) {
		next_dir = dir->next;
		if( dir->parent == parent ) {
			
			buffer_inotify_rm_recursive( buffer, dir, fd );
		}
		
		dir = next_dir;
	}
	buffer_inotify_rm_specific( buffer, parent, fd );
}
