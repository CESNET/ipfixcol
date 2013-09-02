/**
 * future features:
 *	add/remove watching directories while running
 */
#include <getopt.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>

#include "config.h"
#include "expire.h"
#include "scan.h"
#include "pipe.h"
#include "delete_queue.h"
#include "watcher.h"

volatile int done = 0;
int verbose = 0;

void print_help( void )
{
	printf( "Use: "PACKAGE_NAME" OPTIONS DIRECTORY\n\n"
			"Options:\n"
			"  -r, --rescan                  Send daemon message to rescan folder. Daemon HAVE TO be running, conflict with c, d\n"
			"  -f, --force                   Force rescan directories when daemon starts (ignore stat files)\n"
			"  -p, --pipe=NAME               Pipe name, default is ./expiredaemon_ipfix_col\n"
			"  -d, --depth=DEPTH             Dept of watched directories, default 1\n"
			"  -c, --count=COUNT             Count of watched directories, default 1\n"
			"  -s, --max-size=SIZE           Max size of all directories in MB\n\n"
			""
			"");
}
void print_version( void )
{
	printf( PACKAGE_STRING"\n");
}	

void term_signal_handler(int sig)
{
	return;
}

int main (int argc, char **argv) {
	int c;
	int force_rescan = 0;
	char pipe_name[100] = "/var/tmp/expiredaemon-queue";
	int dir_count = 1;
	int dir_depth = 1;
	
	FILE * fifo;
	
	sigset_t action;
	int sig;
	
	long size = 0;
	int rescan = 0;
	int daemon = 0;
	
	struct s_data * data;
	
	int option_index = 0;
	
	static struct option long_options[] = {
		{"version",      no_argument,       0, 'v'},
		{"help",         no_argument,       0, 'h'},
		{"pipe",         required_argument, 0, 'p'},
		{"rescan",       no_argument,       0, 'r'},	// send daemon message to rescan folder
		{"force-rescan", no_argument,       0, 'f'}, // force rescan directory when daemon starts ( ignore stat files and create new ones )
		{"depth",        required_argument, 0, 'd'},
		{"count",        required_argument, 0, 'c'},
		{"max-size",     required_argument, 0, 's'},
		{"verbose-level",     required_argument, 0, 'V'},
		{0, 0, 0, 0}
	};
	
	while (1)
	{
		c = getopt_long(argc, argv, "rhvfp:d:c:s:V:", long_options, &option_index);
        if (c == -1)
            break;
		
		switch( c ) {
			case 'v':
				print_version();
				return 0;
				break;
			case 'h':
				print_help();
				return 0;
				break;
			case 'p':
				memcpy( pipe_name, optarg, strlen( optarg ) );
				break;
			case 'r':
				// rescan, cannot use witouth running daemon
				rescan = 1;
				break;
			case 'f':
				daemon = 1;
				force_rescan = 1;
				break;
			case 'd':
				daemon = 1;
				dir_depth = atoi( optarg );
				break;
			case 'c':
				daemon = 1;
				dir_count = atoi( optarg );
				break;
			case 's':
				daemon = 1;
				size = atol( optarg );
				break;
			case 'V':
				verbose = atol( optarg );
				break;
		}
	}
	
	// check if directories was entered
	if( optind >= argc) {
		fprintf( stderr, "You must specify directories you want to watch. Type %s --help for help.\n", argv[0] );
		return 1;
	}
	if( rescan == 0 ) daemon = 1;
	// check conflict parametres
	if( ( daemon == 1 ) && ( rescan == 1 ) ) {
		fprintf( stderr, "You cannot run rescan while using parametres for running daemon.\n" );
		return 1;
	}
	

	
	sigemptyset(&action);
	sigaddset (&action, SIGTERM);
	sigaddset (&action, SIGINT);
	sigaddset (&action, SIGQUIT);
	
	
	if( rescan ) {
		fifo = fopen( pipe_name, "w" );
		if( fifo == NULL ) {
			fprintf( stderr, "%s\n", strerror( errno ) );
			return 1;
		}
		while (optind < argc) {
			fprintf( fifo, "%s\n", argv[optind]);	
			optind++;
		}
//		fclose( fifo );
		
		return 0;	
	}
	
	if( daemon ) {
		if( size <= 0 ) {
			printf( "You have to enter maximal directory size.\n" );
			return 1;
		}
		data = ( struct s_data * ) malloc( sizeof( struct s_data ) );
		if( data == NULL ) {
			ERROR;
			return 1;
		}
		
		if( ( data->queue_inotify = buffer_inotify_init() ) == NULL ) return 1;
		if( ( data->queue_rescan = buffer_init() ) == NULL ) return 1;
		if( ( data->queue_watch = buffer_init() ) == NULL ) return 1;
		if( ( data->queue_delete = buffer_init() ) == NULL ) return 1;
		
		data->force_rescan = force_rescan;
		data->dir_depth = dir_depth;
		data->dir_count = dir_count;
		data->total_size = 0;
		data->all_ok = 1;
		data->max_size = size*1024*1024;
		
		if( access( pipe_name, F_OK ) != -1 ) {
			// file exists
		} else {
			mkfifo( pipe_name, 0777 );
		}
		
		// add directories
		while (optind < argc) {
			// remove / (if any) from end of directory name
			if( argv[optind][strlen( argv[optind] )-1] == '/' ) argv[optind][strlen( argv[optind] )-1] = '\0';
			
			buffer_add_dir( data->queue_watch, argv[optind++] );
		}
		printf( "%d", verbose );		
		pthread_mutex_init( &data->mutex_mem, NULL );
		if( errno ) {
			ERROR;
			return 1;
		}
		
		pthread_mutex_init( &data->mutex_can_delete, NULL );
		if( errno ) {
			ERROR;
			return 1;
		}
		
		pthread_mutex_init( &data->mutex_file, NULL );
		if( errno ) {
			ERROR;
			return 1;
		}
		
		sem_init( &data->sem_rescan, 0, 0 );
		if( errno ) {
			ERROR;
			return 1;
		}
		
		strcpy( data->pipe_name, pipe_name );
		
		
		pthread_create( &data->thread_rescan, NULL, &thread_rescan_func, ( void * ) data );
		pthread_create( &data->thread_pipe, NULL, &thread_pipe_func, ( void * ) data );
		pthread_create( &data->thread_inotify, NULL, &thread_inotify_func, ( void *) data );

	}

	
	sigwait( &action, &sig );
	fprintf( stderr, "Received %i, waiting for threads to end...\n", sig );
	done = 1;
	
	pthread_kill( data->thread_rescan, SIGUSR1 );
	pthread_kill( data->thread_inotify, SIGUSR1 );
	pthread_kill( data->thread_pipe, SIGUSR1 );
	
	
	
	pthread_join( data->thread_rescan, NULL );
	pthread_join( data->thread_inotify, NULL );
	pthread_join( data->thread_pipe, NULL );
	
	buffer_inotify_destruct( data->queue_inotify );
	buffer_destruct( data->queue_rescan );
	buffer_destruct( data->queue_watch );
	buffer_destruct( data->queue_delete );
	
	pthread_mutex_destroy( &data->mutex_mem );
	pthread_mutex_destroy( &data->mutex_can_delete );
	pthread_mutex_destroy( &data->mutex_file );
	sem_destroy( &data->sem_rescan );
	
	free( data );
	
	fprintf( stderr, "Exited...\n\n ");
	return 0;
}
