/**
 * \file storage_profiles.c
 * \author Lukas Hutak <xhutak01@stud.fit.vutbr.cz>
 * \brief Profile storage management (source file)
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

#include <ipfixcol.h>
#include <string.h>
#include <ipfixcol/profiles.h>
#include <limits.h>
#include <stdint.h>

#include "storage_profiles.h"
#include "lnfstore.h"
#include "files_manager.h"
#include "profiler_events.h"
#include "configuration.h"
#include "storage_common.h"

/**
 * \brief Global data shared among all channels (read-only)
 */
struct stg_profiles_global {
	/** Pointer to the plugin parameters */
	const struct conf_params *params;
	/** Start of current time window (required for runtime reconfiguration of
	 *  channels i.e. creating/deleting) */
	time_t window_start;

	/** Operation status (returns status of selected callbacks) */
	int op_status;
};

/**
 * \brief Local data for each channel
 */
struct stg_profiles_chnl_local {
	/** Manager of output file(s)
	 *  This manager must NOT exist when parameters of it's channel are not
	 *  correct, for example, storage path is outside of the main storage dir.,
	 *  etc.
	 */
	files_mgr_t *manager;
};

/**
 * \brief Internal structure of the manager
 */
struct stg_profiles_s {
	/** Profile event manager */
	pevents_t *event_mgr;

	/** Pointer to the global parameters shared among all channels */
	struct stg_profiles_global global;
};

/**
 * \brief Generate an output directory filename of a channel
 *
 * Based on an output directory specified by the channel's parent profile and
 * channel name generates directory filename.
 * Format: 'profile_dir'/'chanel_name'̈́/
 *
 * \warning Return value MUST be freed by user using free().
 * \param[in] channel Pointer to the channel
 * \return On success returns a pointer to the filename (string). Otherwise
 *   returns a non-zero value.
 */
static char *
channel_get_dirname(void *channel)
{
	const char *channel_subdir = "channels";
	const char *channel_name;
	const char *profile_dir;
	void *profile_ptr;

	channel_name = channel_get_name(channel);
	profile_ptr  = channel_get_profile(channel);
	profile_dir  = profile_get_directory(profile_ptr);

	const size_t len_extra = strlen(channel_subdir) + 4U; // 4 = 3x'/' + 1x'\0'
	size_t dir_len = strlen(profile_dir) + strlen(channel_name) + len_extra;
	if (dir_len >= PATH_MAX) {
		MSG_ERROR(msg_module, "Failed to create directory path (Directory name "
			"is too long)");
		return NULL;
	}

	char *out_dir = (char *) malloc(dir_len * sizeof(char));
	if (!out_dir) {
		MSG_ERROR(msg_module, "Unable to allocate memory (%s:%d)",
			__FILE__, __LINE__);
		return NULL;
	}

	int ret = snprintf(out_dir, dir_len, "%s/%s/%s/", profile_dir,
		channel_subdir, channel_name);
	if (ret < 0 || ((size_t) ret) >= dir_len) {
		MSG_ERROR(msg_module, "snprintf() error");
		free(out_dir);
		return NULL;
	}

	return out_dir;
}

/**
 * \brief Close a channel's storage
 *
 * Destroy a files manager.
 * \param[in,out] local Local data of the channel
 */
static void
channel_storage_close(struct stg_profiles_chnl_local *local)
{
	// Close & delete a files manager
	if (local->manager) {
		files_mgr_destroy(local->manager);
		local->manager = NULL;
	}
}

/**
 * \brief Open a channel's storage
 *
 * First, if the previous instance of the storage exists, it'll be closed and
 * deleted. Second, the function generates a name of an output directory
 * and creates a new files manager for output files.
 *
 * \note Output files will not be ready yet, you have to create a time window
 *   first i.e. channel_storage_new_window().
 * \param[in,out] local       Local data of the channel
 * \param[in]     global      Global structure shared among all channels
 * \param[in]     channel_ptr Pointer to a channel (from a profiler)
 * \return On success returns 0. Otherwise returns a non-zero value and the
 *   files manager is not initialized (i.e. no records can be created).
 */
