/**
 * \file storage_profiles.h
 * \author Lukas Hutak <xhutak01@stud.fit.vutbr.cz>
 * \brief Profile storage management (header file)
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

#ifndef LS_STORAGE_PROFILES_H
#define LS_STORAGE_PROFILES_H

#include <libnf.h>
#include "configuration.h"

/**
 * \brief Internal type
 */
typedef struct stg_profiles_s stg_profiles_t;

/**
 * \brief Create a profile storage
 * \param[in] params Parameters of this plugin instance
 * \return On success returns a pointer to the storage. Otherwise returns NULL.
 */
stg_profiles_t *
stg_profiles_create(const struct conf_params *params);

/**
 * \brief Delete a profile storage
 *
 * Close output file(s) and delete the storage
 * \param[in,out] storage Storage
 */
void
stg_profiles_destroy(stg_profiles_t *storage);

/**
 * \brief Store a LNF record to a storage
 * \param[in,out] storage Storage
 * \param[in]     rec     LNF record
 * \return On success (all channels have been found) returns 0. Otherwise
 *   (failed to find a channel and rebuild a new configuration) returns
 *   a non-zero value. The return value do not signalize status of output files.
 */
int
stg_profiles_store(stg_profiles_t *storage, const struct metadata *mdata,
	lnf_rec_t *rec);

/**
 * \brief Create a new time window
 *
 * Current output file(s) will be closed and new ones will be opened.
 * \param[in,out] storage Storage
 * \param[in]     window  Identification time of new window (UTC)
 * \return On success returns 0. Otherwise (at least one window is not
 *   properly initialized) returns a non-zero value.
 */
int
stg_profiles_new_window(stg_profiles_t *storage, time_t window);


#endif //LS_STORAGE_PROFILES_H
