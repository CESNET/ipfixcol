/**
 * \file queues.c
 * \author Radek Krejci <rkrejci@cesnet.cz>
 * \brief Implementation of queues needed by ipfixcol core to pass data.
 *
 * Copyright (C) 2011 CESNET, z.s.p.o.
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

#include <unistd.h>
#include <errno.h>
#include <stdlib.h>

#include "queues.h"

/** Identifier to MSG_* macros */
static char *msg_module = "queue";

/**
 * \brief Initiate ring buffer structure with specified size.
 *
 * @param[in] size Size of the ring buffer.
 * @return Pointer to initialized ring buffer structure.
 */
struct ring_buffer* rbuffer_init (unsigned int size)
{
	struct ring_buffer* retval = NULL;

	if (size == 0) {
		MSG_ERROR(msg_module, "Size of the ring buffer set to zero");
		return (NULL);
	}

	retval = (struct ring_buffer*) malloc (sizeof(struct ring_buffer));
	if (retval == NULL) {
		MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)", __FILE__, __LINE__);
		return (NULL);
	}

	retval->read_offset = 0;
	retval->write_offset = 0;
	retval->count = 0;
	retval->size = size;
	retval->data = (struct ipfix_message**) malloc (size * sizeof(struct ipfix_message*));
	if (retval->data == NULL) {
		MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)", __FILE__, __LINE__);
		free (retval);
		return (NULL);
	}
	retval->data_references = (unsigned int*) calloc (size, sizeof(unsigned int));
	if (retval->data_references == NULL) {
		MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)", __FILE__, __LINE__);
		free (retval->data);
		free (retval);
		return (NULL);
	}

	if (pthread_mutex_init (&(retval->mutex), NULL) != 0) {
		MSG_ERROR(msg_module, "Initialization of condition variable failed (%s:%d)", __FILE__, __LINE__);
		free (retval->data_references);
		free (retval->data);
		free (retval);
		return (NULL);
	}

	if (pthread_cond_init (&(retval->cond), NULL) != 0) {
		MSG_ERROR(msg_module, "Initialization of condition variable failed (%s:%d)", __FILE__, __LINE__);
		pthread_mutex_destroy (&(retval->mutex));
		free (retval->data_references);
		free (retval->data);
		free (retval);
		return (NULL);
	}

	if (pthread_cond_init (&(retval->cond_empty), NULL) != 0) {
		MSG_ERROR(msg_module, "Initialization of condition variable failed (%s:%d)", __FILE__, __LINE__);
		pthread_mutex_destroy (&(retval->mutex));
		free (retval->data_references);
		free (retval->data);
		free (retval);
		return (NULL);
	}

	return (retval);
}

/**
 * \brief Add new record into the ring buffer.
 *
 * @param[in] rbuffer Ring buffer.
 * @param[in] record IPFIX message structure to be added into the ring buffer.
 * @param[in] refcount Initial refference count - number of reading threads.
 * @return 0 on success, nonzero on error.
 */
int rbuffer_write (struct ring_buffer* rbuffer, struct ipfix_message* record, unsigned int refcount)
{
	if (rbuffer == NULL || refcount == 0) {
		MSG_ERROR(msg_module, "Invalid ring buffer write parameters");
		return (EXIT_FAILURE);
	}

	if (pthread_mutex_lock (&(rbuffer->mutex)) != 0) {
		MSG_ERROR(msg_module, "Mutex lock failed (%s:%d)", __FILE__, __LINE__);
		return (EXIT_FAILURE);
	}
	/* it will be never more than ring buffer size, but just to be sure I'm checking it 
	 * leave one position in buffer free, so that faster thread cannot read 
	 * data yet not procees by slower one */
	while (rbuffer->count + 1 >= rbuffer->size) {
		if (pthread_cond_wait (&(rbuffer->cond), &(rbuffer->mutex)) != 0) {
			MSG_ERROR(msg_module, "Condition wait failed (%s:%d)", __FILE__, __LINE__);

			if (pthread_mutex_unlock (&(rbuffer->mutex)) != 0) {
				MSG_ERROR(msg_module, "Mutex unlock failed (%s:%d)", __FILE__, __LINE__);
			}
			
			return (EXIT_FAILURE);
		}
	}

	rbuffer->data[rbuffer->write_offset] = record;
	rbuffer->data_references[rbuffer->write_offset] = refcount;
	rbuffer->write_offset = (rbuffer->write_offset + 1) % rbuffer->size;
	rbuffer->count++;

	/* I did change of rbuffer->count so inform about it other threads (read threads) */
	if (pthread_cond_signal (&(rbuffer->cond)) != 0) {
		MSG_ERROR(msg_module, "Condition signal failed (%s:%d)", __FILE__, __LINE__);
		return (EXIT_FAILURE);
	}

	if (pthread_mutex_unlock (&(rbuffer->mutex)) != 0) {
		MSG_ERROR(msg_module, "Mutex unlock failed (%s:%d)", __FILE__, __LINE__);
		return (EXIT_FAILURE);
	}

	return (EXIT_SUCCESS);
}

