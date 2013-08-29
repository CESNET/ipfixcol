/* 
 * File:   expire.h
 * Author: fuf
 *
 * Created on 26. červenec 2013, 10:36
 */

#ifndef EXPIRE_H
#define	EXPIRE_H

#define DELETEBUFFERSIZE 20 // size of buffer of old files

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <semaphore.h>
#include <syslog.h>
#include <stdint.h>

#include "buffer.h"

#define ERROR syslog( LOG_ERR, "%s:%d %s\n", __FILE__, __LINE__, strerror( errno ) );
#define VERBOSE(level, text, args...) if( level<=verbose ){syslog( LOG_INFO, text, ##args);}

extern volatile int done;
extern int verbose;

void term_signal_handler(int sig);

struct s_data {
	struct s_buffer * queue_rescan;  // queue buffer between pipe and rescan thread
	struct s_buffer_inotify * queue_inotify; // list of directories watched by inotify
	struct s_buffer * queue_watch;   // list of directories entered in command line
	struct s_buffer * queue_delete;  // queue of oldest files
	
	char pipe_name[100];
	
	sem_t sem_rescan;
	
	pthread_mutex_t mutex_mem;
	pthread_mutex_t mutex_file;
	pthread_mutex_t mutex_can_delete;
		
	pthread_t thread_rescan;
	pthread_t thread_pipe;
	pthread_t thread_delete_queue;
	pthread_t thread_inotify;
	
	volatile int all_ok;
	
	uint64_t total_size;
	int force_rescan;
	int dir_depth;
	int dir_count;
	
	uint64_t max_size;
	
	int actual_watch;
	
	int inotify_fd;
	
	FILE * file_fifo_read;
	FILE * file_fifo_write;
};

#endif	/* EXPIRE_H */