static int
channel_storage_open(struct stg_profiles_chnl_local *local,
	const struct stg_profiles_global *global, void *channel_ptr)
{
	// Make sure that the previous instance is deleted
	channel_storage_close(local);

	// Get a new directory name
	char *dir = channel_get_dirname(channel_ptr);
	if (!dir) {
		// Failed to create the directory name
		return 1;
	}

	// Create a new files manager
	files_mgr_t *new_mgr;
	new_mgr = stg_common_files_mgr_create(global->params, dir);
	free(dir);
	if (!new_mgr) {
		// Failed to create a files manager
		return 1;
	}

	local->manager = new_mgr;
	return 0;
}

/**
 * \brief Check if a directory is inside a parent directory (based on names)
 * \param[in] path_dir    The directory path
 * \param[in] path_parent The parent path
 * \note If the \p path_parent is NULL or empty, the function will return 0.
 * \return If the directory is in the parent directory returns 0. Otherwise
 *   returns a non-zero value.
 */
static int
channel_storage_check_subdir(const char *path_dir, const char *path_parent)
{
	if (!path_parent) {
		// If the path_parent is not defined, do not check it
		return 0;
	}

	// Make copies of both strings
	const size_t len_parent = strlen(path_parent);
	const size_t len_dir = strlen(path_dir);
	if (len_parent == 0) {
		// Empty string
		return 0;
	}

	char *cpy_parent = calloc(len_parent + 2, sizeof(char));
	char *cpy_dir = calloc(len_dir + 2, sizeof(char)); // 2 = '/' + '\0'
	if (!cpy_dir || !cpy_parent) {
		MSG_ERROR(msg_module, "Unable to allocate memory (%s:%d)",
			__FILE__, __LINE__);
		free(cpy_parent);
		free(cpy_dir);
		return 1;
	}

	strncpy(cpy_parent, path_parent, len_parent);
	strncpy(cpy_dir, path_dir, len_dir);

	// Add slashes and sanitize the strings
	cpy_parent[len_parent] = '/';
	cpy_dir[len_dir] = '/';
	files_mgr_names_sanitize(cpy_parent);
	files_mgr_names_sanitize(cpy_dir);

	// Compare
	int result = strncmp(cpy_parent, cpy_dir, strlen(cpy_parent));
	free(cpy_parent);
	free(cpy_dir);

	return result;
}

/**
 * \brief Create a new time window
 *
 * Close & reopen output files
 * \param[in,out] local       Local data of the channel
 * \param[in]     global      Global structure shared among all channels
 * \param[in]     channel_ptr Pointer to a channel (from a profiler)
 * \return On success returns 0. Otherwise returns a non-zero value.
 */
static int
channel_storage_new_window(struct stg_profiles_chnl_local *local,
	const struct stg_profiles_global *global, void *channel_ptr)
{
	(void) channel_ptr;
	if (!local->manager) {
		// Uninitialized manager
		return 1;
	}

	// Check if the main storage directory exists (if it is defined)
	const char *main_storage = global->params->files.path; // can be NULL
	if (main_storage && stg_common_dir_exists(main_storage) != 0) {
		// The storage directory doesn't exists -> don't try to create files
		return 1;
	}

	// Create a time window
	if (files_mgr_new_window(local->manager, &global->window_start)) {
		// Totally or partially failed...
		return 1;
	};

	return 0;
}

