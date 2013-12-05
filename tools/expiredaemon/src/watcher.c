#include <sys/inotify.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <dirent.h>
#include <signal.h>
#include <inttypes.h>

#define NAME_MAX 255
#define BUFFLEN ( 10*(sizeof(struct inotify_event) + NAME_MAX + 1) )

#include "watcher.h"
#include "expire.h"
#include "buffer.h"
#include "delete_queue.h"

void * thread_inotify_func( void * ptr ){
	struct s_data * data = ( struct s_data * ) ptr;
	struct s_directory * dir;
	struct s_directory_inotify * dir_inotify3;
	struct s_directory_inotify * dir_inotify2;
	struct s_directory_inotify * dir_inotify;
	int i;
	char * p;
	char * child_name;
	char buffer[ BUFFLEN ];
	struct inotify_event *event;
	ssize_t read_len;
	FILE * fifo;
	FILE * stat_file;
	int64_t stat_size;
	char * stat_file_name;
	char * stat_file_dir;
	int64_t size = 0;
	struct sigaction action;
	char * ptrc;

	struct dirent * file_info;
	DIR * dirptr;
	
	sigemptyset(&action.sa_mask);
	action.sa_flags = 0;
	action.sa_handler = term_signal_handler;
	
	
	data->inotify_fd = inotify_init();
	
	dir = data->queue_watch->directory;
	while( dir != NULL ) {
		for( i = 0; dir->name[i] != '\0'; i++ ) {
			if( ( dir->name[i] >= '0' ) && ( dir->name[i] <= '9')  ) break;
		}
		
		dir_inotify = buffer_inotify_add_watch( data->queue_inotify, dir->name, 0, NULL, atol( dir->name+i ), data->inotify_fd );
		inotify_scan_init( data, dir->name, 1, dir_inotify );
		
		dir = dir->next;
	}
	while( !done ) {
		if( ( read_len = read( data->inotify_fd, buffer, BUFFLEN ) ) == -1) continue;
		
		fifo = fopen( data->pipe_name, "w");
		// magic
		for (p = buffer; p < buffer + read_len; ) {
			event = (struct inotify_event *) p;
			if( (event->len == 0 )|| (strcmp( event->name, "stat.txt" ) == 0) ) {
				p += sizeof(struct inotify_event) + event->len;
				continue;
			}

			dir_inotify = data->queue_inotify->directory;
			
			while( ( dir_inotify != NULL ) && ( dir_inotify->inotify_wd != event->wd ) ) {
				dir_inotify = dir_inotify->next;
			}
			p += sizeof(struct inotify_event) + event->len;
			
			if( dir_inotify == NULL ) continue;

			if( ( event->mask & IN_ISDIR ) && ( event->mask & IN_CREATE )) { // add new watch and remove old watchers
				if( asprintf( &child_name, "%s/%s", dir_inotify->name, event->name ) == -1 ) {
					ERROR;
					continue;
				}
				
				// get date from directory name
				for( i = 0; event->name[i] != '\0'; i++ ) {
					if( ( event->name[i] >= '0' ) && ( event->name[i] <= '9')  ) break;
				}
				
				// ??? check if directory exists in buffer
				dir_inotify2 = data->queue_inotify->directory;
				while( dir_inotify2 != NULL ) {
					if( strcmp( child_name, dir_inotify2->name ) == 0 ) break;
					dir_inotify2 = dir_inotify2->next;
				}

				if( dir_inotify2 != NULL ) continue;
				
				/*
				 * If we have new directory on watched depth, we will add it to inotify queue, then remove oldest directory
				 * and rescan removed directory. We DO NOT monitor inotify events from directory in inotify queue. That was in first
				 * idea, but it would cause multiple rescan for one directory (for every new or changed file)
             */
				dir_inotify2 = buffer_inotify_add_watch( data->queue_inotify, child_name, dir_inotify->depth+1, dir_inotify, atol(event->name+i), data->inotify_fd );
				
repeat:

				if( dir_inotify2->depth == data->dir_depth ) {
					VERBOSE( 2, "W | New data                     %s\n", child_name);
					dir_inotify2 = NULL;

					while( dir_inotify->parent != NULL ) {
						dir_inotify = dir_inotify->parent;
					}
					dir_inotify2 = dir_inotify;
					// get oldest directory 
					while( dir_inotify2->depth != data->dir_depth ) {
						dir_inotify3 = dir_inotify2;
						dir_inotify2 = buffer_inotify_find_oldest( data->queue_inotify, dir_inotify2 );
						if( dir_inotify2 == NULL ) {
							dir_inotify2 = dir_inotify; 
							buffer_inotify_rm_specific( data->queue_inotify, dir_inotify3, data->inotify_fd );
						}

					}
					// rescan oldest directory
					fprintf(fifo, "%s\n", dir_inotify2->name );
					
					// remove from queue
					pthread_mutex_lock( &data->mutex_mem );
					buffer_inotify_rm_recursive( data->queue_inotify, dir_inotify2, data->inotify_fd );
					pthread_mutex_unlock( &data->mutex_mem );
				}
				else if( dir_inotify2->depth <00 data->dir_depth ) {
					if( asprintf( &stat_file_name, "%s/stat.txt", child_name )== -1 ) {
						ERROR;
						continue;
					}

					stat_file = fopen( stat_file_name, "w" );
					if( stat_file == NULL ) {

						ERROR;
						VERBOSE( 1, "E | %s\n", stat_file_name);
						free( stat_file_name );
						continue;
					}
					fprintf( stat_file, "0" );
					fclose( stat_file );
					free( stat_file_name );

					dirptr = opendir( child_name );
					if( dirptr == NULL ) {
						ERROR;
					}

					errno = 0;
					if( ( file_info = readdir( dirptr ) ) != NULL ) {
						if( asprintf( &child_name, "%s/%s", child_name, file_info->d_name ) == -1 ) {
							ERROR;
							continue;
						}
						for( i = 0; file_info->d_name[i] != '\0'; i++ ) {
							if( ( file_info->d_name[i] >= '0' ) && ( file_info->d_name[i] <= '9')  ) break;
						}
						dir_inotify2 = buffer_inotify_add_watch( data->queue_inotify, child_name, dir_inotify2->depth+1, dir_inotify2, atol(file_info->d_name+i), data->inotify_fd );
						goto repeat;
					}
					else {
						ERROR;
					}
				}
				
				free( child_name );
			}
			
			// remove files
			if( data->total_size >= data->max_size ) {
				while( !done && ( data->total_size >= data->watermark ) ) {
					// check if delete queue is empty
					if( data->queue_delete->directory == NULL ) {
						gen_delete_queue( data );
					}
					
					// get size of directory we want to delete
					if( asprintf( &stat_file_name, "%s/stat.txt", data->queue_delete->directory->name ) == -1 ) {
						ERROR;
						continue;
					}
					stat_file = fopen( stat_file_name, "r" );
					if( stat_file != NULL ) {
						pthread_mutex_lock( &data->mutex_file );
						fscanf( stat_file, "%" SCNd64, &stat_size );
						pthread_mutex_unlock( &data->mutex_file );
						fclose( stat_file );
					}
					else {
						// if we want to delete folder and stat.txt does not exists, we don't know if size of directory was counted in total size
						// so my solution is to set size to 0 and after removing deduct 0
						stat_size = 0;
					}
					
					
					free( stat_file_name );
					
					dir = data->queue_watch->directory;
					while( dir != NULL ) {
						if( strncmp( dir->name, data->queue_delete->directory->name, strlen( dir->name ) ) == 0 ) break;
						
						dir = dir->next;
					}
					stat_file_dir = strdup( data->queue_delete->directory->name );
					
					while( strcmp( dir->name, stat_file_dir ) != 0 ) {
						
						ptrc = strrchr( stat_file_dir, '/' );
						ptrc[0] = '\0';
	//					printf( "\t%s\n", stat_file_dir );

						if( asprintf( &stat_file_name, "%s/stat.txt", stat_file_dir ) == -1 ) {
							ERROR;
							continue;
						}

						stat_file = fopen( stat_file_name, "r" );
						if( stat_file != NULL ) {
							pthread_mutex_lock( &data->mutex_file );
							fscanf( stat_file, "%" SCNd64, &size );
							pthread_mutex_unlock( &data->mutex_file );
							fclose( stat_file );
						}
						else {
							ERROR;
							size = 0;
						}
						size -= stat_size;
						stat_file = fopen( stat_file_name, "w" );
						pthread_mutex_lock( &data->mutex_file );
						fprintf( stat_file, "%" PRId64, size );
						pthread_mutex_unlock( &data->mutex_file );
						fclose( stat_file );
	//					printf( "\tUpdating size in %s\n", stat_file_name );
						free( stat_file_name );
					}
					free( stat_file_dir );
					// cya our beloved data!
					inotify_delete_dir( data->queue_delete->directory->name );
					pthread_mutex_lock( &data->mutex_mem );
					VERBOSE( 1,"D | -%6.2fMB                    %s\n", ((float)stat_size/(1024*1024.0)), data->queue_delete->directory->name );
					data->total_size -= stat_size;
					pthread_mutex_unlock( &data->mutex_mem );
					buffer_rm_dir( data->queue_delete );
				}
			}
		
		}
		fclose( fifo );
		

	}
	
	return NULL;
}

