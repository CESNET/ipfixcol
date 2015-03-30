/**
 * \file fast_hash_table.c
 * \brief Fast 4-way hash table with stash.
 * \author Matej Vido, <xvidom00@stud.fit.vutbr.cz>
 * \author Tomas Cejka, <cejkat@cesnet.cz>
 * \date 2014
 */
/*
 * Copyright (C) 2014 CESNET
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

#include "fast_hash_table.h"
#include "hashes.h"

#include <stdlib.h>
#include <string.h>

#define CHECK_AND_FREE(p) if ((p) != NULL) { \
   free(p); \
}

/**
 * Index to the lookup table is the current value of free flag from the hash table row.
 * The value on the index is the order (column) of first free item in
 * the hash table row.
 */
uint8_t lt_free_flag[] = {
   0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0
};


/**
 * Index to the lookup table is the exponent and the value on the index is
 * the power of two.
 */
uint8_t lt_pow_of_two[] = {
   1, 2, 4, 8, 16, 32, 64, 128
};


/**
 * For inserting, data getting functions.
 * First index to the lookup table is current value of the replacement vector from the hash table row.
 * Second index is order of item (column) in the hash table row, which will be replaced/updated.
 * The value in lookup table is the new value of replacement vector.
 */
uint8_t lt_replacement_vector[][4] = {
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, { 0x6C, 0x63, 0x4B, 0x1B},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, { 0x6C, 0x63, 0x4E, 0x1E}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, { 0x78, 0x63, 0x4B, 0x27},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, { 0x6C, 0x72, 0x4E, 0x2D}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, { 0x78, 0x72, 0x4B, 0x36}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, { 0x78, 0x72, 0x4E, 0x39}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, { 0x9C, 0x93, 0x4B, 0x1B},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, { 0x9C, 0x93, 0x4E, 0x1E}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, { 0xB4, 0x63, 0x87, 0x27},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   { 0x6C, 0xB1, 0x8D, 0x2D}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, { 0xB4, 0x72, 0x87, 0x36}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   { 0x78, 0xB1, 0x8D, 0x39}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, { 0xD8, 0x93, 0x87, 0x1B},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, { 0x9C, 0xD2, 0x8D, 0x1E}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, { 0xE4, 0x93, 0x87, 0x27},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   { 0x9C, 0xE1, 0x8D, 0x2D}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, { 0xB4, 0xB1, 0xC6, 0x36}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   { 0xB4, 0xB1, 0xC9, 0x39}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, { 0xD8, 0xD2, 0xC6, 0x1B}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, { 0xD8, 0xD2, 0xC9, 0x1E}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, { 0xE4, 0xD2, 0xC6, 0x27}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   { 0xD8, 0xE1, 0xC9, 0x2D}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, { 0xE4, 0xE1, 0xC6, 0x36}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   { 0xE4, 0xE1, 0xC9, 0x39}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}
};

/**
 * For removing functions.
 * First index to the lookup table is current value of the replacement vector from the hash table row.
 * Second index is order of item (column) in the hash table row, which will be removed.
 * The value in lookup table is the new value of replacement vector.
 */
uint8_t lt_replacement_vector_remove[][4] = {
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, { 0x1B, 0x1E, 0x36, 0xC6},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, { 0x1B, 0x1E, 0x39, 0xC9}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, { 0x27, 0x1E, 0x36, 0xD2},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, { 0x1B, 0x2D, 0x39, 0xD8}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, { 0x27, 0x2D, 0x36, 0xE1}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, { 0x27, 0x2D, 0x39, 0xE4}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, { 0x4B, 0x4E, 0x36, 0xC6},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, { 0x4B, 0x4E, 0x39, 0xC9}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, { 0x63, 0x1E, 0x72, 0xD2},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   { 0x1B, 0x6C, 0x78, 0xD8}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, { 0x63, 0x2D, 0x72, 0xE1}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   { 0x27, 0x6C, 0x78, 0xE4}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, { 0x87, 0x4E, 0x72, 0xC6},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, { 0x4B, 0x8D, 0x78, 0xC9}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, { 0x93, 0x4E, 0x72, 0xD2},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   { 0x4B, 0x9C, 0x78, 0xD8}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, { 0x63, 0x6C, 0xB1, 0xE1}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   { 0x63, 0x6C, 0xB4, 0xE4}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, { 0x87, 0x8D, 0xB1, 0xC6}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, { 0x87, 0x8D, 0xB4, 0xC9}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, { 0x93, 0x8D, 0xB1, 0xD2}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   { 0x87, 0x9C, 0xB4, 0xD8}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, { 0x93, 0x9C, 0xB1, 0xE1}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   { 0x93, 0x9C, 0xB4, 0xE4}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0},
   {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}, {    0,    0,    0,    0}
};


/**
 * Index to the lookup table is current replacement vector from the hash table row.
 * The value on the index is order (column) of the oldest item in the hash table row.
 * Zero also for non-existing replacement vector values.
 */
