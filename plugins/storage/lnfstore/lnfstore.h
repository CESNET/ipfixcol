/**
 * \file lnfstore.h
 * \author Imrich Stoffa <xstoff02@stud.fit.vutbr.cz>
 * \author Lukas Hutak <xhutak01@stud.fit.vutbr.cz>
 * \brief lnfstore plugin interface (header file)
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

#ifndef LS_LNFSTORE_H 
#define LS_LNFSTORE_H

#include <libxml/xmlstring.h>
#include <stdbool.h>
#include <stdint.h>
#include <libnf.h>

#include "bitset.h"

/**
 * \brief Structure for configuration parsed from XML
 */
struct conf_params {
	char *storage_path;              /**< Storage directory (template)    */
	char *file_prefix;               /**< File prefix                     */
	char *file_suffix;               /**< File suffix (template)          */
	char *file_ident;                /**< Internal file identification    */

	uint32_t window_time;            /**< Time windows size               */
	bool window_align;               /**< Enable/disable window alignment */
	bool compress;                   /**< Enable/disable LZO compression  */

	bool profiles;                   /**< Use profile metadata            */
};

// Size of conversion buffer
#define BUFF_SIZE (65535)

/** \brief Profile identification */
typedef struct profile_file_s {
	void *address;
	lnf_file_t *file;
} profile_file_t;

/**
 * \brief Configuration of the plugin instantion
 */
struct lnfstore_conf
{
	struct conf_params *params;      /**< Configuration from XML file     */

	uint8_t buffer[BUFF_SIZE];       /**< Buffer for record conversion    */
	lnf_rec_t *rec_ptr;              /**< Converted record */
	time_t window_start;             /**< Start of current window         */

	lnf_file_t *file_ptr;            /**< Storage (for no profiler mode)  */

	profile_file_t *profiles_ptr;    /**< Storages (for profile mode)     */
	int profiles_size;               /**< Size of the array               */
	bitset_t *bitset;                /**< Aux bitset                      */
};

#endif //LS_LNFSTORE_H
