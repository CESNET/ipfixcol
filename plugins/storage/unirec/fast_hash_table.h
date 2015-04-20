/**
 * \file fast_hash_table.h
 * \brief Fast 4-way hash table with stash - header file.
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

#ifndef __FAST_HASH_TABLE_H__
#define __FAST_HASH_TABLE_H__

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Number of columns in the hash table.
 */
#define FHT_TABLE_COLS 4

/**
 * Value of free flag when column is full.
 */
#define FHT_COL_FULL ((uint8_t) 0x000F)

/**
 * Lookup tables.
 */
extern uint8_t lt_free_flag[];
extern uint8_t lt_pow_of_two[];
extern uint8_t lt_replacement_vector[][4];
extern uint8_t lt_replacement_vector_remove[][4];
extern uint8_t lt_replacement_index[];

/**
 * Constants used for table functions.
 */
enum fht_table
{
    FHT_INSERT_OK = 0,
    FHT_INSERT_LOST = 1,
    FHT_INSERT_STASH_OK = 2,
    FHT_INSERT_STASH_LOST = 3,
    FHT_INSERT_FAILED = -1,
    FHT_INSERT_FULL = -2,
};

/**
 * Constants used for iterator functions.
 */
enum fht_iter
{
    FHT_ITER_RET_OK  =  0,
    FHT_ITER_RET_END =  1,
    FHT_ITER_START   = -1,
    FHT_ITER_STASH   = -2,
    FHT_ITER_END     = -3,
};

/**
 * Structure of the hash table.
 *
 * Replacement vector:
 * 
 * Every row has one replacement vector.
 * Every item is represented by two bits.
 * 
 * 0 0 - the 1. newest item in row
 * 0 1 - the 2. newest item in row
 * 1 0 - the 3. newest item in row
 * 1 1 - the    oldest item in row
 *
 *                        MSB                         LSB
 * Bits                  | X   X | X   X | X   X | X   X |
 * Index of item in row      3       2       1       0
 *
 *
 * Free flag:
 * 0 - free 
 * 1 - full
 *
 * Table free flag:
 *                        MSB                         LSB
 * Bits                  | 0   0   0   0 | X | X | X | X |
 * Index of item in row                    3   2   1   0
 *
 */
typedef struct
{
    uint32_t   table_rows;                                 /**< Number of rows in the table. */
    uint32_t   key_size;                                   /**< Size of key in bytes. */
    uint32_t   data_size;                                  /**< Size of data in bytes. */
    uint32_t   stash_size;                                 /**< Max number of items in stash. */
    uint32_t   stash_index;                                /**< Index to the stash, where the next item will be inserted. */
    uint8_t    *key_field;                                 /**< Pointer to array of keys. */
    uint8_t    *data_field;                                /**< Pointer to array of data. */
    uint8_t    *free_flag_field;                           /**< Pointer to array of free flags. */
    uint8_t    *replacement_vector_field;                  /**< Pointer to array of replacement_vector_field. */
    uint8_t    *stash_key_field;                           /**< Pointer to array of keys of items in stash. */
    uint8_t    *stash_data_field;                          /**< Pointer to array of data of items in stash. */
    uint8_t    *stash_free_flag_field;                     /**< Pointer to array of free flags of items in stash. */
    int8_t     *lock_table;                                /**< Pointer to array of locks for rows in the table. */
    int8_t     lock_stash;                                 /**< Lock for stash. */
    uint32_t   (*hash_function)(const void *, int32_t);    /**< Pointer to used hash function. */
} fht_table_t;

/**
 * Iterator structure.
 */
typedef struct
{
    fht_table_t * table;    /**< Pointer to the structure of table. */
    int32_t row;            /**< Value of row where the item is located. */
    int32_t col;            /**< Value of column where the item is located. */
    uint8_t *key_ptr;       /**< Pointer to the key of item. */
    uint8_t *data_ptr;      /**< Pointer to the data of item. */
} fht_iter_t;

/**
 * \brief Function for initializing the hash table. 
 * 
 * Parameters need to meet following requirements:
 * table_rows - non-zero, power of two
 * key_size   - non-zero
 * data_size  - non-zero
 * stash_size - power of two
 *
 * @param table_rows Number of rows in the table.
 * @param key_size   Size of key in bytes.
 * @param data_size  Size of data in bytes.
 * @param stash_size Number of items in stash.
 *
 * @return Pointer to the structure of the hash table, NULL if the memory couldn't be allocated
 *         or parameters do not meet requirements.
 */