/**
 * \brief All-in-one initialization of a channel's storage
 *
 * First, check if storage parameters are correct. Second, create a new file
 * manager and, finally, create a new time window files. All changes are stored
 * into the local data of the channel. Therefore, channel will end up,
 * respectively, without file manager (i.e. NULL), with file manager without
 * output files or with the fully working manager. If the file manager is NULL,
 * it can be reinitialized only by this function. If the file manager is
 * defined but output files are not (partially) ready, it can be fixed using
 * channel_storage_new_window() function, but this usually automatically when
 * new time windows are created.
 *
 * If the channel is not fully ready (something went wrong), all other
 * functions that manipulate channel can be used, but they usually do not have
 * any effect.
 *
 * \note This function can be also used to reinitialize the channel's file
 *   manager when profile's/channel's storage directory has been changed.
 *
 * \param[in,out] local       Local data of the channel
 * \param[in]     global      Global structure shared among all channels
 * \param[in]     channel_ptr Pointer to a channel (from a profiler)
 * \return On success returns 0. Otherwise (something went wrong) returns
 *   a non-zero value.
 */
static int
channel_storage_init(struct stg_profiles_chnl_local *local,
	const struct stg_profiles_global *global, void *channel_ptr)
{
	const char *channel_path = channel_get_path(channel_ptr);
	const char *channel_name = channel_get_name(channel_ptr);

	void *profile_ptr  = channel_get_profile(channel_ptr);
	const char *profile_dir = profile_get_directory(profile_ptr);
	const char *main_dir = global->params->files.path; // Can be NULL

	// Check if the storage parameters are correct
	if (main_dir && channel_storage_check_subdir(profile_dir, main_dir)) {
		// Failed -> do not create a file manager
		MSG_ERROR(msg_module, "Failed to create a storage of channel '%s%s'. "
			"Main storage directory (%s) is specified, but the storage "
			"directory of this channel's profile (%s) is outside of the main "
			"directory. Further records of this channel will NOT be stored. "
			"Change storage directory of the profile or omit storage "
			"directory in the plugin's configuration",
			channel_path, channel_name, main_dir, profile_dir);
		return 1;
	}

	// Create a storage manager and save it to the local data
	if (channel_storage_open(local, global, channel_ptr)) {
		// Failed
		MSG_WARNING(msg_module, "Failed to create storage of channel '%s%s'. "
			"Further records of this channel will NOT be stored.",
			channel_path, channel_name);
		return 1;
	}

	// Create a new time window
	if (channel_storage_new_window(local, global, channel_ptr)) {
		// Failed
		MSG_WARNING(msg_module, "Failed to create a new time window of channel "
			"'%s%s'. Output file(s) of this channel are not prepared and "
			"further records will NOT be stored.",
			channel_path, channel_name);
		return 1;
	}

	// Success
	return 0;
}

/**
 * \brief Create a new channel
 *
 * The main purpose of this function is to create files managers (output files)
 * for this channel.
 *
 * \param[in,out] ctx Information context about the channel
 * \return On success returns a pointer to local data. Otherwise returns NULL.
 */
static void *
channel_create_cb(struct pevents_ctx *ctx)
{
	const char *channel_path = channel_get_path(ctx->ptr.channel);
	const char *channel_name = channel_get_name(ctx->ptr.channel);
	MSG_DEBUG(msg_module, "Processing new channel '%s%s'...", channel_path,
		channel_name);

	// Create an internal structure
	struct stg_profiles_chnl_local *local_data;
	local_data = calloc(1, sizeof(*local_data));
	if (!local_data) {
		MSG_ERROR(msg_module, "Unable to allocate memory (%s:%d)",
			__FILE__, __LINE__);
		MSG_ERROR(msg_module, "Failed to create storage of channel '%s%s' "
			"(memory allocation error). Unrecoverable error occurred, please, "
			"delete and create the channel or restart this plugin.",
			channel_path, channel_name);
		return NULL;
	}

	void *profile = channel_get_profile(ctx->ptr.channel);
	const enum PROFILE_TYPE type = profile_get_type(profile);
	if (type != PT_NORMAL) {
		// Do not create a file manager for this shadow channel
		return local_data;
	}

	// Initialize a storage
	if (channel_storage_init(local_data, ctx->user.global, ctx->ptr.channel)) {
		// Something went wrong and a message was printed.
		return local_data;
	}

	MSG_INFO(msg_module, "Channel '%s%s' has been successfully created.",
		channel_path, channel_name);
	return local_data;
}