uint8_t lt_replacement_index[] = {
   0, 0, 0, 0,
   0, 0, 0, 0,
   0, 0, 0, 0,
   0, 0, 0, 0,
   0, 0, 0, 0,
   0, 0, 0, 0,
   0, 0, 0, 0,
   0, 0, 1, 0,
   0, 0, 0, 0,
   0, 0, 0, 0,
   0, 0, 0, 0,
   0, 1, 0, 0,
   0, 0, 0, 0,
   0, 0, 2, 0,
   0, 2, 0, 0,
   0, 0, 0, 0,
   0, 0, 0, 0,
   0, 0, 0, 0,
   0, 0, 0, 0,
   0, 0, 1, 0,
   0, 0, 0, 0,
   0, 0, 0, 0,
   0, 0, 0, 0,
   0, 0, 0, 0,
   0, 0, 0, 0,
   0, 0, 0, 0,
   0, 0, 0, 0,
   1, 0, 0, 0,
   0, 0, 2, 0,
   0, 0, 0, 0,
   2, 0, 0, 0,
   0, 0, 0, 0,
   0, 0, 0, 0,
   0, 0, 0, 0,
   0, 0, 0, 0,
   0, 1, 0, 0,
   0, 0, 0, 0,
   0, 0, 0, 0,
   0, 0, 0, 0,
   1, 0, 0, 0,
   0, 0, 0, 0,
   0, 0, 0, 0,
   0, 0, 0, 0,
   0, 0, 0, 0,
   0, 2, 0, 0,
   2, 0, 0, 0,
   0, 0, 0, 0,
   0, 0, 0, 0,
   0, 0, 0, 0,
   0, 0, 3, 0,
   0, 3, 0, 0,
   0, 0, 0, 0,
   0, 0, 3, 0,
   0, 0, 0, 0,
   3, 0, 0, 0,
   0, 0, 0, 0,
   0, 3, 0, 0,
   3, 0, 0, 0,
   0, 0, 0, 0,
   0, 0, 0, 0,
   0, 0, 0, 0,
   0, 0, 0, 0,
   0, 0, 0, 0,
   0, 0, 0, 0
};

/**
 * Default value of replacement vector is:
 * 0 0  0 1  1 0  1 1
 */
#define FHT_DEFAULT_REPLACEMENT_VECTOR 0x1B

/**
 * \brief Function for initializing the hash table.
 *
 * @param table_rows Number of rows in the table.
 * @param key_size   Size of key in bytes.
 * @param data_size  Size of data in bytes.
 * @param stash_size Number of items in stash.
 *
 * @return Pointer to the structure of the hash table, NULL if the memory couldn't be allocated
 *         or parameters do not meet requirements.
 */
fht_table_t * fht_init(uint32_t table_rows, uint32_t key_size, uint32_t data_size, uint32_t stash_size)
{
   //power of two
   if (!(table_rows && !(table_rows & (table_rows - 1)))) {
      return NULL;
   }
   if (!key_size) {
      return NULL;
   }
   if (!data_size) {
      return NULL;
   }
   if (stash_size & (stash_size - 1)) {
      return NULL;
   }

   //allocate table
   fht_table_t *new_table = (fht_table_t *) calloc(1, sizeof(fht_table_t));

   if (new_table == NULL) {
      return NULL;
   }

   new_table->table_rows = table_rows;
   new_table->key_size = key_size;
   new_table->data_size = data_size;
   new_table->stash_size = stash_size;

   new_table->stash_index = 0;

   //set pointer to hash function
   if (key_size == 40) {
      new_table->hash_function = &hash_40;
   } else if (key_size % 8 == 0) {
      new_table->hash_function = &hash_div8;
   } else {
      new_table->hash_function = &hash;
   }

   //allocate field of keys
   if ((new_table->key_field = (uint8_t *) calloc(key_size * table_rows * FHT_TABLE_COLS, sizeof(uint8_t))) == NULL) {
      CHECK_AND_FREE(new_table);
      return NULL;
   }

   //allocate field of datas
   if ((new_table->data_field = (uint8_t *) calloc(data_size * table_rows * FHT_TABLE_COLS, sizeof(uint8_t))) == NULL) {
      CHECK_AND_FREE(new_table->key_field);
      CHECK_AND_FREE(new_table);
      return NULL;
   }

   //allocate field of free flags
   if ((new_table->free_flag_field = (uint8_t *) calloc(table_rows, sizeof(uint8_t))) == NULL) {
      CHECK_AND_FREE(new_table->key_field);
      CHECK_AND_FREE(new_table->data_field);
      CHECK_AND_FREE(new_table);
      return NULL;
   }

   //allocate field of replacement vectors
   if ((new_table->replacement_vector_field = (uint8_t *) calloc(table_rows, sizeof(uint8_t))) == NULL) {
      CHECK_AND_FREE(new_table->key_field);
      CHECK_AND_FREE(new_table->data_field);
      CHECK_AND_FREE(new_table->free_flag_field);
      CHECK_AND_FREE(new_table);
      return NULL;
   }

   uint32_t i;
   //set replacement vectors to default values
   for (i = 0; i < new_table->table_rows; i++) {
      new_table->replacement_vector_field[i] = FHT_DEFAULT_REPLACEMENT_VECTOR;
   }

   //allocate field of stash keys
   if ((new_table->stash_key_field = (uint8_t *) calloc(stash_size * key_size, sizeof(uint8_t))) == NULL) {
      CHECK_AND_FREE(new_table->key_field);
      CHECK_AND_FREE(new_table->data_field);
      CHECK_AND_FREE(new_table->free_flag_field);
      CHECK_AND_FREE(new_table->replacement_vector_field);
      CHECK_AND_FREE(new_table);
      return NULL;
   }

   //allocate field of stash datas
   if ((new_table->stash_data_field = (uint8_t *) calloc(stash_size * data_size, sizeof(uint8_t))) == NULL) {
      CHECK_AND_FREE(new_table->key_field);
      CHECK_AND_FREE(new_table->data_field);
      CHECK_AND_FREE(new_table->free_flag_field);
      CHECK_AND_FREE(new_table->replacement_vector_field);
      CHECK_AND_FREE(new_table->stash_key_field);
      CHECK_AND_FREE(new_table);
      return NULL;
   }

   //allocate field of stash free flags
   if ((new_table->stash_free_flag_field = (uint8_t *) calloc(stash_size, sizeof(uint8_t))) == NULL) {
      CHECK_AND_FREE(new_table->key_field);
      CHECK_AND_FREE(new_table->data_field);
      CHECK_AND_FREE(new_table->free_flag_field);
      CHECK_AND_FREE(new_table->replacement_vector_field);
      CHECK_AND_FREE(new_table->stash_key_field);
      CHECK_AND_FREE(new_table->stash_data_field);
      CHECK_AND_FREE(new_table);
      return NULL;
   }

   //allocate wrt locks for table
   if ((new_table->lock_table = (int8_t *) calloc(table_rows, sizeof(int8_t))) == NULL) {
      CHECK_AND_FREE(new_table->key_field);
      CHECK_AND_FREE(new_table->data_field);
      CHECK_AND_FREE(new_table->free_flag_field);
      CHECK_AND_FREE(new_table->replacement_vector_field);
      CHECK_AND_FREE(new_table->stash_key_field);
      CHECK_AND_FREE(new_table->stash_data_field);
      CHECK_AND_FREE(new_table->stash_free_flag_field);
      CHECK_AND_FREE(new_table);
      return NULL;
   }

   return new_table;
#undef X
}