fht_table_t * fht_init(uint32_t table_rows, uint32_t key_size, uint32_t data_size, uint32_t stash_size);

/**
 * \brief Function for inserting the item into the table without using stash.
 *
 * Function checks whether there already is item with the key "key" in the table.
 * If not, function inserts the item in the table. Function checks whether there is
 * free place in the current row. If yes, new item is inserted. If not, new item will replace
 * the oldest item in row according to replacement vector. Function updates replacement vector
 * and free flag.
 *
 * key_lost and data_lost are set only when the return value is FHT_INSERT_LOST.
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
int fht_insert(fht_table_t *table, const void *key, const void *data, void *key_lost, void *data_lost);

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
int fht_insert_wr(fht_table_t *table, const void *key, const void *data);

/**
 * \brief Function for inserting the item into the table using stash.
 *
 * Function checks whether there already is item with the key "key" in the table.
 * If not, function inserts the item in the table. Function checks whether there is
 * free place in the current row. If yes, new item is inserted. If not, new item will replace
 * the oldest item in row according to replacement vector and the oldest item is inserted 
 * in stash. Function updates replacement vector and free flag.
 *
 *
 * @param table         Pointer to the hash table structure.
 * @param key           Pointer to key of the inserted item.
 * @param data          Pointer to data of the inserted item.
 * @param key_lost      Pointer to memory, where key of the replaced item will be inserted.
 *                      If NULL, key of that item will be lost.
 * @param data_lost     Pointer to memory, where data of the replaced item will be inserted.
 *                      If NULL, data of that item will be lost.
 *
 * key_lost and data_lost are set only when the return value is FHT_INSERT_LOST or FHT_INSERT_STASH_LOST.
 * 
 * @return              FHT_INSERT_OK if the item was successfully inserted.
 *                      FHT_INSERT_LOST if the inserted item pulled out the oldest item in the row of the table 
 *                          and it was not inserted in stash.
 *                      FHT_INSERT_STASH_OK if the inserted item pulled out the oldest item in the row of the table 
 *                          and it was inserted in stash.
 *                      FHT_INSERT_STASH_LOST if item inserted in stash replaced an item in stash.
 *                      FHT_INSERT_FAILED if there already is an item with such key in the table.
 */
int fht_insert_with_stash(fht_table_t *table, const void *key, const void *data, void *key_lost, void *data_lost);

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
int fht_insert_with_stash_wr(fht_table_t *table, const void *key, const void *data);

/**
 * \brief Function for getting data from the table without looking for in stash, looks for by key.
 *
 * Function looks for the key and returns pointer to data belonging to the key
 * when found. Function updates replacement vector. Found item is set to be the newest.
 * Function returns unlocked data. Be careful with using pointer to data in multiple threads.
 * 
 * @param table     Pointer to the hash table structure.
 * @param key       Key of wanted item.
 
 * @return          Pointer to data if found.
 *                  NULL if not found.
 */
static inline void * fht_get_data(fht_table_t *table, const void *key)
{
   unsigned long long table_row = (table->table_rows - 1) & (table->hash_function)(key, table->key_size);
   unsigned long long table_col_row = table_row * FHT_TABLE_COLS;

   //lock row
   while (__sync_lock_test_and_set(&table->lock_table[table_row], 1))
      ;
   //

   if ((table->free_flag_field[table_row] & 0x01U) && !memcmp(&table->key_field[table_col_row * table->key_size], key, table->key_size)) {
      table->replacement_vector_field[table_row] = lt_replacement_vector[table->replacement_vector_field[table_row]][0];

      //unlock row
      __sync_lock_release(&table->lock_table[table_row]);
      //

      return (void *) &table->data_field[table_col_row * table->data_size];
   }

   if ((table->free_flag_field[table_row] & 0x02U) && !memcmp(&table->key_field[(table_col_row + 1) * table->key_size], key, table->key_size)) {
      table->replacement_vector_field[table_row] = lt_replacement_vector[table->replacement_vector_field[table_row]][1];

      //unlock row
      __sync_lock_release(&table->lock_table[table_row]);
      //

      return (void *) &table->data_field[(table_col_row + 1) * table->data_size];
   }

   if ((table->free_flag_field[table_row] & 0x04U) && !memcmp(&table->key_field[(table_col_row + 2) * table->key_size], key, table->key_size)) {
      table->replacement_vector_field[table_row] = lt_replacement_vector[table->replacement_vector_field[table_row]][2];

      //unlock row
      __sync_lock_release(&table->lock_table[table_row]);
      //

      return (void *) &table->data_field[(table_col_row + 2) * table->data_size];
   }

   if ((table->free_flag_field[table_row] & 0x08U) && !memcmp(&table->key_field[(table_col_row + 3) * table->key_size], key, table->key_size)) {
      table->replacement_vector_field[table_row] = lt_replacement_vector[table->replacement_vector_field[table_row]][3];

      //unlock row
      __sync_lock_release(&table->lock_table[table_row]);
      //

      return (void *) &table->data_field[(table_col_row + 3) * table->data_size];
   }

   //unlock row
   __sync_lock_release(&table->lock_table[table_row]);
   //

   return NULL;
}

