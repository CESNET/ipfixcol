/**
 * \file bfi_manager.h
 * \author Lukas Hutak <xhutak01@stud.fit.vutbr.cz>
 * \brief Bloom filter index (header file)
 *
 * Copyright (C) 2016-2017 CESNET, z.s.p.o.
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

#ifndef BFI_MANAGER_H
#define BFI_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

/** Minimal false positive probability */
#define FPP_MIN (0.000001)
/** Maximal false positive probability */
#define FPP_MAX (1)

// Internal type
typedef struct bfi_mgr_s bfi_mgr_t;

/**
 * \brief Create a manager for a BloomFilter index
 *
 * \note
 *   An output file for current window doesn't exist. Adding records without
 *   the window configuration causes an error. To create a new window and
 *   it's storage file use bfi_mgr_new_file().
 *
 * \param[in] prob     False positive probability
 * \param[in] item_cnt Projected element count (i.e. IP address count)
 * \param[in] autosize Enable automatic recalculation of parameters based on
 *   usage.
 *
 * \warning Parameter \p prob must be in range 0.000001 - 1
 * \return On success returns a pointer to the manager. Otherwise returns
 *   NULL.
 */
bfi_mgr_t *
bfi_mgr_create(double prob, uint64_t item_cnt, bool autosize);

/**
 * \brief Destroy a manager
 *
 * If an output file exits, content of the index will be stored to the file.
 * \param[in,out] index Pointer to the manager
 */
void
bfi_mgr_destroy(bfi_mgr_t *mgr);

/**
 * \brief Create a new window
 *
 * Each index window is stored into a file.
 * First, if a previous window exists, store the index to the previous output
 * file. Second, if automatic recalculation of parameters is enabled and
 * parameters are not suitable anymore, modify the parameters of the Bloom
 * filter index. Third, clear an internal index and prepare the new window.
 *
 * \note The the output file is created after replacement by new window or by
 *   destroying of its manager.
 *
 * \param[in,out] index Pointer to a manager
 * \param[in] filename  Full path name of output file with the index.
 * \return On success returns 0. Otherwise returns non-zero value and addition
 *   of new IP addresses is not allowed.
 */
int
bfi_mgr_window_new(bfi_mgr_t *mgr, const char *filename);

/**
 * \brief Close current window
 *
 * First, if a current window exists, store the index to an appropriate output
 * file. Second, set a new window as broken. This function is useful to
 * signalize that something else is broken and the next window should not be
 * create.
 *
 * \note If automatic recalculation of parameters is enabled, new parameters are
 *   NOT calculated and information required for new recalculation is lost.
 *   Thus, direct combination with bfi_mgr_window_new() do not make sense.
 * \warning To create a new window use directly bfi_mgr_window_new() function.
 *   You DON'T have to close the windows first.
 *
 * \param[in,out] mgr Pointer to a manager
 */
void
bfi_mgr_window_close(bfi_mgr_t *mgr);

/**
 * \brief Add an IP address to an index
 * \param[in,out] index Pointer to a manager
 * \param[in] buffer Pointer to the address stored in a buffer
 * \param[in] len    Length of the buffer
 * \return On success returns 0. Otherwise (an index window is not ready)
 *   returns non-zero value.
 */
int
bfi_mgr_add(bfi_mgr_t *mgr, const uint8_t *buffer, const size_t len);

#endif // BFI_MANAGER_H
