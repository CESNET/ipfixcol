/**
 * \file lnfstore.h
 * \author Imrich Stoffa <xstoff02@stud.fit.vutbr.cz>
 * \author Lukas Hutak <xhutak01@stud.fit.vutbr.cz>
 * \author Pavel Krobot <Pavel.Krobot@cesnet.cz>
 * \brief lnfstore plugin interface (header file)
 *
 * Copyright (C) 2015, 2016 CESNET, z.s.p.o.
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

#ifndef LS_LNFSTORE_H
#define LS_LNFSTORE_H

#include <libxml/xmlstring.h>
#include <stdbool.h>
#include <stdint.h>
#include <libnf.h>

#include "bitset.h"
#include <bf_index.h>

#define BF_FILE_PREFIX          "bf."
#define BF_DEFAULT_FP_PROB      0.01
#define BF_DEFAULT_ITEM_CNT_EST 100000
/** UPPPER_TOLERANCE should be small, since real unique item count should NOT
 ** be higher than bloom filter estimated item count. If there are more items
 ** than expected (=estimated) real false positive probability could be higher
 ** than desired f. p. probability.
 ** LOWER_TOLERANCE could be more benevolent. In this case bloom filter size is
 ** unnecessarily big. Value of LOWER_TOLERANCE depends on trade-off between
 ** wasted space and frequency of bloom filter re-creation (with new
 ** parameters).
**/
#define BF_TOL_COEFF(x) ((x > 10000000) ? 1.1 : (x > 100000) ? 1.2 : \
        (x > 30000) ? 1.5 : (x > 5000) ? 2 : \
        (x > 500) ? 3 : 10)

#define BF_UPPER_TOLERANCE(val,coeff)     ((unsigned long)(val * (1 + coeff * 0.05)))
#define BF_LOWER_TOLERANCE(val,coeff)     ((unsigned long)(val * (1 + coeff * \
                                                                  ((coeff > 1.2) ? 1.3 : 0.5) )))

typedef enum {
    BF_INIT = 0,
    BF_IN_PROGRESS_FIRST,
    BF_CLOSING_FIRST,
    BF_IN_PROGRESS,
    BF_CLOSING,
    BF_CLOSING_LAST,
    BF_ERROR
//    BF_NOT_IN_USE
} bf_indexing_states_t;

/**
 * \brief Structure for configuration parsed from XML
 */
struct conf_params {
	char *storage_path;              /**< Storage directory (template)        */
	char *file_prefix;               /**< File prefix                         */
	char *file_suffix;               /**< File suffix (template)              */
	char *file_ident;                /**< Internal file identification        */

	uint32_t window_time;            /**< Time windows size                   */
	bool window_align;               /**< Enable/disable window alignment     */
	bool compress;                   /**< Enable/disable LZO compression      */

	bool profiles;                   /**< Use profile metadata                */

	struct index_params bf;         /**< Bloom filter indexing parameters    */
	bool bf_index_autosize;          /**< Adaptive adjusting of B.F. index item
	                                      count                               */
};

// Size of conversion buffer
#define BUFF_SIZE (65535)

struct lnfstore_index{
    index_t *index;                 /**< Bloom filter index for IP addresses) */
    unsigned long unique_item_cnt;   /**< Stores unique item count of last time
                                         window                               */
    bool params_changed;            /**< Flag which indicates if bloom filter
                                         parameters was changed (unique item
                                         count in last window has changed
                                         significantly )                      */
    bf_indexing_states_t state;     /**< ... */
};

/** \brief Profile identification */
typedef struct profile_file_s {
	void *address;
	lnf_file_t *file;
	struct lnfstore_index *lnf_index;
} profile_file_t;

/**
 * \brief Configuration of the plugin instantion
 */
struct lnfstore_conf
{
	struct conf_params *params;      /**< Configuration from XML file         */

	uint8_t buffer[BUFF_SIZE];       /**< Buffer for record conversion        */
	lnf_rec_t *rec_ptr;              /**< Converted record                    */
	time_t window_start;             /**< Start of current window             */

    /// No profiler mode
	lnf_file_t *file_ptr;            /**< Storage                             */
	struct lnfstore_index *lnf_index;

	/// Profile mode
	profile_file_t *profiles_ptr;    /**< Storages                            */
	int profiles_size;               /**< Size of the array                   */
	bitset_t *bitset;                /**< Aux bitset                          */
};


struct lnfstore_index *create_lnfstore_index(struct index_params params);
void destroy_lnfstore_index(struct lnfstore_index *lnf_index);

#endif //LS_LNFSTORE_H