/**
 * @brief Scan newest directories and put inotify watch on them. If directory is on depth we want to watch, it will scan more dirs
 * @param data
 * @param dir_name
 * @param depth
 * @param parent
 */
void inotify_scan_init(struct s_data * data, char * dir_name, int depth, struct s_directory_inotify * parent)
{
	struct s_directory * dir_new;
	struct s_directory * dir;
	struct s_buffer * buffer;

	struct s_directory_inotify * dir_inotify;
	
	int i;
	
	int child_name_number = 0;
	int child_name_number_old = 0;
	
	char * child_name;

	DIR * rdir;
	struct dirent * file;
	
	buffer = buffer_init();

	rdir = opendir( dir_name );
	if( rdir == NULL ) {
		ERROR;
		return;
	}
	while( !done ) {
		file = readdir( rdir );
		
		if( errno && ( file == NULL ) ) {
			ERROR;
			break;
		}
		
		if( file == NULL ) break; // reached end of directory

		// skip "." and ".."
		if( ( strcmp( ".", file->d_name ) == 0 ) ||
			( strcmp( "..", file->d_name ) == 0 )
		  ) {
			continue;
		}
		
		if( asprintf( &child_name, "%s/%s", dir_name, file->d_name ) == -1 ) {
			ERROR;
			continue;
		}
		
		
		if( file->d_type == DT_DIR ) {
			for( i = 0; file->d_name[i] != '\0'; i++ ) {
				if( ( file->d_name[i] >= '0' ) && ( file->d_name[i] <= '9')  ) break;
			}

			child_name_number = atol( file->d_name+i );
			if( child_name_number < 1 ) child_name_number = 1 << (sizeof( long ) -1);
			// if we are on depth we want to watch, make sorted list of directories
			if (depth == data->dir_depth) {
				dir_new = buffer_create_dir( child_name );
				dir_new->data = child_name_number;
				dir = buffer->directory;

				if( dir == NULL ) {
					buffer->directory = dir_new;
				}
				else if ( dir->data > dir_new->data ) {
					dir_new->next = dir;
					buffer->directory = dir_new;
				}
				else {
					// find position
					while( dir->next != NULL ) {
						if( dir->next->data > dir_new->data ) break;
						dir = dir->next;
					}
					dir_new->next = dir->next;
					dir->next = dir_new;
				}
				buffer->count++;
				// if there is more than wanted count of directories, remove oldest
				if( ( buffer->count > data->dir_count ) /*|| ( ( buffer->count + data->actual_watch ) > data->dir_count ) */) {
					buffer_rm_dir( buffer );
				}
			
			// if we are on higher depth than we want, add watch on all directories
			} else if ( depth > data->dir_depth ) {
				// for getting number from directory name
				dir_new = buffer_add_dir( buffer, child_name );
				dir_new->data = child_name_number;
			} else if ( depth < data->dir_depth ){
				// in other ways we add only newest folder
				
				if( child_name_number_old < child_name_number ) {
					child_name_number_old = child_name_number;
					buffer_rm_dir( buffer );
					dir_new = buffer_add_dir( buffer, child_name );
					dir_new->data = child_name_number;
				}
			}
		
			
		}
		free( child_name );
	}
	closedir(rdir);
	
	// ok we have buffer of directories we want to watch, so now we can "inotify" them
	while( buffer->directory != NULL ) {
		if (depth == data->dir_depth) {
			data->actual_watch++;
		}

		dir_inotify = buffer_inotify_add_watch( data->queue_inotify, buffer->directory->name, depth, parent, buffer->directory->data, data->inotify_fd );
		
		
		inotify_scan_init( data, buffer->directory->name, depth+1, dir_inotify );
		buffer_rm_dir( buffer );
	}
	
	// in watched directories there are less subdirectories then we want to watch, usually happend in new day/month/year
/*	if( ( depth == data->dir_depth ) && ( data->actual_watch < data->dir_count ) ) {
		while( data->actual_watch < data->dir_count ) {
			
		}
	}
*/	
	buffer_destruct( buffer );
	
}

/**
 * @brief Recursively deletes directory from disk
 * @param name
 */
void inotify_delete_dir( const char * name ) {
	DIR * rdir;
	struct dirent * file;
	char * child_name;
	
	rdir = opendir( name );
	if( rdir == NULL ) {
		ERROR;
		return;
	}
	while( !done ) {
		file = readdir( rdir );
		
		if( errno && ( file == NULL ) ) {
			ERROR;
			break;
		}
		
		if( file == NULL ) break; // reached end of directory

		// skip "." and ".."
		if( ( strcmp( ".", file->d_name ) == 0 ) ||
			( strcmp( "..", file->d_name ) == 0 )
		  ) {
			continue;
		}
		
		if( asprintf( &child_name, "%s/%s", name, file->d_name ) == -1 ) {
			ERROR;
			continue;
		}
		
		if( file->d_type == DT_DIR ) {
			inotify_delete_dir( child_name );
			
		}
		else {
			errno = 0;
			unlink( child_name );
			if(errno) {
				ERROR;
				continue;
			}
		}
		free( child_name );
	}
	closedir( rdir );
	errno = 0;
	rmdir( name );
	if(errno) {
		ERROR;
	}
}