/**
 * \brief Function for inserting the item into the table without using stash.
 *
 * @param table         Pointer to the hash table structure.
 * @param key           Pointer to key of the inserted item.
 * @param data          Pointer to data of the inserted item.
 * @param key_lost      Pointer to memory, where key of the replaced item will be inserted.
 *                      If NULL, key of that item will be lost.
 * @param data_lost     Pointer to memory, where data of the replaced item will be inserted.
 *                      If NULL, data of that item will be lost.
 *
 * @return              FHT_INSERT_OK if the item was successfully inserted.
 *                      FHT_INSERT_LOST if the inserted item pulled out the oldest item in the row of the table.
 *                      FHT_INSERT_FAILED if there already is an item with such key in the table.
 */
int fht_insert(fht_table_t *table, const void *key, const void *data, void *key_lost, void *data_lost)
{
   unsigned long long table_row = (table->table_rows - 1) & (table->hash_function)(key, table->key_size);
   unsigned long long table_col_row = table_row * FHT_TABLE_COLS;

   //lock row
   while (__sync_lock_test_and_set(&table->lock_table[table_row], 1))
      ;
   //

   //looking for item
   if ((table->free_flag_field[table_row] & 0x01U) && !memcmp(&table->key_field[table_col_row * table->key_size], key, table->key_size)) {
      table->replacement_vector_field[table_row] = lt_replacement_vector[table->replacement_vector_field[table_row]][0];

      //unlock row
      __sync_lock_release(&table->lock_table[table_row]);
      //

      return FHT_INSERT_FAILED;
   }

   if ((table->free_flag_field[table_row] & 0x02U) && !memcmp(&table->key_field[(table_col_row + 1) * table->key_size], key, table->key_size)) {
      table->replacement_vector_field[table_row] = lt_replacement_vector[table->replacement_vector_field[table_row]][1];

      //unlock row
      __sync_lock_release(&table->lock_table[table_row]);
      //

      return FHT_INSERT_FAILED;
   }

   if ((table->free_flag_field[table_row] & 0x04U) && !memcmp(&table->key_field[(table_col_row + 2) * table->key_size], key, table->key_size)) {
      table->replacement_vector_field[table_row] = lt_replacement_vector[table->replacement_vector_field[table_row]][2];

      //unlock row
      __sync_lock_release(&table->lock_table[table_row]);
      //

      return FHT_INSERT_FAILED;
   }

   if ((table->free_flag_field[table_row] & 0x08U) && !memcmp(&table->key_field[(table_col_row + 3) * table->key_size], key, table->key_size)) {
      table->replacement_vector_field[table_row] = lt_replacement_vector[table->replacement_vector_field[table_row]][3];

      //unlock row
      __sync_lock_release(&table->lock_table[table_row]);
      //

      return FHT_INSERT_FAILED;
   }

   if (table->free_flag_field[table_row] < FHT_COL_FULL) {
      //insert item
      memcpy(&table->key_field[(table_col_row + lt_free_flag[table->free_flag_field[table_row]]) * table->key_size], key, table->key_size);
      memcpy(&table->data_field[(table_col_row + lt_free_flag[table->free_flag_field[table_row]]) * table->data_size], data, table->data_size);

      //change replacement vector
      table->replacement_vector_field[table_row] = lt_replacement_vector[table->replacement_vector_field[table_row]][lt_free_flag[table->free_flag_field[table_row]]];

      //change free flag
      table->free_flag_field[table_row] += lt_pow_of_two[lt_free_flag[table->free_flag_field[table_row]]];

      //unlock row
      __sync_lock_release(&table->lock_table[table_row]);
      //

      return FHT_INSERT_OK;
   } else {
      if (key_lost != NULL) {
         memcpy(key_lost, &table->key_field[(table_col_row + lt_replacement_index[table->replacement_vector_field[table_row]]) * table->key_size], table->key_size);
      }
      if (data_lost != NULL) {
         memcpy(data_lost, &table->data_field[(table_col_row + lt_replacement_index[table->replacement_vector_field[table_row]]) * table->data_size], table->data_size);
      }

      //replace oldest item
      memcpy(&table->key_field[(table_col_row + lt_replacement_index[table->replacement_vector_field[table_row]]) * table->key_size], key, table->key_size);
      memcpy(&table->data_field[(table_col_row + lt_replacement_index[table->replacement_vector_field[table_row]]) * table->data_size], data, table->data_size);

      //change replacement vector
      table->replacement_vector_field[table_row] = lt_replacement_vector[table->replacement_vector_field[table_row]][lt_replacement_index[table->replacement_vector_field[table_row]]];

      //unlock row
      __sync_lock_release(&table->lock_table[table_row]);
      //

      return FHT_INSERT_LOST;
   }
}

/**
 * \brief Function for inserting the item into the table without using stash
 *        and without replacing the oldest item when the row is full.
 *
 * @param table         Pointer to the hash table structure.
 * @param key           Pointer to key of the inserted item.
 * @param data          Pointer to data of the inserted item.
 *
 * @return              FHT_INSERT_OK if the item was successfully inserted.
 *                      FHT_INSERT_FAILED if there already is an item with such key in the table.
 *                      FHT_INSERT_FULL if the row where the item should be placed is full.
 */