/**
 * \brief Function for getting data from the table without looking for in stash, looks for by key.
 *        Works same way as fht_get_data, but returns locked data, need to use unlock function.
 *
 * @param table     Pointer to the hash table structure.
 * @param key       Key of wanted item.
 * @param lock      Function sets "lock" to point to lock of row where is located data of found item.
 *                  If NULL is returned "lock" is undefined and table row is unlocked.
 *
 * @return          Pointer to data if found.
 *                  NULL if not found.
 */
inline void * fht_get_data_locked(fht_table_t *table, const void *key, int8_t **lock)
{
   unsigned long long table_row = (table->table_rows - 1) & (table->hash_function)(key, table->key_size);
   unsigned long long table_col_row = table_row * FHT_TABLE_COLS;

   //lock row
   while (__sync_lock_test_and_set(&table->lock_table[table_row], 1))
      ;
   //

   if ((table->free_flag_field[table_row] & 0x01U) && !memcmp(&table->key_field[table_col_row * table->key_size], key, table->key_size)) {
      table->replacement_vector_field[table_row] = lt_replacement_vector[table->replacement_vector_field[table_row]][0];

      *lock = &table->lock_table[table_row];

      return (void *) &table->data_field[table_col_row * table->data_size];
   }

   if ((table->free_flag_field[table_row] & 0x02U) && !memcmp(&table->key_field[(table_col_row + 1) * table->key_size], key, table->key_size)) {
      table->replacement_vector_field[table_row] = lt_replacement_vector[table->replacement_vector_field[table_row]][1];

      *lock = &table->lock_table[table_row];

      return (void *) &table->data_field[(table_col_row + 1) * table->data_size];
   }

   if ((table->free_flag_field[table_row] & 0x04U) && !memcmp(&table->key_field[(table_col_row + 2) * table->key_size], key, table->key_size)) {
      table->replacement_vector_field[table_row] = lt_replacement_vector[table->replacement_vector_field[table_row]][2];

      *lock = &table->lock_table[table_row];

      return (void *) &table->data_field[(table_col_row + 2) * table->data_size];
   }

   if ((table->free_flag_field[table_row] & 0x08U) && !memcmp(&table->key_field[(table_col_row + 3) * table->key_size], key, table->key_size)) {
      table->replacement_vector_field[table_row] = lt_replacement_vector[table->replacement_vector_field[table_row]][3];

      *lock = &table->lock_table[table_row];

      return (void *) &table->data_field[(table_col_row + 3) * table->data_size];
   }

   //unlock row
   __sync_lock_release(&table->lock_table[table_row]);
   //

   return NULL;
}

/**
 * \brief Function for getting data from the table with looking for in stash, looks for by key.
 *
 * Function looks for the key and returns pointer to data belonging to the key
 * when found. Function updates replacement vector. Found item is set to be the newest.
 * Function returns unlocked data. Be careful with using pointer to data in multiple threads.
 * 
 * @param table     Pointer to the hash table structure.
 * @param key       Key of wanted item.
 
 * @return          Pointer to data if found.
 *                  NULL if not found.
 */