/**
 * \brief Delete a channel
 *
 * The function deletes early create a file manager (aka local data)
 * \param[in,out] ctx Information context about the channel
 */
static void
channel_delete_cb(struct pevents_ctx *ctx)
{
	const char *channel_path = channel_get_path(ctx->ptr.channel);
	const char *channel_name = channel_get_name(ctx->ptr.channel);
	MSG_DEBUG(msg_module, "Deleting channel '%s%s'...", channel_path,
		channel_name);

	struct stg_profiles_chnl_local *local_data;
	local_data = (struct stg_profiles_chnl_local *) ctx->user.local;
	if (local_data != NULL) {
		channel_storage_close(local_data);
		free(local_data);
	}

	MSG_INFO(msg_module, "Channel '%s%s' has been successfully closed.",
		channel_path, channel_name);
}

/**
 * \brief Update a channel
 *
 * Based on a configuration of the channel and parent's profile, open/change/
 * close file storage.
 *
 * \param[in,out] ctx   Information context about the channel
 * \param[in]     flags Changes (see #PEVENTS_CHANGE)
 */
static void
channel_update_cb(struct pevents_ctx *ctx, uint16_t flags)
{
	void *channel_ptr = ctx->ptr.channel;
	const char *channel_path = channel_get_path(channel_ptr);
	const char *channel_name = channel_get_name(channel_ptr);
	MSG_DEBUG(msg_module, "Updating channel '%s%s'...", channel_path,
		channel_name);

	struct stg_profiles_chnl_local *local_data;
	local_data = (struct stg_profiles_chnl_local *) ctx->user.local;
	if (!local_data) {
		MSG_ERROR(msg_module, "Channel '%s%s' cannot be updated, because it's "
			"not properly initialized. Try to delete it from a profiling "
			"configuration and create it again or restart this plugin.",
			channel_path, channel_name);
		return;
	}

	void *profile = channel_get_profile(channel_ptr);
	const enum PROFILE_TYPE type = profile_get_type(profile);

	// Is the profile's type still the same?
	if (type != PT_NORMAL) {
		// The type is shadow -> Delete the files manager, if it exists.
		if (local_data->manager == NULL) {
			// Already deleted
			return;
		}

		channel_storage_close(local_data);
		MSG_INFO(msg_module, "Channel '%s%s' has been successfully updated "
			"(storage has been closed).", channel_path, channel_name);
		return;
	}

	// Only "Normal" type can be here!
	if ((flags & PEVENTS_CHANGE_DIR) || (flags & PEVENTS_CHANGE_TYPE)) {
		/* The profile's directory and/or type has been changed.
		 * Delete the old storage & create a new one
		 */
		if (channel_storage_init(local_data, ctx->user.global, channel_ptr)) {
			return;
		}

		MSG_INFO(msg_module, "Channel '%s%s' has been successfully updated "
			"(storage has been created/changed).", channel_path, channel_name);
		return;
	}
}

/**
 * \brief Process data for a channel
 *
 * Data record is stored to output file(s)
 * \param[in,out] ctx Information context about the channel
 * \param[in] data LNF record
 */
static void
channel_data_cb(struct pevents_ctx *ctx, void *data)
{
	struct stg_profiles_chnl_local *local_data;
	local_data = (struct stg_profiles_chnl_local *) ctx->user.local;
	if (!local_data || !local_data->manager) {
		// A file manager is not available
		return;
	}

	lnf_rec_t *rec_ptr = data;
	int ret = files_mgr_add_record(local_data->manager, rec_ptr);
	if (ret != 0) {
		// Failed
		void *channel = ctx->ptr.channel;
		MSG_DEBUG(msg_module, "Failed to store a record into channel '%s%s'.",
			channel_get_path(channel), channel_get_name(channel));
	}
}