int fht_insert_wr(fht_table_t *table, const void *key, const void *data)
{
   unsigned long long table_row = (table->table_rows - 1) & (table->hash_function)(key, table->key_size);
   unsigned long long table_col_row = table_row * FHT_TABLE_COLS;

   //lock row
   while (__sync_lock_test_and_set(&table->lock_table[table_row], 1))
      ;
   //

   //looking for item
   if ((table->free_flag_field[table_row] & 0x01U) && !memcmp(&table->key_field[table_col_row * table->key_size], key, table->key_size)) {
      table->replacement_vector_field[table_row] = lt_replacement_vector[table->replacement_vector_field[table_row]][0];

      //unlock row
      __sync_lock_release(&table->lock_table[table_row]);
      //

      return FHT_INSERT_FAILED;
   }

   if ((table->free_flag_field[table_row] & 0x02U) && !memcmp(&table->key_field[(table_col_row + 1) * table->key_size], key, table->key_size)) {
      table->replacement_vector_field[table_row] = lt_replacement_vector[table->replacement_vector_field[table_row]][1];

      //unlock row
      __sync_lock_release(&table->lock_table[table_row]);
      //

      return FHT_INSERT_FAILED;
   }

   if ((table->free_flag_field[table_row] & 0x04U) && !memcmp(&table->key_field[(table_col_row + 2) * table->key_size], key, table->key_size)) {
      table->replacement_vector_field[table_row] = lt_replacement_vector[table->replacement_vector_field[table_row]][2];

      //unlock row
      __sync_lock_release(&table->lock_table[table_row]);
      //

      return FHT_INSERT_FAILED;
   }

   if ((table->free_flag_field[table_row] & 0x08U) && !memcmp(&table->key_field[(table_col_row + 3) * table->key_size], key, table->key_size)) {
      table->replacement_vector_field[table_row] = lt_replacement_vector[table->replacement_vector_field[table_row]][3];

      //unlock row
      __sync_lock_release(&table->lock_table[table_row]);
      //

      return FHT_INSERT_FAILED;
   }

   if (table->free_flag_field[table_row] < FHT_COL_FULL) {
      //insert item
      memcpy(&table->key_field[(table_col_row + lt_free_flag[table->free_flag_field[table_row]]) * table->key_size], key, table->key_size);
      memcpy(&table->data_field[(table_col_row + lt_free_flag[table->free_flag_field[table_row]]) * table->data_size], data, table->data_size);

      //change replacement vector
      table->replacement_vector_field[table_row] = lt_replacement_vector[table->replacement_vector_field[table_row]][lt_free_flag[table->free_flag_field[table_row]]];

      //change free flag
      table->free_flag_field[table_row] += lt_pow_of_two[lt_free_flag[table->free_flag_field[table_row]]];

      //unlock row
      __sync_lock_release(&table->lock_table[table_row]);
      //

      return FHT_INSERT_OK;
   } else {
      //unlock row
      __sync_lock_release(&table->lock_table[table_row]);
      //

      return FHT_INSERT_FULL;
   }
}

/**
 * \brief Function for inserting the item into the table using stash.
 *
 * @param table         Pointer to the hash table structure.
 * @param key           Pointer to key of the inserted item.
 * @param data          Pointer to data of the inserted item.
 * @param key_lost      Pointer to memory, where key of the replaced item will be inserted.
 *                      If NULL, key of that item will be lost.
 * @param data_lost     Pointer to memory, where data of the replaced item will be inserted.
 *                      If NULL, data of that item will be lost.
 *
 * @return              FHT_INSERT_OK if the item was successfully inserted.
 *                      FHT_INSERT_LOST if the inserted item pulled out the oldest item in the row of the table
 *                          and it was not inserted in stash.
 *                      FHT_INSERT_STASH_OK if the inserted item pulled out the oldest item in the row of the table
 *                          and it was inserted in stash.
 *                      FHT_INSERT_STASH_LOST if item inserted in stash replaced an item in stash.
 *                      FHT_INSERT_FAILED if there already is an item with such key in the table.
 */
