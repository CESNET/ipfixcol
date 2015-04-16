/**
 * \file queues.h
 * \author Radek Krejci <rkrejci@cesnet.cz>
 * \brief Header for queues needed by ipfixcol core to pass data.
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

#ifndef QUEUES_H_
#define QUEUES_H_

#include <pthread.h>

#include "ipfixcol.h"

/**
 * \brief Simple ring buffer for passing data between one write thread and one
 * or more read threads.
 *
 * Write thread needs to know the count of reading threads. The scheme of
 * reading the data is usually as follows:
 *
 * - rbuffer_read (); <br/>
 * - do some work with read data; <br/>
 * - rbuffer_remove_reference ();
 *
 * This way data are freed by rbuffer_remove_reference() when all reading
 * threads already read them.
 *
 * Thread calling rbuffer_read() must specify which index it wants to read and
 * the index must be incremented continuously.
 *
 */
struct ring_buffer {
	unsigned int read_offset;
	unsigned int write_offset;
	unsigned int size;
	unsigned int count;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	pthread_cond_t cond_empty;
	struct ipfix_message** data;
	unsigned int* data_references;
};

/**
 * \brief Initiate ring buffer structure with specified size.
 *
 * @param[in] size Size of the ring buffer.
 * @return Pointer to initialized ring buffer structure.
 */
struct ring_buffer* rbuffer_init (unsigned int size);

/**
 * \brief Add new record into the ring buffer.
 *
 * @param[in] rbuffer Ring buffer.
 * @param[in] record IPFIX message structure to be added into the ring buffer.
 * @param[in] refcount Initial refference count - number of reading threads.
 * @return 0 on success, nonzero on error.
 */
int rbuffer_write (struct ring_buffer* rbuffer, struct ipfix_message* record, unsigned int refcount);

/**
 * \brief Get pointer to data in ring buffer - its position is specified by
 * index or by ring buffer's current read offset.
 *
 * @param[in] rbuffer Ring buffer.
 * @param[in] index If (unsigned int)-1, use ring buffer's read offset, else try
 * to get record from index (if value in index is valid).
 * @return Read data from specified index (or read offset) or NULL on error.
 */
struct ipfix_message* rbuffer_read (struct ring_buffer* rbuffer, unsigned int *index);

/**
 * \brief Decrease reference counter on specified record in ring buffer.
 *
 * Each thread can use this function only once (for each ring buffer run) when
 * it is done with data from index. Reference count is set to the number of
 * threads reading data from the ring buffer. The scheme of usage is usually as
 * follows:
 *
 * - rbuffer_read (); <br/>
 * - do some work with read data; <br/>
 * - rbuffer_remove_reference ();
 *
 * @param[in] rbuffer Ring buffer.
 * @param[in] index Index of the item in the ring buffer.
 * @param[in] dofree 1 to free data with 0 references, 0 to lose data by removing
 * the pointer, but do not free the data.
 * @return 0 on success, nonzero on error - no reference on item
 */
int rbuffer_remove_reference (struct ring_buffer* rbuffer, unsigned int index, int dofree);

/**
 * \brief Wait for queue to became empty
 *
 * @param[in] rbuffer Ring buffer.
 * @return 0 on success, nonzero on error
 */
int rbuffer_wait_empty(struct ring_buffer* rbuffer);

/**
 * \brief Destroy ring buffer structures.
 *
 * @param[in] rbuffer Ring buffer to destroy.
 * @return 0 on success, nonzero on error.
 */
int rbuffer_free (struct ring_buffer* rbuffer);

#endif /* QUEUES_H_ */