inline void * fht_get_data_with_stash(fht_table_t *table, const void *key)
{
   unsigned long long table_row = (table->table_rows - 1) & (table->hash_function)(key, table->key_size);
   unsigned long long table_col_row = table_row * FHT_TABLE_COLS;
   unsigned int i;

   //lock row
   while (__sync_lock_test_and_set(&table->lock_table[table_row], 1))
      ;
   //

   if ((table->free_flag_field[table_row] & 0x01U) && !memcmp(&table->key_field[table_col_row * table->key_size], key, table->key_size)) {
      table->replacement_vector_field[table_row] = lt_replacement_vector[table->replacement_vector_field[table_row]][0];

      //unlock row
      __sync_lock_release(&table->lock_table[table_row]);
      //

      return (void *) &table->data_field[table_col_row * table->data_size];
   }

   if ((table->free_flag_field[table_row] & 0x02U) && !memcmp(&table->key_field[(table_col_row + 1) * table->key_size], key, table->key_size)) {
      table->replacement_vector_field[table_row] = lt_replacement_vector[table->replacement_vector_field[table_row]][1];

      //unlock row
      __sync_lock_release(&table->lock_table[table_row]);
      //

      return (void *) &table->data_field[(table_col_row + 1) * table->data_size];
   }

   if ((table->free_flag_field[table_row] & 0x04U) && !memcmp(&table->key_field[(table_col_row + 2) * table->key_size], key, table->key_size)) {
      table->replacement_vector_field[table_row] = lt_replacement_vector[table->replacement_vector_field[table_row]][2];

      //unlock row
      __sync_lock_release(&table->lock_table[table_row]);
      //

      return (void *) &table->data_field[(table_col_row + 2) * table->data_size];
   }

   if ((table->free_flag_field[table_row] & 0x08U) && !memcmp(&table->key_field[(table_col_row + 3) * table->key_size], key, table->key_size)) {
      table->replacement_vector_field[table_row] = lt_replacement_vector[table->replacement_vector_field[table_row]][3];

      //unlock row
      __sync_lock_release(&table->lock_table[table_row]);
      //

      return (void *) &table->data_field[(table_col_row + 3) * table->data_size];
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
         //unlock stash
         __sync_lock_release(&table->lock_stash);
         //
         return (void *) &table->stash_data_field[i * table->data_size];
      }
   }

   //unlock stash
   __sync_lock_release(&table->lock_stash);
   //
   return NULL;
}

/**
 * \brief Function for getting data from the table with looking for in stash, looks for by key.
 *        Works same way as fht_get_data_with_stash, but returns locked data, need to use unlock function.
 *
 * @param table     Pointer to the hash table structure.
 * @param key       Key of wanted item.
 * @param lock      Function sets "lock" to point to the lock of row where is located data of found item.
 *                  If item is located in stash, "lock" is set to point to stash lock.
 *                  If NULL is returned "lock" is undefined and table row is unlocked.
 * 
 * @return          Pointer to data if found.
 *                  NULL if not found.
 */
inline void * fht_get_data_with_stash_locked(fht_table_t *table, const void *key, int8_t **lock)
{
   unsigned long long table_row = (table->table_rows - 1) & (table->hash_function)(key, table->key_size);
   unsigned long long table_col_row = table_row * FHT_TABLE_COLS;
   unsigned int i;

   //lock row
   while (__sync_lock_test_and_set(&table->lock_table[table_row], 1))
      ;
   //

   if ((table->free_flag_field[table_row] & 0x01U) && !memcmp(&table->key_field[table_col_row * table->key_size], key, table->key_size)) {
      table->replacement_vector_field[table_row] = lt_replacement_vector[table->replacement_vector_field[table_row]][0];

      *lock = &table->lock_table[table_row];

      return (void *) &table->data_field[table_col_row * table->data_size];
   }

   if ((table->free_flag_field[table_row] & 0x02U) && !memcmp(&table->key_field[(table_col_row + 1) * table->key_size], key, table->key_size)) {
      table->replacement_vector_field[table_row] = lt_replacement_vector[table->replacement_vector_field[table_row]][1];

      *lock = &table->lock_table[table_row];

      return (void *) &table->data_field[(table_col_row + 1) * table->data_size];
   }

   if ((table->free_flag_field[table_row] & 0x04U) && !memcmp(&table->key_field[(table_col_row + 2) * table->key_size], key, table->key_size)) {
      table->replacement_vector_field[table_row] = lt_replacement_vector[table->replacement_vector_field[table_row]][2];

      *lock = &table->lock_table[table_row];

      return (void *) &table->data_field[(table_col_row + 2) * table->data_size];
   }

   if ((table->free_flag_field[table_row] & 0x08U) && !memcmp(&table->key_field[(table_col_row + 3) * table->key_size], key, table->key_size)) {
      table->replacement_vector_field[table_row] = lt_replacement_vector[table->replacement_vector_field[table_row]][3];

      *lock = &table->lock_table[table_row];

      return (void *) &table->data_field[(table_col_row + 3) * table->data_size];
   }

   //unlock row
   __sync_lock_release(&table->lock_table[table_row]);
   //

   //searching in stash
   //lock stash
   while (__sync_lock_test_and_set(&table->lock_stash, 1))
      ;
   //
   for (i = 0; i < table->stash_size; i++)
      if (table->stash_free_flag_field[i] && !memcmp(&table->stash_key_field[i * table->key_size], key, table->key_size)) {
         *lock = &table->lock_stash;

         return (void *) &table->stash_data_field[i * table->data_size];
      }

   //unlock stash
   __sync_lock_release(&table->lock_stash);
   //
   return NULL;
}

