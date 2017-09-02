/**
 * \file profiler_events.h
 * \author Lukas Hutak <xhutak01@stud.fit.vutbr.cz>
 * \brief Profiler events (header file)
 */
/* Copyright (C) 2016-2017 CESNET, z.s.p.o.
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
 * This software is provided ``as is``, and any express or implied
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

#ifndef PROFILE_EVENTS_H
#define PROFILE_EVENTS_H

#include <stdint.h>
#include "api.h"

/**
 * \brief Identification flags of changes
 */
enum PEVENTS_CHANGE {
	PEVENTS_CHANGE_TYPE = (1U << 0), /**< Type of the profile has been changed*/
	PEVENTS_CHANGE_DIR  = (1U << 1)  /**< Storage directory has been changed  */
};

/**
 * \brief Internal data type
 */
typedef struct pevents_s pevents_t;

/**
 * \brief Channel/profile context
 *
 * This context is used in function channel/profile callbacks (new/deleted/etc.)
 * to identify a channel/profile and hold user defined data.
 */
struct pevents_ctx {
	union {
		/** Pointer to the channel (only for channel callbacks!)              */
		void *channel;
		/** Pointer to the profile (only for profile callbacks!)              */
		void *profile;
	} ptr; /**< Pointer to the channel/profile (based on type of event, i.e.
		     *  profile/channel event, appropriate pointer is filled)         */

	struct {
		void *local;  /**< User defined data for THIS channel/profile         */
		void *global; /**< User defined data shared among channels/profiles   */
	} user; /**< User data defined by the user                                */
};

/**
 * \brief After a channel/profile has been created (callback prototype)
 *
 * In this callback, a user should create and define local data for this
 * channel/profile. For example, to create an output file for the profile.
 *
 * \param[in,out] ctx Channel/profile context
 * \note User defined data (user_data.local) for this (newly create) profile
 *   doesn't exists at the moment of call. Thus, the pointer is always NULL.
 * \return A function should return a pointer to the user defined local data
 *   for this profile or NULL. The pointer will be always filled into the
 *   profile context structure (user_data.local) during subsequent callback
 *   on the same profile.
 */
typedef void *(*pevents_create_cb)(struct pevents_ctx *ctx);

/**
 * \brief Before a channel/profile will be deleted (callback prototype)
 *
 * In this callback, a user should delete previously defined data for this
 * channel/profile. For example, to close an output file for the profile.
 *
 * \param[in,out] ctx Channel/profile context
 */
typedef void (*pevents_delete_cb)(struct pevents_ctx *ctx);

/**
 * \brief After reconfiguration (callback prototype)
 *
 * In this callback, a user should check parameters of the channel/profile and
 * adapt user defined structures. For example, to change directory of an output
 * file.
 *
 * \param[in,out] ctx   Channel/profile context
 * \param[in]     flags Identification of changes. See #PMGR_CHANGE.
 *                      (Multiple flags can be set at the same time)
 */
typedef void (*pevents_update_cb)(struct pevents_ctx *ctx, uint16_t flags);

/**
 * \brief Process a record (callback prototype)
 *
 * In this callback, a user should process a record that belongs to the
 * channel/profile (based on profiler classification). For example, to store
 * the record to an output file.
 *
 * \param[in,out] ctx    Channel/profile context
 * \param[in]     record Record that belongs to the channel/profile
 */
typedef void (*pevents_data_cb)(struct pevents_ctx *ctx, void *record);

/**
 * \brief General function (function prototype)
 *
 * This is not callback! A user can utilize this function prototype
 * (combination with pevents_for_each() function) to call the same function
 * individually on all profiles in a manager.
 */
typedef void (*pevents_fn)(struct pevents_ctx *ctx);


/**
 * \brief Set of events that can happen to a channel/profile
 *
 * Any of the callbacks can be set to NULL.
 */
struct pevent_cb_set {
	pevents_create_cb on_create; /**< A channel/profile has been created      */
	pevents_delete_cb on_delete; /**< A channel/profile will be deleted       */
	pevents_update_cb on_update; /**< A channel/profile has been updated      */
	pevents_data_cb   on_data; /**< A channel/profile has new data to process */
};

/**
 * \brief Create a new event manager
 * \param[in] profiles Callback functions for profiles
 * \param[in] channels Callback functions for channels
 * \return On success returns a pointer to the manager. Otherwise returns NULL.
 */
API pevents_t *
pevents_create(struct pevent_cb_set profiles, struct pevent_cb_set channels);

/**
 * \brief Delete an event manager
 *
 * Before the manager is freed, the delete callbacks for all profiles and
 * channels are called.
 * \param[in,out] mgr Pointer to the event manager
 */
API void
pevents_destroy(pevents_t *mgr);

/**
 * \brief Set a global variable that will be shared among all channels and
 *   profiles in the manager.
 *
 * \note By default, the global variable is set to NULL.
 * \param[in] mgr    Event manger
 * \param[in] global Global variable
 */
API void
pevents_global_set(pevents_t *mgr, void *global);

/**
 * \brief Get a pointer to global variable
 * \param[in] mgr Event manager
 * \return If the global variables hasn't been set before returns NULL.
 *   Otherwise returns the pointer to the global variable.
 */
API void *
pevents_global_get(pevents_t *mgr);

/**
 * \brief Process a data record
 *
 * The main goal of this function is to call data callbacks for profiles and
 * channels (i.e. ::pevents_data_cb) when data belongs to the corresponding
 * profiles and channels.
 *
 * When this event manager recognizes that profiling configuration has been
 * changed, it'll try to map old profiles/channels to new profiles/channels.
 *  - When mapping of a profile/channel is successful and no parameters has been
 *    changed no special callback is called.
 *  - When the mapping is successful but parameters of the profile/channel has
 *    been changed ::pevents_update_cb callback is called.
 *  - When mapping is not successful and a profile/channel doesn't exist anymore
 *    ::pevents_delete_cb callback is called.
 *  - When mapping is not successful and a profile/channel is new,
 *    ::pevents_create_cb callback is called.
 *
 * \note
 *   The mapping of channels/profiles guarantee following order of callbacks:
 *   First, delete callbacks are called, followed by update callbacks and
 *   finally create callbacks are called. The reason of this order is to
 *   prevent collision of shared resources (files, etc.).
 *
 * \note
 *   If a channel/profile is mentioned multiple times in the \p channels
 *   variable. The "data" channel/profile callback is called only once.
 *
 * \note
 *   The manager can map old profiles/channels to new ones only when the names
 *   of profiles/names are still same.
 *
 * \param[in,out] mgr         Event manager
 * \param[in]     channels    Pointer to null-terminated array of channels
 *   where the \p data belongs.
 * \param[in]     data        Pointer to data record
 * \return On success returns 0. Otherwise returns a non-zero value and the
 *   data record was not stored.
 */
API int
pevents_process(pevents_t *mgr, const void **channels, void *data);

/**
 * \brief Call a function on all profiles/channels in an event manager
 *
 * For example, this function can be utilize to close and open new storage
 * files (on time window change).
 *
 * \param[in,out] mgr         Profile manager
 * \param[in]     prfl_fn     General function for profiles (can be NULL)
 * \param[in]     chnl_fn     General function for channels (can be NULL)
 */
API void
pevents_for_each(pevents_t *mgr, pevents_fn prfl_fn, pevents_fn chnl_fn);

#endif // PROFILE_EVENTS_H
