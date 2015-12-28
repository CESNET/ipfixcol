/**
 * \file bitset.h
 * \author Lukas Hutak <xhutak01@stud.fit.vutbr.cz>
 * \brief Bitset (header file)
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

#ifndef LNF_BITSET_H
#define LNF_BITSET_H

#include <stdbool.h>

/** \brief Internal type of bitset */
typedef unsigned int bitset_type_t;

/** \brief Bits per an item of an array */
#define BITSET_BITS (8 * sizeof(bitset_type_t))

/** \brief Bitset structure */
typedef struct bitset_s {
	bitset_type_t *array;     /**< Array */
	int size;                 /**< Array items */
} bitset_t;

/**
 * \brief Create a new bitset
 * \param[in] size Number of bits
 * \return On success returns pointer to the bitset. Otherwise returns NULL.
 */
bitset_t *bitset_create(int size);

/**
 * \brief Destroy a bitset
 * \param[out] set Bitset
 */
void bitset_destroy(bitset_t *set);

/**
 * \brief Clear a bitset
 * \param set Bitset
 */
void bitset_clear(bitset_t *set);

/**
 * \brief Get maximal index
 * \param[in] set Bitset
 * \return Max bit index (outsize of bitset!)
 */
static inline int bitset_get_size(const bitset_t *set)
{
	return set->size * BITSET_BITS;
}

/**
 * \brief Set a bit without check of borders
 * \param[in, out] set Bitset
 * \param[in] idx Index
 * \param[in] val Value
 */
static inline void bitset_set_fast(bitset_t *set, int idx, bool val)
{
	if (val == true) {
		set->array[idx / BITSET_BITS] |= (1 << (idx % BITSET_BITS));
	} else {
		set->array[idx / BITSET_BITS] &= ~(1 << (idx % BITSET_BITS));
	}
}

/**
 * \brief Set a bit with check of borders
 * \param[in, out] set Bitset
 * \param[in] idx Index
 * \param[in] val Value
 */
static inline void bitset_set(bitset_t *set, int idx, bool val)
{
	if (idx < 0 || set->size < idx / BITSET_BITS) {
		return;
	}

	bitset_set_fast(set, idx, val);
}

/**
 * \brief Get a bit without check of borders
 * \param[in] set Bitset
 * \param[in] idx Index
 * \return Value of the bit
 */
static inline bool bitset_get_fast(const bitset_t *set, int idx)
{
	return set->array[idx / BITSET_BITS] & (1 << (idx % BITSET_BITS));
}

/**
 * \brief Get a bit with check of borders
 * \param[in] set Bitset
 * \param[in] idx Index
 * \return Value of the bit
 */
static inline bool bitset_get(const bitset_t *set, int idx)
{
	if (idx < 0 || set->size < idx / BITSET_BITS) {
		return 0;
	}

	return bitset_get_fast(set, idx);
}

/**
unsigned int * bitarray = (int *)calloc(size / 8 + 1, sizeof(unsigned int));

static inline void setIndex(unsigned int * bitarray, size_t idx) {
    bitarray[idx / WORD_BITS] |= (1 << (idx % WORD_BITS));
}

**/

#endif /* LNF_BITSET_H */
