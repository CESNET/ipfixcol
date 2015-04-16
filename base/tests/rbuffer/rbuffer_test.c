/**
 * \file rbuffer_test.c
 * \author Petr Velan <petr.velan@cesnet.cz>
 * \brief Test for ipfixcol's ring buffer queue
 *
 * Copyright (C) 2015 CESNET, z.s.p.o.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of the Company nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * ALTERNATIVELY, provided that this notice is retained in full, this
 * product may be distributed under the terms of the GNU General Public
 * License (GPL) version 2 or later, in which case the provisions
 * of the GPL apply INSTEAD OF those given above.
 *
 * This software is provided ``as is, and any express or implied
 * warranties, including, but not limited to, the implied warranties of
 * merchantability and fitness for a particular purpose are disclaimed.
 * In no event shall the company or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even
 * if advised of the possibility of such damage.
 *
 */

#include "../../../src/queues.h" // We expect that ring buffer API does not change
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#define THREAD_NUM 2 // Number of threads to use
#define BUFFER_SIZE 128 // Size of the ring buffer
#define WRITE_COUNT 100000 // How many items should be written
#define READ_COUNT WRITE_COUNT // How many items should each thread dread

struct ring_buffer *rb;
int delays[THREAD_NUM] = {50, 50}; // Delays for each thread

void *reader_thread(void *arg)
{
	unsigned int index = -1;
	int num = *((int*) arg);
	struct ipfix_message *msg;

	printf("Starting thread %i with delay %i\n", num, delays[num]);

	for (int i=0; i<READ_COUNT; i++) {
		msg = rbuffer_read(rb, &index);

		/* give the data a chance to disappear */
		usleep(delays[num]);

		/* lock the rbuffer so that the error output is consistent */
		if (pthread_mutex_lock (&(rb->mutex)) != 0) {
			printf("Mutex lock failed (%s:%d)", __FILE__, __LINE__);
			return (NULL);
		}

		if ( msg->pkt_header->observation_domain_id != i) {
            printf("Error: ODID does not match\n");
			printf("Thread num: %i iteration: %i read from index: %i\n", num, i, index);
			printf("buffer size: %i buffer count: %i read offset: %i write offset: %i\n\n",
                rb->size, rb->count, rb->read_offset, rb->write_offset);
		}

		if (pthread_mutex_unlock(&(rb->mutex)) != 0) {
			printf("Mutex unlock failed (%s:%d)", __FILE__, __LINE__);
			return (NULL);
		}

		rbuffer_remove_reference(rb, index, 1);

		index = (index+1) % BUFFER_SIZE;
	}

	return NULL;
}

int main()
{
	rb = rbuffer_init(BUFFER_SIZE);

	pthread_t threads[THREAD_NUM];
	int idarray[THREAD_NUM];

	for (int i = 0; i < THREAD_NUM; i++) {
		idarray[i] = i;
		pthread_create(&threads[i], NULL, reader_thread, &idarray[i]);
	}

	for (int i=0; i<WRITE_COUNT; i++) {

		struct ipfix_message *record = malloc(sizeof(struct ipfix_message));
		record->pkt_header = malloc(sizeof(struct ipfix_header));
		record->pkt_header->observation_domain_id = i;
		rbuffer_write(rb, record, THREAD_NUM);
	}

	for (int i = 0; i < THREAD_NUM; i++) {
		pthread_join(threads[i], NULL);
	}

	rbuffer_free(rb);

	return 0;
}