int fht_insert_with_stash(fht_table_t *table, const void *key, const void *data, void *key_lost, void *data_lost)
{
   unsigned long long table_row = (table->table_rows - 1) & (table->hash_function)(key, table->key_size);
   unsigned long long table_col_row = table_row * FHT_TABLE_COLS;
   uint32_t i;

   //lock row
   while (__sync_lock_test_and_set(&table->lock_table[table_row], 1))
      ;
   //

   //looking for item
   if ((table->free_flag_field[table_row] & 0x01U) && !memcmp(&table->key_field[table_col_row * table->key_size], key, table->key_size)) {
      table->replacement_vector_field[table_row] = lt_replacement_vector[table->replacement_vector_field[table_row]][0];

      //unlock row
      __sync_lock_release(&table->lock_table[table_row]);
      //

      return FHT_INSERT_FAILED;
   }

   if ((table->free_flag_field[table_row] & 0x02U) && !memcmp(&table->key_field[(table_col_row + 1) * table->key_size], key, table->key_size)) {
      table->replacement_vector_field[table_row] = lt_replacement_vector[table->replacement_vector_field[table_row]][1];

      //unlock row
      __sync_lock_release(&table->lock_table[table_row]);
      //

      return FHT_INSERT_FAILED;
   }

   if ((table->free_flag_field[table_row] & 0x04U) && !memcmp(&table->key_field[(table_col_row + 2) * table->key_size], key, table->key_size)) {
      table->replacement_vector_field[table_row] = lt_replacement_vector[table->replacement_vector_field[table_row]][2];

      //unlock row
      __sync_lock_release(&table->lock_table[table_row]);
      //

      return FHT_INSERT_FAILED;
   }

   if ((table->free_flag_field[table_row] & 0x08U) && !memcmp(&table->key_field[(table_col_row + 3) * table->key_size], key, table->key_size)) {
      table->replacement_vector_field[table_row] = lt_replacement_vector[table->replacement_vector_field[table_row]][3];

      //unlock row
      __sync_lock_release(&table->lock_table[table_row]);
      //

      return FHT_INSERT_FAILED;
   }

   //searching in stash
   //lock stash
   while (__sync_lock_test_and_set(&table->lock_stash, 1))
      ;
   //
   for (i = 0; i < table->stash_size; i++) {
      if (table->stash_free_flag_field[i] && !memcmp(&table->stash_key_field[i * table->key_size], key, table->key_size)) {
         //unlock stash
         __sync_lock_release(&table->lock_stash);
         //

         //unlock row
         __sync_lock_release(&table->lock_table[table_row]);
         //
         return FHT_INSERT_FAILED;
      }
   }

   //unlock stash
   __sync_lock_release(&table->lock_stash);
   //

   if (table->free_flag_field[table_row] < FHT_COL_FULL) {
      //insert item
      memcpy(&table->key_field[(table_col_row + lt_free_flag[table->free_flag_field[table_row]]) * table->key_size], key, table->key_size);
      memcpy(&table->data_field[(table_col_row + lt_free_flag[table->free_flag_field[table_row]]) * table->data_size], data, table->data_size);

      //change replacement vector
      table->replacement_vector_field[table_row] = lt_replacement_vector[table->replacement_vector_field[table_row]][lt_free_flag[table->free_flag_field[table_row]]];

      //change free flag
      table->free_flag_field[table_row] += lt_pow_of_two[lt_free_flag[table->free_flag_field[table_row]]];

      //unlock row
      __sync_lock_release(&table->lock_table[table_row]);
      //

      return FHT_INSERT_OK;
   } else {
      int ret = FHT_INSERT_LOST;

      if (table->stash_size > 0) {
         //lock stash
         while (__sync_lock_test_and_set(&table->lock_stash, 1))
            ;
         //

         if (!table->stash_free_flag_field[table->stash_index]) {
            ret = FHT_INSERT_STASH_OK;
         } else {
            if (key_lost != NULL) {
               memcpy(key_lost, &table->stash_key_field[table->stash_index * table->key_size], table->key_size);
            }
            if (data_lost != NULL) {
               memcpy(data_lost, &table->stash_data_field[table->stash_index * table->data_size], table->data_size);
            }

            ret = FHT_INSERT_STASH_LOST;
         }

         //insert oldest item to stash
         memcpy(&table->stash_key_field[table->stash_index * table->key_size], &table->key_field[(table_col_row + lt_replacement_index[table->replacement_vector_field[table_row]]) * table->key_size], table->key_size);
         memcpy(&table->stash_data_field[table->stash_index * table->data_size], &table->data_field[(table_col_row + lt_replacement_index[table->replacement_vector_field[table_row]]) * table->data_size], table->data_size);

         table->stash_free_flag_field[table->stash_index] = 1;
         table->stash_index++;
         table->stash_index &= table->stash_size - 1;

         //unlock stash
         __sync_lock_release(&table->lock_stash);
         //
      } else {
         if (key_lost != NULL) {
            memcpy(key_lost, &table->key_field[(table_col_row + lt_replacement_index[table->replacement_vector_field[table_row]]) * table->key_size], table->key_size);
         }
         if (data_lost != NULL) {
            memcpy(data_lost, &table->data_field[(table_col_row + lt_replacement_index[table->replacement_vector_field[table_row]]) * table->data_size], table->data_size);
         }
      }

      //replace oldest item in row (it is now placed in stash)
      memcpy(&table->key_field[(table_col_row + lt_replacement_index[table->replacement_vector_field[table_row]]) * table->key_size], key, table->key_size);
      memcpy(&table->data_field[(table_col_row + lt_replacement_index[table->replacement_vector_field[table_row]]) * table->data_size], data, table->data_size);

      //change replacement vector
      table->replacement_vector_field[table_row] = lt_replacement_vector[table->replacement_vector_field[table_row]][lt_replacement_index[table->replacement_vector_field[table_row]]];

      //unlock row
      __sync_lock_release(&table->lock_table[table_row]);
      //

      return ret;
   }
}

/**
 * \brief Function for inserting the item into the table using stash
 *        and without replacing items in stash.
 *
 * @param table         Pointer to the hash table structure.
 * @param key           Pointer to key of the inserted item.
 * @param data          Pointer to data of the inserted item.
 *
 * @return              FHT_INSERT_OK if the item was successfully inserted.
 *                      FHT_INSERT_STASH_OK if the inserted item pulled out the oldest item in the row of the table
 *                          and it was inserted in stash.
 *                      FHT_INSERT_FAILED if there already is an item with such key in the table.
 *                      FHT_INSERT_FULL if the row where the item should be placed is full.
 */