/**
 * \brief Get pointer to data in ring buffer - its position is specified by
 * index or by ring buffer's current read offset.
 *
 * @param[in] rbuffer Ring buffer.
 * @param[in] index If (unsigned int)-1, use ring buffer's read offset, else try
 * to get record from index (if value in index is valid).
 * @return Read data from specified index (or read offset) or NULL on error.
 */
struct ipfix_message* rbuffer_read (struct ring_buffer* rbuffer, unsigned int *index)
{
	if (*index == (unsigned int) -1) {
		/* if no index specified -> read from read_offset, so just 1 record in ring buffer required */
		*index = rbuffer->read_offset;
	}

	/* check if the ring buffer is full enough, if not wait (and block) for it */
	if (pthread_mutex_lock (&(rbuffer->mutex)) != 0) {
		MSG_ERROR(msg_module, "Mutex lock failed (%s:%d)", __FILE__, __LINE__);
		return (NULL);
	}
	/* wait when trying to read from write_offset - no data here yer
	 * otherwise it's ok, reading tread connot outrun the writing one,
	 * unles it demands indexes nonlinearly */
	while (rbuffer->write_offset == *index) {
		if (pthread_cond_wait (&(rbuffer->cond), &(rbuffer->mutex)) != 0) {
			MSG_ERROR(msg_module, "Condition wait failed (%s:%d)", __FILE__, __LINE__);

			if (pthread_mutex_unlock (&(rbuffer->mutex)) != 0) {
				MSG_ERROR(msg_module, "Mutex unlock failed (%s:%d)", __FILE__, __LINE__);
			}

			return (NULL);
		}
	}
	if (pthread_mutex_unlock(&(rbuffer->mutex)) != 0) {
		MSG_ERROR(msg_module, "Mutex unlock failed (%s:%d)", __FILE__, __LINE__);
		return (NULL);
	}

	/* Wake up other threads waiting for read */
	if (pthread_cond_signal (&(rbuffer->cond)) != 0) {
		MSG_ERROR(msg_module, "Condition signal failed (%s:%d)", __FILE__, __LINE__);
		return (NULL);
	}

