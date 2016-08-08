/**
 * \file bitset.c
 * \author Lukas Hutak <xhutak01@stud.fit.vutbr.cz>
 * \brief Bitset (source file)
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

#include <stdlib.h>
#include <string.h>
#include "bitset.h"

// Create a new bitset
bitset_t *bitset_create(int size)
{
	bitset_t *set = (bitset_t *) calloc(1, sizeof(bitset_t));
	if (!set) {
		return NULL;
	}

	set->size = (size / BITSET_BITS) + 1;
	set->array = (bitset_type_t *) calloc(set->size, sizeof(bitset_type_t));
	if (!set->array) {
		free(set);
		return NULL;
	}

	return set;
}

// Destroy a bitset
void bitset_destroy(bitset_t *set)
{
	if (!set) {
		return;
	}

	free(set->array);
	free(set);
}

// Clear a bitset
void bitset_clear(bitset_t *set)
{
	if (!set) {
		return;
	}

	memset(set->array, 0, set->size * sizeof(bitset_type_t));
}
