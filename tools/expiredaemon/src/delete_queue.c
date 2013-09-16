#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <dirent.h>
#include <pthread.h>
#include <signal.h>

#include "expire.h"
#include "delete_queue.h"
#include "buffer.h"
/**
 * @brief Create list of oldest directories for deleting.
 *        It is better to have that list instead of rescaning while directory 
 *        tree to get oldest dir and remove it everytime we want to delete dir
 * @param ptr
 * @return 
 */
void * gen_delete_queue( struct s_data * data ) {
	struct s_directory * dir;
	struct s_directory * dir_next;
	struct s_buffer * tmp_queue;
	struct s_directory * watch_dir;
	struct s_directory * dir2;
	struct sigaction action;
	
	sigemptyset(&action.sa_mask);
	action.sa_flags = 0;
	action.sa_handler = term_signal_handler;
	sigaction(SIGUSR1, &action, NULL);
	
	

	VERBOSE( 3, "Q | New delete queue\n");
	watch_dir = data->queue_watch->directory;

	// this is full of woodoo magic, it works so let it be
	while( watch_dir != NULL ) {
		tmp_queue = buffer_init();
		delete_queue( data, watch_dir->name, 1, tmp_queue );

		if( tmp_queue == NULL ) {
			buffer_destruct( tmp_queue );
			continue;
		}

		// if delete queue is empty, copy there tmp queue
		if( data->queue_delete->directory == NULL ) {
			memcpy( data->queue_delete, tmp_queue, sizeof( struct s_buffer ) );
			//*data->queue_delete = *tmp_queue;
			free( tmp_queue );
		}
		else {
			// queue is full and oldest element is later than oldest element in tmp_queue -> continue;
			if( 
				( buffer_get_last( data->queue_delete )->data <= tmp_queue->directory->data ) ) {
				buffer_destruct( tmp_queue );;
				watch_dir = watch_dir->next;
				continue;
			}

			while( ( dir = tmp_queue->directory ) != NULL ) {
				dir_next = dir->next;
				if ( data->queue_delete->directory->data > dir->data ) {
					dir->next = data->queue_delete->directory;
					data->queue_delete->directory = dir;
					data->queue_delete->count++;

				}
				else {
					dir2 = data->queue_delete->directory;
					while( dir2->next != NULL ) {
						if( dir2->next->data > dir->data ) break;
						dir2 = dir2->next;
					}
					dir->next = dir2->next;
					dir2->next = dir;
					data->queue_delete->count++;

					
				}
				
				if( ( data->queue_delete->count == DELETEBUFFERSIZE ) ) {
					data->queue_delete->count--;
					dir2 = data->queue_delete->directory;
					while( dir2->next->next != NULL ) {
						dir2 = dir2->next;
					}
					free( dir2->next->name );
					free( dir2->next );
					dir2->next = NULL;
				}

				tmp_queue->directory = dir_next;
			}
			
			buffer_destruct( tmp_queue );
		}

		watch_dir = watch_dir->next;

	}
	
	return NULL;

}

void delete_queue( struct s_data * data, char * dir_name, int depth, struct s_buffer * queue ) {
	struct s_directory * dir_new;
	struct s_directory * dir;
	struct s_directory * dir_added;
	struct s_buffer * buffer;

	int i;
	
	char * child_name;

	DIR * rdir;
	struct dirent * file;
	
	buffer = buffer_init();

	rdir = opendir( dir_name );
	if( rdir == NULL ) {
		fprintf( stderr, "delete_queue(): opendir(): %s: %s\n", dir_name ,strerror( errno ) );
		return	;
	}
	
	while( !done ) {
		errno = 0;
		file = readdir( rdir );
		if( file == NULL ) {
			if( errno ) {
				fprintf( stderr, "delete_queue(): readdir(): %s: %s\n", dir_name ,strerror( errno ) );
				
			}
			break;
		}
		
		if( file == NULL ) break; // reached end of directory

		// skip "." and ".."
		if( ( strcmp( ".", file->d_name ) == 0 ) ||
			( strcmp( "..", file->d_name ) == 0 ) ||
			( strcmp( "stat.txt", file->d_name ) == 0 )
		  ) {
			continue;
		}
		
		if( asprintf( &child_name, "%s/%s", dir_name, file->d_name ) == -1 ) {
			fprintf( stderr, "delete_queue: asprintf error alocation\n" );
			continue;
		}
		if( file->d_type == DT_DIR ) {
			dir_new = buffer_create_dir( child_name );
			for( i = 0; file->d_name[i] != '\0'; i++ ) {
				if( ( file->d_name[i] >= '0' ) && ( file->d_name[i] <= '9')  ) break;
			}
			
			dir_new->data = atol( file->d_name+i );
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
		}
		free( child_name );
	}
	if( buffer == NULL ) {
		rewinddir(rdir);
		while( 1 ) {
			file = readdir( rdir );
			if( file == NULL ) {
				if( errno ) {
					fprintf( stderr, "delete_queue(): readdir(): %s: %s\n", dir_name ,strerror( errno ) );
					
				}
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
				fprintf( stderr, "delete_queue: asprintf error alocation\n" );
				continue;
			}


			unlink(child_name);
		}
	}
	closedir( rdir );
	if( depth == data->dir_depth ) {
		dir = buffer->directory;
		while( dir != NULL ) {
			if( queue->count >= DELETEBUFFERSIZE ) break;
			
			dir_added = buffer_add_dir( queue, dir->name );
			dir_added->data = dir->data;
			
			dir = dir->next;
		}
		
	}
	else {
		dir = buffer->directory;
		while( dir != NULL ) {
			if( queue->count >= DELETEBUFFERSIZE ) break;
			
			delete_queue( data, dir->name, depth+1, queue);
			
			dir = dir->next;
		}
	}
	
	buffer_destruct( buffer );
}