/**
 * \brief Function for removing item from the table without looking for in stash.
 *
 * Function removes item which key is equal to "key" from the table.
 * Replacement vector and free flag are updated.
 *
 * @param table         Pointer to the hash table structure.
 * @param key           Key of item which will be removed.
 * 
 * @return              0 if item is found and removed.
 *                      1 if item is not found and not removed.
 */
int fht_remove(fht_table_t *table, const void *key);

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
int fht_remove_locked(fht_table_t *table, const void *key, int8_t *lock_ptr);

/**
 * \brief Function for removing item from the table with looking for in stash.
 *
 * Function removes item which key is equal to "key" from the table.
 * When removing from table, replacement vector and free flag are updated.
 * When removing from stash, free flag is updated.
 *
 * @param table         Pointer to the hash table structure.
 * @param key           Key of item which will be removed.
 * 
 * @return              0 if item is found and removed.
 *                      1 if item is not found and not removed.
 */
int fht_remove_with_stash(fht_table_t *table, const void *key);

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
int fht_remove_with_stash_locked(fht_table_t *table, const void *key, int8_t *lock_ptr);

/**
 * \brief Function for removing actual item from the table when using iterator.
 *
 * @param iter          Pointer to iterator structure.
 * 
 * @return              0 if item is removed.
 *                      1 if item is not removed.
 */
int fht_remove_iter(fht_iter_t *iter);

/**
 * \brief Function for clearing the table.
 *
 * Function sets replacement flags of all items in the table and in stash to zero.
 * Items with zero replacement flag are considered free. Data and keys remain in table.
 *
 * @param table     Pointer to the hash table structure.
 */
void fht_clear(fht_table_t *table);

/**
 * \brief Function for destroying the table and freeing memory.
 *
 * Function frees memory of the table structure.
 *
 * @param table     Pointer to the hash table structure.
 */
void fht_destroy(fht_table_t *table);

/**
 * \brief Function for unlocking table row/stash.
 *
 * @param lock      Pointer to lock variable.
 */
inline void fht_unlock_data(int8_t *lock)
{
   __sync_lock_release(lock);
}

/**
 * \brief Function for initializing iterator for the table.
 *
 * @param table     Pointer to the hash table structure.
 *
 * @return          Pointer to the iterator structure.
 *                  NULL if could not allocate memory.
 */
fht_iter_t * fht_init_iter(fht_table_t *table);

/**
 * \brief Function for reinitializing iterator for the table.
 *
 * @param iter      Pointer to the existing iterator.
 */
void fht_reinit_iter(fht_iter_t *iter);

/**
 * \brief Function for getting next item in the table.
 *
 * @param iter      Pointer to the iterator structure.
 *
 * @return          FHT_ITER_RET_OK if iterator structure contain next structure.
 *                  FHT_ITER_RET_END if iterator is in the end of the table and does not
 *                  contain any other item.
 */
int32_t fht_get_next_iter(fht_iter_t *iter);

/**
 * \brief Function for destroying iterator and freeing memory.
 *
 * If function is used in the middle of the table, function also 
 * unlocks row or stash, which is locked.
 *
 * @param iter      Pointer to the iterator structure.
 */
void fht_destroy_iter(fht_iter_t *iter);

#ifdef __cplusplus
}
#endif

#endif
