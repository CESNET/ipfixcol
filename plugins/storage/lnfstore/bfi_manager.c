/**
 * \file bfi_manager.c
 * \author Lukas Hutak <xhutak01@stud.fit.vutbr.cz>
 * \author Pavel Krobot <Pavel.Krobot@cesnet.cz>
 * \brief Bloom filter index (source file)
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

// Bloomfilter index library API
#include <bf_index.h>
#include <ipfixcol.h>
#include "bfi_manager.h"

#include <string.h> // strdup

extern const char* msg_module;


#define BF_TOL_COEFF(x) ((x > 10000000) ? 1.1 : (x > 100000) ? 1.2 : \
	(x > 30000) ? 1.5 : (x > 5000) ? 2 : \
	(x > 500) ? 3 : 10)

/**
 * UPPPER_TOLERANCE should be small, since real unique item count should NOT
 * be higher than bloom filter estimated item count. If there are more items
 * than expected (=estimated) real false positive probability could be higher
 * than desired f. p. probability.
 */
#define BF_UPPER_TOLERANCE(val, coeff) \
	((unsigned long)(val * (1 + coeff * 0.05)))
/*
 * LOWER_TOLERANCE could be more benevolent. In this case bloom filter size is
 * unnecessarily big. Value of LOWER_TOLERANCE depends on trade-off between
 * wasted space and frequency of bloom filter re-creation (with new
 * parameters).
 */
#define BF_LOWER_TOLERANCE(val, coeff) \
	((unsigned long)(val * (1 + coeff * ((coeff > 1.2) ? 1.3 : 0.5) )))

/** \brief Status of the manager */
enum BFI_MGR_STATUS {
	BFI_MGR_S_INIT,            /**< Before creating of the first window   */
	BFI_MGR_S_WINDOW_PARTIAL,  /**< A window that is not suitable for size
 		* recalculation of the next window. This state is used only when
 		* the autosize of the index is enabled. */
	BFI_MGR_S_WINDOW_FULL,     /**< A window that is suitable for size
 		* recalculation of the next window. */
	BFI_MGR_S_ERROR            /**< An index or output file is not ready. */
};

/** \brief Internal structure of the manager */
struct bfi_mgr_s {
	index_t *idx;           /**< Instance of a Bloom filter index             */

	struct {
		uint64_t est_items; /**< Estimated item count in a Bloom filter       */
		double   fp_prob;   /**< False positive probability of a Bloom filter */
	} cfg_bloom;            /**< Configuration of a Bloom Filter              */

	struct {
		bool  en_autosize; /**< Enable auto-size (on/off)                     */
		enum BFI_MGR_STATUS status; /**< Status of the manager                */
	} cfg_mgr;             /**< Configuration of the manager                  */
};


bfi_mgr_t *
bfi_mgr_create(double prob, uint64_t item_cnt, bool autosize)
{
	// Check parameters
	if (prob < FPP_MIN || prob > FPP_MAX) {
		MSG_ERROR(msg_module, "BFI manager error (the probability parameter is "
				"out of range).");
		return NULL;
	}

	// Create structures
	bfi_mgr_t *mgr = (bfi_mgr_t *) calloc(1, sizeof(bfi_mgr_t));
	if (!mgr) {
		// Memory allocation failed
		MSG_ERROR(msg_module, "Unable to allocate memory (%s:%d)",
			__FILE__, __LINE__);
		return NULL;
	}

	// Save parameters
	mgr->cfg_bloom.est_items = item_cnt;
	mgr->cfg_bloom.fp_prob = prob;
	mgr->cfg_mgr.en_autosize = autosize;
	mgr->cfg_mgr.status = BFI_MGR_S_INIT;

	return mgr;
}

/**
 * \brief Store/flush an BloomFilter index to an output file
 *
 * \note Output file is defined during call of bfi_mgr_new_file() function.
 * \param[in] mgr Pointer to a manager
 * \return On success returns 0. Otherwise returns a non-zero value.
 */
static int
bfi_mgr_save(const bfi_mgr_t *mgr)
{
	if (mgr->cfg_mgr.status != BFI_MGR_S_WINDOW_FULL &&
			mgr->cfg_mgr.status != BFI_MGR_S_WINDOW_PARTIAL) {
		// Index file is broken or doesn't exist
		return 1;
	}

	if (store_index(mgr->idx) != BFI_OK) {
		// TODO: add an error message
		return 1;
	}

	return 0;
}

/**
 * \brief Prepare the Bloom Filter index
 *
 * Create & initialise a new Bloom Filter index. If previous one still exists,
 * it is destroyed first.
 * \param[in,out] mgr Pointer to a manager
 * \return On success returns 0. Otherwise returns a non-zero value.
 */
