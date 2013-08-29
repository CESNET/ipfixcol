/* 
 * File:   delete_queue.h
 * Author: fuf
 *
 * Created on 30. červenec 2013, 10:23
 */

#ifndef DELETE_QUEUE_H
#define	DELETE_QUEUE_H
#include "expire.h"
void * gen_delete_queue( struct s_data * data );
void delete_queue( struct s_data * data, char * dir_name, int depth, struct s_buffer * queue );

#endif	/* DELETE_QUEUE_H */

