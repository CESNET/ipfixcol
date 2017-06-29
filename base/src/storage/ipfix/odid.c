/**
 * \file storage/ipfix/odid.c
 * \author Lukas Hutak <xhutak01@stud.fit.vutbr.cz>
 * \brief ODID information (source file)
 */
/* Copyright (C) 2017 CESNET, z.s.p.o.
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
* This software is provided ``as is``, and any express or implied
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
*/


#include "odid.h"
#include <stdlib.h>

/** Number of pre-allocated records during initialization */
#define ODID_CNT_PREALLOC (8U)
/** Number of added records during reallocation          */
#define ODID_CNT_ADD      (8U)

/**
 * \brief Internal structure
 */
struct odid_s {
	/** Sorted array of ODID records (sorted by ODID number) */
	struct odid_record *records;
	/** Number of valid ODID records                         */
	size_t rec_cnt;
	/** Number of pre-allocated records                       */
	size_t rec_alloc;
};

/**
 * \breif Compare function for bsearch and qsort
 * \param[in] o1 First ODID record
 * \param[in] o2 Second ODID record
 * \returns An integer less than, equal to, or greater than zero if the first
 *   argument is  considered  to be respectively less than, equal to, or
 *   greater than the second.
 */
static int
odid_cmp(const void *o1, const void *o2)
{
	const struct odid_record *odid1 = (const struct odid_record *) o1;
	const struct odid_record *odid2 = (const struct odid_record *) o2;

	if (odid1->odid == odid2->odid) {
		return 0;
	} else {
		return (odid1->odid < odid2->odid) ? (-1) : 1;
	}
}

/**
 * \brief Sort records in the ODID maintainer by their ID.
 */
static void
odid_sort(odid_t *odid)
{
	qsort(odid->records, odid->rec_cnt, sizeof(struct odid_record), odid_cmp);
}

odid_t *
odid_create()
{
	odid_t *result = calloc(1, sizeof(*result));
	if (!result) {
		return NULL;
	}

	result->records = calloc(ODID_CNT_PREALLOC, sizeof(struct odid_record));
	if (!result->records) {
		free(result);
		return NULL;
	}

	result->rec_alloc = ODID_CNT_PREALLOC;
	return result;
}

void
odid_destroy(odid_t *odid)
{
	if (!odid) {
		return;
	}

	free(odid->records);
	free(odid);
}

struct odid_record *
odid_find(odid_t *odid, uint32_t id)
{
	struct odid_record key;
	key.odid = id;

	return bsearch(&key, odid->records, odid->rec_cnt,
		sizeof(struct odid_record), odid_cmp);
}

struct odid_record *
odid_get(odid_t *odid, uint32_t id)
{
	struct odid_record *ptr = odid_find(odid, id);
	if (ptr) {
		// Found
		return ptr;
	}

	// Not present -> create a new one
	if (odid->rec_cnt == odid->rec_alloc) {
		// Resize the array
		struct odid_record *new_array;
		const size_t new_alloc = odid->rec_cnt + ODID_CNT_ADD;
		const size_t mem_size = new_alloc * sizeof(struct odid_record);

		new_array = realloc(odid->records, mem_size);
		if (!new_array) {
			return NULL;
		}

		odid->records = new_array;
		odid->rec_alloc = new_alloc;
	}

	// Add the record
	ptr = &odid->records[odid->rec_cnt];
	ptr->odid = id;
	ptr->seq_num = 0;
	ptr->export_time = 0;
	odid->rec_cnt++;

	// Sort the array of the ODID records
	odid_sort(odid);

	// The position has been changed -> find the record again
	return odid_find(odid, id);
}

int
odid_remove(odid_t *odid, uint32_t id)
{
	struct odid_record *ptr = odid_find(odid, id);
	if (!ptr) {
		// Not found
		return 1;
	}

	// Replace it with the last record in the array
	const size_t last_idx = odid->rec_cnt - 1;
	const struct odid_record *last_rec = &odid->records[last_idx];

	if (ptr->odid == last_rec->odid) {
		// Just remove the last record
		odid->rec_cnt--;
		return 0;
	}

	// Copy data and sort;
	*ptr = *last_rec;
	odid->rec_cnt--;
	odid_sort(odid);
	return 0;
}