int fht_insert_with_stash_wr(fht_table_t *table, const void *key, const void *data)
{
   unsigned long long table_row = (table->table_rows - 1) & (table->hash_function)(key, table->key_size);
   unsigned long long table_col_row = table_row * FHT_TABLE_COLS;
   uint32_t i;

   //lock row
   while (__sync_lock_test_and_set(&table->lock_table[table_row], 1))
      ;
   //

   //looking for item
   if ((table->free_flag_field[table_row] & 0x01U) && !memcmp(&table->key_field[table_col_row * table->key_size], key, table->key_size)) {
      table->replacement_vector_field[table_row] = lt_replacement_vector[table->replacement_vector_field[table_row]][0];

      //unlock row
      __sync_lock_release(&table->lock_table[table_row]);
      //

      return FHT_INSERT_FAILED;
   }

   if ((table->free_flag_field[table_row] & 0x02U) && !memcmp(&table->key_field[(table_col_row + 1) * table->key_size], key, table->key_size)) {
      table->replacement_vector_field[table_row] = lt_replacement_vector[table->replacement_vector_field[table_row]][1];

      //unlock row
      __sync_lock_release(&table->lock_table[table_row]);
      //

      return FHT_INSERT_FAILED;
   }

   if ((table->free_flag_field[table_row] & 0x04U) && !memcmp(&table->key_field[(table_col_row + 2) * table->key_size], key, table->key_size)) {
      table->replacement_vector_field[table_row] = lt_replacement_vector[table->replacement_vector_field[table_row]][2];

      //unlock row
      __sync_lock_release(&table->lock_table[table_row]);
      //

      return FHT_INSERT_FAILED;
   }

   if ((table->free_flag_field[table_row] & 0x08U) && !memcmp(&table->key_field[(table_col_row + 3) * table->key_size], key, table->key_size)) {
      table->replacement_vector_field[table_row] = lt_replacement_vector[table->replacement_vector_field[table_row]][3];

      //unlock row
      __sync_lock_release(&table->lock_table[table_row]);
      //

      return FHT_INSERT_FAILED;
   }

   //searching in stash
   //lock stash
   while (__sync_lock_test_and_set(&table->lock_stash, 1))
      ;
   //
   for (i = 0; i < table->stash_size; i++) {
      if (table->stash_free_flag_field[i] && !memcmp(&table->stash_key_field[i * table->key_size], key, table->key_size)) {
         //unlock stash
         __sync_lock_release(&table->lock_stash);
         //

         //unlock row
         __sync_lock_release(&table->lock_table[table_row]);
         //
         return FHT_INSERT_FAILED;
      }
   }

   //unlock stash
   __sync_lock_release(&table->lock_stash);
   //

   if (table->free_flag_field[table_row] < FHT_COL_FULL) {
      //insert item
      memcpy(&table->key_field[(table_col_row + lt_free_flag[table->free_flag_field[table_row]]) * table->key_size], key, table->key_size);
      memcpy(&table->data_field[(table_col_row + lt_free_flag[table->free_flag_field[table_row]]) * table->data_size], data, table->data_size);

      //change replacement vector
      table->replacement_vector_field[table_row] = lt_replacement_vector[table->replacement_vector_field[table_row]][lt_free_flag[table->free_flag_field[table_row]]];

      //change free flag
      table->free_flag_field[table_row] += lt_pow_of_two[lt_free_flag[table->free_flag_field[table_row]]];

      //unlock row
      __sync_lock_release(&table->lock_table[table_row]);
      //

      return FHT_INSERT_OK;
   } else {
      if (table->stash_size > 0) {
         //lock stash
         while (__sync_lock_test_and_set(&table->lock_stash, 1))
            ;
         //

         for (i = 0; i < table->stash_size; i++) {
            if (!table->stash_free_flag_field[i]) {
               //insert oldest item to stash
               memcpy(&table->stash_key_field[i * table->key_size], &table->key_field[(table_col_row + lt_replacement_index[table->replacement_vector_field[table_row]]) * table->key_size], table->key_size);
               memcpy(&table->stash_data_field[i * table->data_size], &table->data_field[(table_col_row + lt_replacement_index[table->replacement_vector_field[table_row]]) * table->data_size], table->data_size);

               table->stash_free_flag_field[i] = 1;
               table->stash_index++;
               table->stash_index &= table->stash_size - 1;

               //unlock stash
               __sync_lock_release(&table->lock_stash);
               //

               //insert new item to the row (replaces the oldest item which is now in stash)
               memcpy(&table->key_field[(table_col_row + lt_replacement_index[table->replacement_vector_field[table_row]]) * table->key_size], key, table->key_size);
               memcpy(&table->data_field[(table_col_row + lt_replacement_index[table->replacement_vector_field[table_row]]) * table->data_size], data, table->data_size);

               //change replacement vector
               table->replacement_vector_field[table_row] = lt_replacement_vector[table->replacement_vector_field[table_row]][lt_replacement_index[table->replacement_vector_field[table_row]]];

               //unlock row
               __sync_lock_release(&table->lock_table[table_row]);
               //

               return FHT_INSERT_STASH_OK;
            }
         }

         //unlock stash
         __sync_lock_release(&table->lock_stash);
         //
      }

      //unlock row
      __sync_lock_release(&table->lock_table[table_row]);
      //

      return FHT_INSERT_FULL;
   }
}

/**
 * \brief Function for removing item from the table without looking for in stash.
 *
 * @param table         Pointer to the hash table structure.
 * @param key           Key of item which will be removed.
 *
 * @return              0 if item is found and removed.
 *                      1 if item is not found and not removed.
 */
int fht_remove(fht_table_t *table, const void *key)
{
   unsigned long long table_row = (table->table_rows - 1) & (table->hash_function)(key, table->key_size);
   unsigned long long table_col_row = table_row * FHT_TABLE_COLS;
   unsigned int i;

   //lock row
   while (__sync_lock_test_and_set(&table->lock_table[table_row], 1))
      ;
   //

   for (i = 0; i < FHT_TABLE_COLS; i++) {
      if ((table->free_flag_field[table_row] & (1 << i)) && !memcmp(&table->key_field[(table_col_row + i) * table->key_size], key, table->key_size)) {
         table->replacement_vector_field[table_row] = lt_replacement_vector_remove[table->replacement_vector_field[table_row]][i];
         table->free_flag_field[table_row] &= ~(1 << i);

         //unlock row
         __sync_lock_release(&table->lock_table[table_row]);
         //

         return 0;
      }
   }

   //unlock row
   __sync_lock_release(&table->lock_table[table_row]);
   //

   return 1;
}

/**
 * \brief Function for removing item from the table without looking for in stash.
 *        Function does not locks lock, it can be used only to remove item which is locked
 *        (after use of function fht_get_data_locked or fht_get_data_with_stash_locked).
 *
 * @param table         Pointer to the hash table structure.
 * @param key           Key of item which will be removed.
 * @param lock_ptr      Pointer to lock of the table row, where the item is.
 *
 * @return              0 if item is found. Item is removed and ROW IS UNLOCKED!!
 *                      1 if item is not found and not removed. ROW REMAINS LOCKED!!
 */