static int
bfi_mgr_index_prepare(bfi_mgr_t *mgr)
{
	if (mgr->idx != NULL) {
		// Destroy previous instance
		destroy_index(mgr->idx);
	}

	// Create & initialise a new instance
	mgr->idx = create_index();
	if (!mgr->idx) {
		MSG_ERROR(msg_module, "Failed to create an Bloom Filter index.");
		// TODO: improve the error message
		return 1;
	}

	struct index_params params;
	params.est_item_cnt = mgr->cfg_bloom.est_items;
	params.fp_prob = mgr->cfg_bloom.fp_prob;
	params.indexing = true;	// TODO: these two parameters doesn't make sense!!!
	params.file_prefix = NULL;  // TODO: <--

	if (init_index(params, mgr->idx) != BFI_OK) {
		// TODO: improve the error message
		MSG_ERROR(msg_module, "Failed to create an Bloom Filter index.");
		return 1;
	}

	return 0;
}

/**
 * \brief Clear the Bloom Filter index
 * \param mgr Pointer to a manager
 * \return Always returns 0;
 */
static int
bfi_mgr_index_clear(bfi_mgr_t *mgr)
{
	clear_index(mgr->idx);
	return 0;
}

/**
 * \brief Destroy the Bloom Filter index
 * \param mgr Pointer to a manager
 */
static void
bfi_mgr_index_destroy(bfi_mgr_t *mgr)
{
	if (mgr->idx) {
		destroy_index(mgr->idx);
	}
}

void
bfi_mgr_destroy(bfi_mgr_t *mgr)
{
	if (!mgr) {
		return;
	}

	bfi_mgr_save(mgr);
	bfi_mgr_index_destroy(mgr);
	free(mgr);
}

int
bfi_mgr_window_new(bfi_mgr_t *mgr, const char *filename)
{
	int status;
	bool reinit = false;

	// Store/flush current index
	bfi_mgr_save(mgr);

	// Check status
	if (mgr->cfg_mgr.status == BFI_MGR_S_INIT ||
			mgr->cfg_mgr.status == BFI_MGR_S_ERROR) {
		reinit = true;
	}

	if (!reinit && mgr->cfg_mgr.en_autosize) {
		/*
		 * Calculate minimal & maximal expected estimate (item count in Bloom
		 * Filter index) based on number of elements in the current window.
		 */
		unsigned long act_cnt = stored_item_cnt(mgr->idx);
		double coeff = BF_TOL_COEFF(act_cnt);

		double est_low = BF_LOWER_TOLERANCE(act_cnt, coeff);
		double est_high = BF_UPPER_TOLERANCE(act_cnt, coeff);

		/*
		 * Compare the current configuration of the index estimate and the
		 * expected (low and high) estimates.
		 */
		if (est_high > mgr->cfg_bloom.est_items) {
			// Higher act_cnt = make bigger bloom filter
			mgr->cfg_bloom.est_items = act_cnt * coeff;
			reinit = true;
		} else if (est_low < mgr->cfg_bloom.est_items &&
				mgr->cfg_mgr.status == BFI_MGR_S_WINDOW_FULL) {
			// Lower act_cnt = save space, make smaller bloom filter
			// Note: allow size reduction only based on the previous full window
			mgr->cfg_bloom.est_items = act_cnt * coeff;
			reinit = true;
		}
	}

	// Prepare
	if (reinit) {
		// Destroy & create a new index (parameters changed)
		status = bfi_mgr_index_prepare(mgr);
	} else {
		// Only clear the current index (parameters are the same)
		status = bfi_mgr_index_clear(mgr);
	}

	// Change current status
	if (status != 0) {
		// Something went wrong
		mgr->cfg_mgr.status = BFI_MGR_S_ERROR;
		return 1;
	}

	// Because the library requires a copy of the filename, make the copy
	char *filename_cpy = strdup(filename);
	if (!filename_cpy) {
		// Memory allocation failed
		mgr->cfg_mgr.status = BFI_MGR_S_ERROR;
		return 1;
	}
	set_index_filename(mgr->idx, filename_cpy);

	// Change status of the manager
	switch (mgr->cfg_mgr.status) {
	case BFI_MGR_S_INIT:
		mgr->cfg_mgr.status = (mgr->cfg_mgr.en_autosize)
			? BFI_MGR_S_WINDOW_PARTIAL
			: BFI_MGR_S_WINDOW_FULL;
		break;

	case BFI_MGR_S_WINDOW_PARTIAL:
		mgr->cfg_mgr.status = BFI_MGR_S_WINDOW_FULL;
		break;

	case BFI_MGR_S_WINDOW_FULL:
		// Do not change the state
		break;

	case BFI_MGR_S_ERROR:
		// Recovery from an error can occur only with start of a new window
		mgr->cfg_mgr.status = BFI_MGR_S_WINDOW_FULL;
		break;
	}

	return 0;
}

void
bfi_mgr_window_close(bfi_mgr_t *mgr)
{
	bfi_mgr_save(mgr);
	mgr->cfg_mgr.status = BFI_MGR_S_ERROR;
}

int
bfi_mgr_add(bfi_mgr_t *mgr, const uint8_t *buffer, const size_t len)
{
	if (mgr->cfg_mgr.status != BFI_MGR_S_WINDOW_FULL &&
			mgr->cfg_mgr.status != BFI_MGR_S_WINDOW_PARTIAL) {
		return 1;
	}

	add_addr_index(mgr->idx, buffer, len);
	return 0;
}

