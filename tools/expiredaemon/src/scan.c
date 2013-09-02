#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <inttypes.h>

#include "expire.h"
#include "scan.h"

void * thread_rescan_func( void * ptr ) {
	struct s_data * data = ( struct s_data * ) ptr;
	struct s_directory * dir;
	char * dir_name;
	char * stat_file_dir;
	char * ptrc;
	FILE * stat_file;
	int rescan;
	int64_t size=0;
	int64_t size2=0;
	struct sigaction action;
	
	sigemptyset(&action.sa_mask);
	action.sa_flags = 0;
	action.sa_handler = term_signal_handler;
	sigaction(SIGUSR1, &action, NULL);
	
	dir = data->queue_watch->directory;
	while( dir != NULL ) {
		rescan = data->force_rescan;
		if( asprintf( &dir_name, "%s/stat.txt", dir->name ) == -1 ) {
			ERROR;
			continue;
		}
		
		stat_file = fopen( dir_name, "r" );
		free( dir_name );
		if( stat_file == NULL ) {
			rescan = 1;
		}
		
		// if there is force rescan, we rescan
		if( rescan ) {
			VERBOSE( 2, "S | initial scanning             %s\n", dir->name);
			data->total_size += scan_dir( data, dir->name );
			VERBOSE( 3, "S | done                         %s\n", dir->name);
		}
		// or we can just open stat file and read size of data from it
		else {
			fscanf( stat_file, "%" SCNu64, &size );
			data->total_size += size;
		}
		if( stat_file ) {
			fclose( stat_file );
		}
		dir = dir->next;
	}
	while( !done ) {
		// pipe thread is doing for us list of directories which are worthy of rescan
		// so we wait for signal from pipe that list is ready
		if( sem_wait( &data->sem_rescan ) == -1 ) break;
		// list is ready, rescan it
		while( data->queue_rescan->directory != NULL ) {
			VERBOSE( 2, "S | scanning                     %s\n", data->queue_rescan->directory->name);
			size2=0;
			if( asprintf( &dir_name, "%s/stat.txt", data->queue_rescan->directory->name ) == -1 ) {
				ERROR;
				continue;
			}
			if( access( dir_name, R_OK ) == 0 ) {
				stat_file = fopen( dir_name, "r" );
				if( stat_file != NULL ) {
					pthread_mutex_lock( &data->mutex_file );
					fscanf( stat_file, "%" SCNd64, &size2 );
					pthread_mutex_unlock( &data->mutex_file );
					fclose( stat_file );
				}
				else {
					ERROR;
					continue;
				}
			}
			
			size = scan_dir( data, data->queue_rescan->directory->name );			
			size = size - size2;
			
			
			pthread_mutex_lock( &data->mutex_mem );
			data->total_size += size;
			pthread_mutex_unlock( &data->mutex_mem );
			
			// get root directory of scanned directory
			dir = data->queue_watch->directory;
			while( dir != NULL ) {
				if( strncmp( dir->name, data->queue_rescan->directory->name, strlen( dir->name ) ) == 0 ) break;
					
				dir = dir->next;
			}
			stat_file_dir = strdup( data->queue_rescan->directory->name );
			
			
			while( strcmp( dir->name, stat_file_dir ) != 0 ) {
				size2 = 0;
				
				ptrc = strrchr( stat_file_dir, '/' );
				ptrc[0] = '\0';

				if( asprintf( &dir_name, "%s/stat.txt", stat_file_dir ) == -1 ) {
					ERROR;
					continue;
				}

				stat_file = fopen( dir_name, "r" );
				if( stat_file != NULL ) {
					pthread_mutex_lock( &data->mutex_file );
					fscanf( stat_file, "%" SCNd64, &size2 );
					pthread_mutex_unlock( &data->mutex_file );
					fclose( stat_file );
				}
				else {
					ERROR;
					size2 = 0;
				}
				size2 += size;
				stat_file = fopen( dir_name, "w" );
				pthread_mutex_lock( &data->mutex_file );
				fprintf( stat_file, "%" PRId64, size2 );
				pthread_mutex_unlock( &data->mutex_file );
				fclose( stat_file );
				free( dir_name );
			}
		
			free( stat_file_dir );
			
			VERBOSE( 3, "S | done                         %s\n", data->queue_rescan->directory->name );
			pthread_mutex_lock( &data->mutex_mem );
			buffer_rm_dir( data->queue_rescan );
			pthread_mutex_unlock( &data->mutex_mem );
		}
		
		

	}
	return NULL;	
}

/**
 * @brief Recursively scan directory, in directory create stat file which contents size of all subfirectories/files
 * @param force if 1 ignore stat files and return full size + add file sizes to data structure, if 0 check differences between data in stat file and real directory size, returns difference
 * @param data
 * @param dir_name
 * @return 
 */
int64_t scan_dir( struct s_data * data, char * dir_name ) {
	char * child;
	DIR * rdir;
	FILE * stat_file;
	int64_t size = 0;
	struct dirent * file;
	struct stat file_stat;
	
	rdir = opendir( dir_name );
	if( rdir == NULL ) {
		ERROR;
		return 0;
	}
	
	while( !done ) {
		errno = 0;
		file = readdir( rdir );
		if( file == NULL ) {
			if( errno ) {
				ERROR;
				break;
			}
			break;
		}

		// skip "." and ".."
		if( ( strcmp( ".", file->d_name ) == 0 ) ||
			( strcmp( "..", file->d_name ) == 0 ) ||
			( strcmp( "stat.txt", file->d_name ) == 0 ) 
		  ) {
			continue;
		}
		
		if( asprintf( &child, "%s/%s", dir_name, file->d_name ) == -1 ) {
			ERROR;
			continue;
		}

		if( file->d_type == DT_DIR ) {
			size +=scan_dir( data, child );
		}
		else {
			stat( child, &file_stat );
			size += file_stat.st_size;
		}
		free( child );
		
	}
	
	closedir( rdir );
	
	if( asprintf( &child, "%s/stat.txt", dir_name ) == -1 ) {
		ERROR;

		return size;
	}
	
	stat_file = fopen( child, "w" );
	if( stat_file == NULL ) {
		ERROR;

		return 0;
	}
	free( child );
	fprintf( stat_file, "%" PRId64, size );
	fclose( stat_file );

	return size;
}