int fht_remove_locked(fht_table_t *table, const void *key, int8_t *lock_ptr)
{
   unsigned long long table_row = (table->table_rows - 1) & (table->hash_function)(key, table->key_size);
   unsigned long long table_col_row = table_row * FHT_TABLE_COLS;
   unsigned int i;

   if (lock_ptr == &table->lock_table[table_row]) {
      for (i = 0; i < FHT_TABLE_COLS; i++) {
         if ((table->free_flag_field[table_row] & (1 << i)) && !memcmp(&table->key_field[(table_col_row + i) * table->key_size], key, table->key_size)) {
            table->replacement_vector_field[table_row] = lt_replacement_vector_remove[table->replacement_vector_field[table_row]][i];
            table->free_flag_field[table_row] &= ~(1 << i);

            //unlock row
            __sync_lock_release(&table->lock_table[table_row]);
            //

            return 0;
         }
      }
   }

   return 1;
}

/**
 * \brief Function for removing item from the table with looking for in stash.
 *
 * @param table         Pointer to the hash table structure.
 * @param key           Key of item which will be removed.
 *
 * @return              0 if item is found and removed.
 *                      1 if item is not found and not removed.
 */
int fht_remove_with_stash(fht_table_t *table, const void *key)
{
   unsigned long long table_row = (table->table_rows - 1) & (table->hash_function)(key, table->key_size);
   unsigned long long table_col_row = table_row * FHT_TABLE_COLS;
   unsigned int i;

   //lock row
   while (__sync_lock_test_and_set(&table->lock_table[table_row], 1))
      ;
   //

   for (i = 0; i < FHT_TABLE_COLS; i++) {
      if ((table->free_flag_field[table_row] & (1 << i)) && !memcmp(&table->key_field[(table_col_row + i) * table->key_size], key, table->key_size)) {
         table->replacement_vector_field[table_row] = lt_replacement_vector_remove[table->replacement_vector_field[table_row]][i];
         table->free_flag_field[table_row] &= ~(1 << i);

         //unlock row
         __sync_lock_release(&table->lock_table[table_row]);
         //

         return 0;
      }
   }

   //unlock row
   __sync_lock_release(&table->lock_table[table_row]);
   //

   //searching in stash
   //lock stash
   while (__sync_lock_test_and_set(&table->lock_stash, 1))
      ;
   //
   for (i = 0; i < table->stash_size; i++) {
      if (table->stash_free_flag_field[i] && !memcmp(&table->stash_key_field[i * table->key_size], key, table->key_size)) {
         table->stash_free_flag_field[i] = 0;

         //unlock stash
         __sync_lock_release(&table->lock_stash);
         //

         return 0;
      }
   }

   //unlock stash
   __sync_lock_release(&table->lock_stash);
   //
   return 1;
}

/**
 * \brief Function for removing item from the table with looking for in stash.
 *        Function does not locks lock, it can be used only to remove item which is locked
 *        (after use of function fht_get_data_locked or fht_get_data_with_stash_locked).
 *
 * @param table         Pointer to the hash table structure.
 * @param key           Key of item which will be removed.
 * @param lock_ptr      Pointer to lock of the table row or stash, where the item is.
 *
 * @return              0 if item is found. Item is removed and ROW/STASH IS UNLOCKED!!
 *                      1 if item is not found and not removed. ROW/STASH REMAINS LOCKED!!
 */
int fht_remove_with_stash_locked(fht_table_t *table, const void *key, int8_t *lock_ptr)
{
   unsigned long long table_row = (table->table_rows - 1) & (table->hash_function)(key, table->key_size);
   unsigned long long table_col_row = table_row * FHT_TABLE_COLS;
   unsigned int i;

   if (lock_ptr == &table->lock_table[table_row]) {
      for (i = 0; i < FHT_TABLE_COLS; i++) {
         if ((table->free_flag_field[table_row] & (1 << i)) && !memcmp(&table->key_field[(table_col_row + i) * table->key_size], key, table->key_size)) {
            table->replacement_vector_field[table_row] = lt_replacement_vector_remove[table->replacement_vector_field[table_row]][i];
            table->free_flag_field[table_row] &= ~(1 << i);

            //unlock row
            __sync_lock_release(&table->lock_table[table_row]);
            //

            return 0;
         }
      }
   } else if (lock_ptr == &table->lock_stash) {
      //searching in stash
      for (i = 0; i < table->stash_size; i++) {
         if (table->stash_free_flag_field[i] && !memcmp(&table->stash_key_field[i * table->key_size], key, table->key_size)) {
            table->stash_free_flag_field[i] = 0;

            //unlock stash
            __sync_lock_release(&table->lock_stash);
            //

            return 0;
         }
      }
   }

   return 1;
}

/**
 * \brief Function for removing actual item from the table when using iterator.
 *
 * @param iter          Pointer to iterator structure.
 *
 * @return              0 if item is removed.
 *                      1 if item is not removed.
 */
int fht_remove_iter(fht_iter_t *iter)
{
   switch(iter->row) {
   case FHT_ITER_START:
   case FHT_ITER_END:
      return 1;

   case FHT_ITER_STASH:
      if (iter->table->stash_free_flag_field[iter->col]) {
         iter->table->stash_free_flag_field[iter->col] = 0;
         iter->key_ptr = NULL;
         iter->data_ptr = NULL;
         return 0;
      }
      return 1;

   default:
      if (iter->table->free_flag_field[iter->row] & (1 << iter->col)) {
         iter->table->replacement_vector_field[iter->row] = lt_replacement_vector_remove[iter->table->replacement_vector_field[iter->row]][iter->col];
         iter->table->free_flag_field[iter->row] &= ~(1 << iter->col);
         iter->key_ptr = NULL;
         iter->data_ptr = NULL;
         return 0;
      }
      return 1;
   }
}

/**
 * \brief Function for clearing the table.
 *
 * @param table     Pointer to the hash table structure.
 */
