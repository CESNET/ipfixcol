/**
 * \file hashes.h
 * \brief Fast 4-way hash table with stash - hash functions.
 * \author Matej Vido, xvidom00@stud.fit.vutbr.cz
 * \date 2014
 */
/*
 * Copyright (C) 2015 CESNET
 *
 * LICENSE TERMS
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
 * This software is provided ``as is'', and any express or implied
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

#ifndef __HASHES_H__
#define __HASHES_H__

#include <stdint.h>

/*
 * \brief Hash functions used for the table.
 *
 * This functions are MurmurHash3.
 * https://code.google.com/p/smhasher/wiki/MurmurHash3
 *
 * hash_40      optimized and used for 40 bytes long keys
 * hash_div8    optimized and used for keys which size is divisible by 8
 * hash         used for other key sizes
 */

#define ROTL64(num,amount) (((num) << (amount & 63)) | ((num) >> (64 - (amount & 63))))

extern inline uint32_t hash_40(const void *key, int32_t key_size);
inline uint32_t hash_40(const void *key, int32_t key_size)
{
    uint32_t c1 = 5333;
    uint32_t c2 = 7177;
    uint32_t r1 = 19;
    uint32_t m1 = 11117;
    uint32_t n1 = 14011;
    uint64_t h = 42;
    uint64_t * k_ptr = (uint64_t *) key;
    uint64_t k;

    //
    k = *k_ptr;
    k *= c1;
    k = ROTL64(k, r1);
    k *= c2;

    h ^= k;

    h = ROTL64(h, r1);
    h = h * m1 + n1;
    //
    k = *(k_ptr + 1);
    k *= c1;
    k = ROTL64(k, r1);
    k *= c2;

    h ^= k;

    h = ROTL64(h, r1);
    h = h * m1 + n1;
    //
    k = *(k_ptr + 2);
    k *= c1;
    k = ROTL64(k, r1);
    k *= c2;

    h ^= k;

    h = ROTL64(h, r1);
    h = h * m1 + n1;
    //
    k = *(k_ptr + 3);
    k *= c1;
    k = ROTL64(k, r1);
    k *= c2;

    h ^= k;

    h = ROTL64(h, r1);
    h = h * m1 + n1;
    //
    k = *(k_ptr + 4);
    k *= c1;
    k = ROTL64(k, r1);
    k *= c2;

    h ^= k;

    h = ROTL64(h, r1);
    h = h * m1 + n1;
    //

    h ^= h >> 33;
    h *= 0xff51afd7ed558ccd;
    h ^= h >> 33;
    h *= 0xc4ceb9fe1a85ec53;
    h ^= h >> 33;

    return (uint32_t) h;
}

extern inline uint32_t hash_div8(const void *key, int32_t key_size);
inline uint32_t hash_div8(const void *key, int32_t key_size)
{
    uint32_t c1 = 5333;
    uint32_t c2 = 7177;
    uint32_t r1 = 19;
    uint32_t m1 = 11117;
    uint32_t n1 = 14011;
    uint64_t h = 42;
    uint64_t * k_ptr = (uint64_t *) key;
    uint64_t k;
    uint32_t rep = key_size / 8;
    uint32_t i;

    for (i = 0; i < rep; i++)
    {
        k = *(k_ptr + i);
        k *= c1;
        k = ROTL64(k, r1);
        k *= c2;

        h ^= k;

        h = ROTL64(h, r1);
        h = h * m1 + n1;
    }

    h ^= h >> 33;
    h *= 0xff51afd7ed558ccd;
    h ^= h >> 33;
    h *= 0xc4ceb9fe1a85ec53;
    h ^= h >> 33;

    return (uint32_t) h;
}

extern inline uint32_t hash(const void *key, int32_t key_size);
inline uint32_t hash(const void *key, int32_t key_size)
{
    uint32_t c1 = 5333;
    uint32_t c2 = 7177;
    uint32_t r1 = 19;
    uint32_t m1 = 11117;
    uint32_t n1 = 14011;
    uint64_t h = 42;
    uint64_t * k_ptr = (uint64_t *) key;
    uint8_t * tail_ptr = (uint8_t *) (k_ptr + key_size/8);
    uint64_t k = 0;
    uint32_t rep = key_size / 8;
    uint32_t i;

    for (i = 0; i < rep; i++)
    {
        k = *(k_ptr + i);
        k *= c1;
        k = ROTL64(k, r1);
        k *= c2;

        h ^= k;

        h = ROTL64(h, r1);
        h = h * m1 + n1;
    }

    switch (key_size & 7)
    {
        case 7: k ^= (uint64_t) (tail_ptr[6]) << 48;
        case 6: k ^= (uint64_t) (tail_ptr[5]) << 40;
        case 5: k ^= (uint64_t) (tail_ptr[4]) << 32;
        case 4: k ^= (uint64_t) (tail_ptr[3]) << 24;
        case 3: k ^= (uint64_t) (tail_ptr[2]) << 16;
        case 2: k ^= (uint64_t) (tail_ptr[1]) << 8;
        case 1: k ^= (uint64_t) (tail_ptr[0]) << 0;
                k *= c1;
                k = ROTL64(k, r1);
                k *= c2;
                
                h ^= k;

                h = ROTL64(h, r1);
                h = h * m1 + n1;
    }

    h ^= h >> 33;
    h *= 0xff51afd7ed558ccd;
    h ^= h >> 33;
    h *= 0xc4ceb9fe1a85ec53;
    h ^= h >> 33;

    return (uint32_t) h;
}

#endif
