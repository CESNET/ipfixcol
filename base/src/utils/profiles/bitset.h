/**
 * \file bitset.h
 * \author Lukas Hutak <xhutak01@stud.fit.vutbr.cz>
 * \brief Bitset (header file)
 *
 * Copyright (C) 2015-2017 CESNET, z.s.p.o.
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
	bitset_type_t *array;     /**< Bit array                         */
	size_t         size;      /**< Number of items (bitset_type_t)   */
} bitset_t;

/**
 * \brief Create a new bitset
 * \param[in] size Number of bits
 * \return On success returns pointer to the bitset. Otherwise returns NULL.
 */
bitset_t *
bitset_create(size_t size);

/**
 * \brief Destroy a bitset
 * \param[out] set Bitset
 */
void
bitset_destroy(bitset_t *set);

/**
 * \brief Clear a bitset
 * \param[in,out] set Bitset
 */
void
bitset_clear(bitset_t *set);

/**
 * \brief Resize the bitset
 *
 * The values of bits will be unchanged. If the new size is larger than
 * the old size, the added bits are set will be set to false.
 * \param[in,out] set  Bitset
 * \param[in]     size New number of bits
 * \return On success returns 0. Otherwise returns a non-zero value and
 *   the size is unchanged.
 */
int
bitset_resize(bitset_t *set, size_t size);

/**
 * \brief Get maximal index
 * \param[in] set Bitset
 * \return Max bit index (outsite of bitset!)
 */
static inline size_t
bitset_get_size(const bitset_t *set)
{
	return set->size * BITSET_BITS;
}

/**
 * \brief Set a bit (without border check)
 * \param[in, out] set Bitset
 * \param[in] idx Index
 * \param[in] val Value
 */
static inline void
bitset_set_fast(bitset_t *set, size_t idx, bool val)
{
	if (val == true) {
		set->array[idx / BITSET_BITS] |= (1U << (idx % BITSET_BITS));
	} else {
		set->array[idx / BITSET_BITS] &= ~(1U << (idx % BITSET_BITS));
	}
}

/**
 * \brief Set a bit (with border check)
 * \param[in, out] set Bitset
 * \param[in] idx Index
 * \param[in] val Value
 */
static inline void
bitset_set(bitset_t *set, size_t idx, bool val)
{
	if (set->size <= idx / BITSET_BITS) {
		return;
	}

	bitset_set_fast(set, idx, val);
}

/**
 * \brief Get a bit (without border check)
 * \param[in] set Bitset
 * \param[in] idx Index
 * \return Value of the bit.
 */
static inline bool
bitset_get_fast(const bitset_t *set, size_t idx)
{
	return set->array[idx / BITSET_BITS] & (1U << (idx % BITSET_BITS));
}

/**
 * \brief Get a bit (with border check)
 * \param[in] set Bitset
 * \param[in] idx Index
 * \return Value of the bit.
 */
static inline bool
bitset_get(const bitset_t *set, size_t idx)
{
	if (set->size <= idx / BITSET_BITS) {
		return 0;
	}

	return bitset_get_fast(set, idx);
}

/**
 * \brief Get a bit and set its new value (with border check)
 * \param set Bitset
 * \param idx Index
 * \param val New value
 * \return Value of the bit before setting the new value.
 */
static inline bool
bitset_get_and_set(bitset_t *set, size_t idx, bool val)
{
	if (set->size <= idx / BITSET_BITS) {
		return 0;
	}

	bool result = bitset_get_fast(set, idx);
	bitset_set_fast(set, idx, val);
	return result;
}

#endif /* LNF_BITSET_H */