	/* get data */
	return (rbuffer->data[*index]);
}

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
int rbuffer_remove_reference (struct ring_buffer* rbuffer, unsigned int index, int dofree)
{
	int i;

	/* atomic rbuffer->data_references[index]--; and check <= 0 */
	if (__sync_fetch_and_sub (&(rbuffer->data_references[index]), 1) <= 0) {
		return (EXIT_FAILURE);
	}

	if (pthread_mutex_lock (&(rbuffer->mutex)) != 0) {
		MSG_ERROR(msg_module, "Mutex lock failed (%s:%d)", __FILE__, __LINE__);
		return (EXIT_FAILURE);
	}
	/* it will be never less than zero, but just to be sure I'm checking it */
	if (rbuffer->data_references[rbuffer->read_offset] <= 0) {
		while ((rbuffer->data_references[rbuffer->read_offset] == 0) && (rbuffer->count > 0)) {
			if (dofree) {
				/* free the data */
				if (rbuffer->data[rbuffer->read_offset]) {
					if (rbuffer->data[rbuffer->read_offset]->pkt_header) {
						free (rbuffer->data[rbuffer->read_offset]->pkt_header);
					}

					/* Decrement reference on templates */
					for (i = 0; i < MSG_MAX_DATA_COUPLES && rbuffer->data[rbuffer->read_offset]->data_couple[i].data_set; ++i) {
						if (rbuffer->data[rbuffer->read_offset]->data_couple[i].data_template) {
							tm_template_reference_dec(rbuffer->data[rbuffer->read_offset]->data_couple[i].data_template);
						}
					}
					
					if (rbuffer->data[rbuffer->read_offset]->metadata) {
						message_free_metadata(rbuffer->data[rbuffer->read_offset]);
					}
					
					free (rbuffer->data[rbuffer->read_offset]);
				}
			}
			/* move offset pointer in ring buffer */
			rbuffer->read_offset = (rbuffer->read_offset + 1) % rbuffer->size;
			rbuffer->count--;

			/* signal to write thread waiting for empty queue */
			if (rbuffer->count == 0) {
				if (pthread_cond_signal (&(rbuffer->cond_empty)) != 0) {
					MSG_ERROR(msg_module, "Condition signal failed (%s:%d)", __FILE__, __LINE__);

					if (pthread_mutex_unlock (&(rbuffer->mutex)) != 0) {
						MSG_ERROR(msg_module, "Mutex unlock failed (%s:%d)", __FILE__, __LINE__);
					}

					return (EXIT_FAILURE);
				}
			}
		}
		if (pthread_mutex_unlock (&(rbuffer->mutex)) != 0) {
			MSG_ERROR(msg_module, "Mutex unlock failed (%s:%d)", __FILE__, __LINE__);
			return (EXIT_FAILURE);
		}
	} else {
		if (pthread_mutex_unlock (&(rbuffer->mutex)) != 0) {
			MSG_ERROR(msg_module, "Mutex unlock failed (%s:%d)", __FILE__, __LINE__);
			return (EXIT_FAILURE);
		}
		/* only reference decrease was done, moving rbuffer->read_offset not */
		return (EXIT_SUCCESS);
	}

	/* I did change of rbuffer->count so inform about it other threads (mainly write thread) */
	if (pthread_cond_signal (&(rbuffer->cond)) != 0) {
		MSG_ERROR(msg_module, "Condition signal failed (%s:%d)", __FILE__, __LINE__);
		return (EXIT_FAILURE);
	}

	return (EXIT_SUCCESS);
}

/**
 * \brief Wait for queue to became empty
 *
 * @param[in] rbuffer Ring buffer.
 * @return 0 on success, nonzero on error
 */
int rbuffer_wait_empty(struct ring_buffer* rbuffer) {
	if (pthread_mutex_lock (&(rbuffer->mutex)) != 0) {
		MSG_ERROR(msg_module, "Mutex lock failed (%s:%d)", __FILE__, __LINE__);
		return (EXIT_FAILURE);
	}

	while (rbuffer->count > 0) {
		if (pthread_cond_wait (&(rbuffer->cond_empty), &(rbuffer->mutex)) != 0) {
			MSG_ERROR(msg_module, "Condition wait failed (%s:%d)", __FILE__, __LINE__);
			return (EXIT_FAILURE);
		}
	}

	if (pthread_mutex_unlock (&(rbuffer->mutex)) != 0) {
		MSG_ERROR(msg_module, "Mutex unlock failed (%s:%d)", __FILE__, __LINE__);
		return (EXIT_FAILURE);
	}
	return (EXIT_SUCCESS);
}

/**
 * \brief Destroy ring buffer structures.
 *
 * @param[in] rbuffer Ring buffer to destroy.
 * @return 0 on success, nonzero on error.
 */
int rbuffer_free (struct ring_buffer* rbuffer)
{
	if (rbuffer) {
		if (rbuffer->data_references) {
			free (rbuffer->data_references);
		}
		if (rbuffer->data) {
			free (rbuffer->data);
		}

		pthread_cond_destroy (&(rbuffer->cond));
		pthread_cond_destroy (&(rbuffer->cond_empty));
		pthread_mutex_destroy (&(rbuffer->mutex));

		free (rbuffer);
	}

	return (EXIT_SUCCESS);
}