void fht_clear(fht_table_t *table)
{
   unsigned long long i;

   for (i = 0; i < table->table_rows; i++) {
      //lock row
      while (__sync_lock_test_and_set(&table->lock_table[i], 1))
         ;
      //

      table->free_flag_field[i] = 0;

      //unlock row
      __sync_lock_release(&table->lock_table[i]);
      //
   }

   //lock stash
   while (__sync_lock_test_and_set(&table->lock_stash, 1))
      ;
   //
   for (i = 0; i < table->stash_size; i++)
      table->stash_free_flag_field[i] = 0;
   //unlock stash
   __sync_lock_release(&table->lock_stash);
   //
}

/**
 * \brief Function for destroying the table and freeing memory.
 *
 * @param table     Pointer to the hash table structure.
 */
void fht_destroy(fht_table_t *table)
{
   CHECK_AND_FREE(table->key_field);
   CHECK_AND_FREE(table->data_field);
   CHECK_AND_FREE(table->free_flag_field);
   CHECK_AND_FREE(table->replacement_vector_field);
   CHECK_AND_FREE(table->stash_key_field);
   CHECK_AND_FREE(table->stash_data_field);
   CHECK_AND_FREE(table->stash_free_flag_field);
   CHECK_AND_FREE(table->lock_table);
   CHECK_AND_FREE(table);
}

/**
 * \brief Function for initializing iterator for the table.
 *
 * @param table     Pointer to the hash table structure.
 *
 * @return          Pointer to the iterator structure.
 *                  NULL if could not allocate memory.
 */
fht_iter_t * fht_init_iter(fht_table_t *table)
{
   if (table == NULL) {
      return NULL;
   }

   fht_iter_t * new_iter = (fht_iter_t *) calloc(1, sizeof(fht_iter_t));

   if (new_iter == NULL) {
      return NULL;
   }

   new_iter->table = table;
   new_iter->row = FHT_ITER_START;
   new_iter->col = FHT_ITER_START;
   new_iter->key_ptr = NULL;
   new_iter->data_ptr = NULL;

   return new_iter;
}

/**
 * \brief Function for reinitializing iterator for the table.
 *
 * @param iter      Pointer to the existing iterator.
 */
void fht_reinit_iter(fht_iter_t *iter)
{
   if (iter->row == FHT_ITER_STASH) {
      //unlock stash
      __sync_lock_release(&iter->table->lock_stash);
   } else if (iter->row >= 0) {
      //unlock row
      __sync_lock_release(&iter->table->lock_table[iter->row]);
   }
   iter->row = FHT_ITER_START;
   iter->col = FHT_ITER_START;
   iter->key_ptr = NULL;
   iter->data_ptr = NULL;
}

/**
 * \brief Function for getting next item in the table.
 *
 * @param iter      Pointer to the iterator structure.
 *
 * @return          FHT_ITER_RET_OK if iterator structure contain next structure.
 *                  FHT_ITER_RET_END if iterator is in the end of the table and does not
 *                  contain any other item.
 */
int32_t fht_get_next_iter(fht_iter_t *iter)
{
   uint32_t i, j;

   switch (iter->row) {
   default:
      for (j = iter->col + 1; j < FHT_TABLE_COLS; j++) {
         if (iter->table->free_flag_field[iter->row] & (1 << j)) {
            iter->col = j;
            iter->key_ptr = &iter->table->key_field[(iter->row * FHT_TABLE_COLS + j) * iter->table->key_size];
            iter->data_ptr = &iter->table->data_field[(iter->row * FHT_TABLE_COLS + j) * iter->table->data_size];

            return FHT_ITER_RET_OK;
         }
      }
      //unlock row
      __sync_lock_release(&iter->table->lock_table[iter->row]);
      //
   case FHT_ITER_START:
      for (i = iter->row + 1; i < iter->table->table_rows; i++) {
         //lock row
         while (__sync_lock_test_and_set(&iter->table->lock_table[i], 1))
            ;
         //
         for (j = 0; j < FHT_TABLE_COLS; j++) {
            if (iter->table->free_flag_field[i] & (1 << j)) {
               iter->row = i;
               iter->col = j;
               iter->key_ptr = &iter->table->key_field[(i * FHT_TABLE_COLS + j) * iter->table->key_size];
               iter->data_ptr = &iter->table->data_field[(i * FHT_TABLE_COLS + j) * iter->table->data_size];

               return FHT_ITER_RET_OK;
            }

         }
         //unlock row
         __sync_lock_release(&iter->table->lock_table[i]);
         //
      }
      //lock stash
      while (__sync_lock_test_and_set(&iter->table->lock_stash, 1))
         ;
      //
   case FHT_ITER_STASH:
      for (i = (iter->row == FHT_ITER_STASH) ? (iter->col + 1) : 0; i < iter->table->stash_size; i++) {
         if (iter->table->stash_free_flag_field[i]) {
            iter->row = FHT_ITER_STASH;
            iter->col = i;
            iter->key_ptr = &iter->table->stash_key_field[i * iter->table->key_size];
            iter->data_ptr = &iter->table->stash_data_field[i * iter->table->data_size];

            return FHT_ITER_RET_OK;
         }
      }
      //unlock stash
      __sync_lock_release(&iter->table->lock_stash);
      //
   case FHT_ITER_END:
      iter->row = FHT_ITER_END;
      iter->col = FHT_ITER_END;
      iter->key_ptr = NULL;
      iter->data_ptr = NULL;

      return FHT_ITER_RET_END;
   }
}

/**
 * \brief Function for destroying iterator and freeing memory.
 *
 * If function is used in the middle of the table, function also
 * unlocks row or stash, which is locked.
 *
 * @param iter      Pointer to the iterator structure.
 */
void fht_destroy_iter(fht_iter_t *iter)
{
   if (iter->row == FHT_ITER_STASH) {
      //unlock stash
      __sync_lock_release(&iter->table->lock_stash);
   }
   if (iter->row >= 0) {
      //unlock row
      __sync_lock_release(&iter->table->lock_table[iter->row]);
   }

   free(iter);
}