/**
 * \brief Auxiliary callback function for invalidating time windows
 * \param[in,out] ctx Information context about the channel
 */
static void
channel_disable_window(struct pevents_ctx *ctx)
{
	struct stg_profiles_chnl_local *local = ctx->user.local;
	if (!local || !local->manager) {
		// Local data or the file manager is not initialized
		return;
	}

	files_mgr_invalidate(local->manager);
}

/**
 * \brief Auxiliary callback function for changing time windows
 * \param[in,out] ctx Information context about the channel
 */
static void
channel_new_window(struct pevents_ctx *ctx)
{
	struct stg_profiles_chnl_local *local = ctx->user.local;
	struct stg_profiles_global *global = ctx->user.global;
	void *channel = ctx->ptr.channel;

	if (!local || !local->manager) {
		// Local data or a file manager is not initialized
		return;
	}

	// Try to create the new window
	int ret_val = channel_storage_new_window(local, global, channel);
	if (ret_val != 0) {
		// Failed
		MSG_WARNING(msg_module, "Failed to create a new time window of "
			"channel '%s%s'. Output file(s) of this channel are not prepared "
			"and further records will NOT be stored.",
			channel_get_path(channel), channel_get_name(channel));
		global->op_status = 1;
	}
}

stg_profiles_t *
stg_profiles_create(const struct conf_params *params)
{
	// Prepare an internal structure
	stg_profiles_t *mgr = (stg_profiles_t *) calloc(1, sizeof(*mgr));
	if (!mgr) {
		MSG_ERROR(msg_module, "Unable to allocate memory (%s:%d)",
			__FILE__, __LINE__);
		return NULL;
	}

	mgr->global.params = params;

	// Initialize an array of callbacks
	struct pevent_cb_set channel_cb;
	memset(&channel_cb, 0, sizeof(channel_cb));

	struct pevent_cb_set profile_cb;
	memset(&profile_cb, 0, sizeof(profile_cb));

	channel_cb.on_create = &channel_create_cb;
	channel_cb.on_delete = &channel_delete_cb;
	channel_cb.on_update = &channel_update_cb;
	channel_cb.on_data   = &channel_data_cb;

	// Create an event manager
	mgr->event_mgr = pevents_create(profile_cb, channel_cb);
	if (!mgr->event_mgr) {
		// Failed
		free(mgr);
		return NULL;
	}

	pevents_global_set(mgr->event_mgr, &mgr->global);
	return mgr;
}

void
stg_profiles_destroy(stg_profiles_t *storage)
{
	// Destroy a profile manager and close all files (delete callback)
	pevents_destroy(storage->event_mgr);
	free(storage);
}

int
stg_profiles_store(stg_profiles_t *storage, const struct metadata *mdata,
	lnf_rec_t *rec)
{
	// Store the record to the channels
	return pevents_process(storage->event_mgr, (const void **)mdata->channels,
		rec);
}

int
stg_profiles_new_window(stg_profiles_t *storage, time_t window)
{
	storage->global.window_start = window;
	storage->global.op_status = 0;

	// If the main storage directory is specified, check if it exists
	const char *main_dir = storage->global.params->files.path;
	if (main_dir != NULL && stg_common_dir_exists(main_dir) != 0) {
		MSG_ERROR(msg_module, "Storage directory '%s' doesn't exist. "
			"All records will be lost! Try to create the directory and make "
			"sure the collector has access rights.", main_dir);
		pevents_for_each(storage->event_mgr, NULL, &channel_disable_window);
		return 1;
	}

	// Try to create new time windows
	pevents_for_each(storage->event_mgr, NULL, &channel_new_window);
	return storage->global.op_status;
}
