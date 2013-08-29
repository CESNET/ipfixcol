#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <signal.h>

#include "expire.h"
#include "pipe.h"
#include "expire.h"
#include "buffer.h"


void * thread_pipe_func( void * ptr ) {
	struct s_data * data = ( struct s_data * ) ptr;
	size_t line_size = 0;
	ssize_t actual_size = 0;
	char *string = NULL;
	struct s_directory * dir;
	int ok;
	struct sigaction action;
	
	sigemptyset(&action.sa_mask);
	action.sa_flags = 0;
	action.sa_handler = term_signal_handler;
	sigaction(SIGUSR1, &action, NULL);
	
	data->file_fifo_read = fopen( data->pipe_name, "r" );
	if( errno ) {
		ERROR;
		return NULL;
	}

	data->file_fifo_write = fopen( data->pipe_name, "w" );
	if( errno ) {
		ERROR;
		return NULL;
	}
	
	while( !done ) {
		if( ( actual_size = getline( &string, &line_size, data->file_fifo_read ) ) == -1 ) break;
		
		// replace \n and / with \0
		string[ actual_size-1 ] = '\0';
		if( string[ actual_size-2 ] == '/' ) string[ actual_size-2 ] = '\0';
		
		
		
		// check if directory is subdirectory of watch list
		dir = data->queue_watch->directory;
		ok = 0;

		while( dir != NULL ) {
			if( strncmp( dir->name, string, strlen( dir->name ) ) == 0 ) ok = 1;
			
			dir = dir->next;
		}
		if( ok == 1 ) {
			VERBOSE( 3,"P | got                          %s\n", string );
			pthread_mutex_lock( &data->mutex_mem );
			buffer_add_dir( data->queue_rescan, string );
			pthread_mutex_unlock( &data->mutex_mem );
			

			sem_post( &data->sem_rescan );

			if( errno ) {
				ERROR;
				return NULL;
			}
		}
		
		
	}
	
	fclose( data->file_fifo_read );
	fclose( data->file_fifo_write );
	
	
	free( string );
	
	return NULL;
}
