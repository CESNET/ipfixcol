/**
 * \file profilestats.cpp
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Intermediate plugin for RRD statistics (source file)
 * \note Inspired by the previous implementation by Michal Kozubik
 */
/*
 * Copyright (C) 2015-2017 CESNET, z.s.p.o.
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

#include <exception>
#include <algorithm>
#include <vector>
#include <memory>
#include <cstring>
#include "configuration.h"
#include "profilestats.h"
#include "RRD.h"

extern "C" {
#include <ipfixcol.h>
#include <ipfixcol/profiles.h>
#include <ipfixcol/profile_events.h>

// API version constant
IPFIXCOL_API_VERSION
}

#define PROFILE_PATH_FIX(var) \
	if ((var) != nullptr && (var)[0] == '\0') { \
		(var) = "live"; \
    }


// Identifier for verbose macros
static const char *msg_module = "profilestats";

/** IPFIX Information Element of bytes       */
constexpr uint16_t IPFIX_IE_BYTES   = 1;
/** IPFIX Information Element of packets     */
constexpr uint16_t IPFIX_IE_PACKETS = 2;
/** IPFIX Information Element of protocol    */
constexpr uint16_t IPFIX_IE_PROTO   = 4;

/**
 * \brief Plugin instance
 */
struct plugin_data {
	/** Internal process configuration   */
	void *ip_config;

	/** Parsed parameters                */
	plugin_config *cfg;
	/** Event manager of profiles        */
	pevents_t *events;
	/** Start of the current interval    */
	time_t interval_start;

    // Constructor
    plugin_data() {
        ip_config = nullptr;
        cfg = nullptr;
        events = nullptr;
        interval_start = 0;
    }

    // Destructor
	~plugin_data() {
		if (cfg != nullptr) {
			delete(cfg);
		}
		if (events != nullptr) {
			pevents_destroy(events);
		}
	}

	// Disable copy constructors
	plugin_data(const plugin_data &) = delete;
	plugin_data &operator=(const plugin_data &) = delete;
};

/**
 * \brief Get a value of an unsigned integer (stored in big endian order a.k.a.
 *   network byte order)
 *
 * The \p value is read from a data \p field and converted from
 * the appropriate byte order to host byte order.
 * \param[in]  field  Pointer to the data field (in "network byte order")
 * \param[in]  size   Size of the data field (min: 1 byte, max: 8 bytes)
 * \param[out] value  Pointer to a variable for the result
 * \return On success returns 0 and fills the \p value.
 *   Otherwise (usually the incorrect \p size of the field) returns
 *   a non-zero value and the \p value is not filled.
 */
static inline int
flow_stat_convert_field(const void *field, size_t size, uint64_t *value)
{
	switch (size) {
	case 8:
		*value = be64toh(*(const uint64_t *) field);
		return 0;

	case 4:
		*value = ntohl(*(const uint32_t *) field);
		return 0;

	case 2:
		*value = ntohs(*(const uint16_t *) field);
		return 0;

	case 1:
		*value = *(const uint8_t *) field;
		return 0;

	default:
		// Other sizes (3,5,6,7)
		break;
	}

	if (size == 0 || size > 8) {
		return 1;
	}

	uint64_t new_value = 0;
	memcpy(&(((uint8_t *) &new_value)[8U - size]), field, size);

	*value = be64toh(new_value);
	return 0;
}

/**
 * \brief Find an IANA IPFIX field in a record and convert it to uint64_t
 * \param[in]  rec    IPFIX record
 * \param[in]  id     Identification of IPFIX Information Element
 * \param[out] result Converted value
 * \return On success returns 0 and \p result is filled.
 *   Otherwise (usually the field is not present) returns a non-zero value.
 */
int
flow_stat_get_value(struct ipfix_record *rec, uint16_t id, uint64_t &result)
{
	uint8_t *rec_data = static_cast<uint8_t *>(rec->record);
	int field_size;
	uint8_t *field_data;

	// Locate the field in the record
	field_data = data_record_get_field(rec_data, rec->templ, 0, id,
		&field_size);
	if (!field_data) {
		return 1;
	}

	if (flow_stat_convert_field(field_data, size_t(field_size), &result) != 0) {
		return 1;
	}

	return 0;
}

/**
 * \brief Gather flow fields for update of RRD statistics
 * \param[in]  rec   IPFIX record
 * \param[out] stats Flow fields
 * \return On success returns 0. Otherwise (at least one field not found),
 *   returns a non-zero value.
 */
int
flow_stat_prepare(struct ipfix_record *rec, struct flow_stat &stats)
{
	if (flow_stat_get_value(rec, IPFIX_IE_PROTO, stats.proto)) {
		return 1;
	}

	if (flow_stat_get_value(rec, IPFIX_IE_BYTES, stats.bytes)) {
		return 1;
	}

	if (flow_stat_get_value(rec, IPFIX_IE_PACKETS, stats.packets)) {
		return 1;
	}

	return 0;
}

/**
 * \brief Create a new channel
 *
 * Generate a new file name of the channel based on profiling configuration
 * and try to create appropriate RRD database.
 * \param[in] ctx Event context (local and global data)
 * \return Pointer to newly created RRD wrapper instance
 */
static void *
channel_create_cb(struct pevents_ctx *ctx)
{
	plugin_data *instance = static_cast<plugin_data *>(ctx->user.global);
	void *channel_ptr = ctx->ptr.channel;
	void *profile_ptr = channel_get_profile(channel_ptr);
	const char *channel_path = channel_get_path(channel_ptr);
	const char *channel_name = channel_get_name(channel_ptr);
	MSG_DEBUG(msg_module, "Creating channel '%s%s'...", channel_path,
		channel_name);

	RRD_wrapper *rrd = nullptr;

	try {
		std::string file;
		file += profile_get_directory(profile_ptr);
		file += "/rrd/channels/";
		file += channel_name;
		file += ".rrd";

		rrd = new RRD_wrapper(instance->cfg->base_dir, file, instance->cfg->interval);
		// Note: If create operation fails, we will still have a wrapper
		rrd->file_create(instance->interval_start, false);

	} catch (std::exception &ex) {
		MSG_WARNING(msg_module, "Failed to create channel '%s%s': %s",
			channel_path, channel_name, ex.what());
		return rrd;
	} catch (...) {
		MSG_WARNING(msg_module, "Failed to create channel '%s%s': %s",
			channel_path, channel_name, "Unknown error has occurred");
		return rrd;
	}

	MSG_INFO(msg_module, "Channel '%s%s' has been successfully created.",
		channel_path, channel_name);
	return rrd;
}

/**
 * \brief Destroy a channel
 *
 * First, flush remaining statistics to the RRD file and then delete RRD
 * wrapper instance. The RRD file will be preserved.
 * \param[in,out] ctx Event context (local and global data)
 */
static void
channel_delete_cb(struct pevents_ctx *ctx)
{
	plugin_data *instance = static_cast<plugin_data *>(ctx->user.global);
	RRD_wrapper *rrd = static_cast<RRD_wrapper *>(ctx->user.local);
	void *channel_ptr = ctx->ptr.channel;
	const char *channel_path = channel_get_path(channel_ptr);
	const char *channel_name = channel_get_name(channel_ptr);
	MSG_DEBUG(msg_module, "Deleting channel '%s%s'...", channel_path,
		channel_name);

	if (!rrd) {
		// Nothing to delete
		return;
	}

	try {
		// Flush up to now statistics and delete the channel
		std::unique_ptr<RRD_wrapper> wrapper(rrd);
		wrapper->file_update(instance->interval_start);
	} catch (std::exception &ex) {
		MSG_WARNING(msg_module, "Failed to properly delete channel '%s%s': %s",
			channel_path, channel_name, ex.what());
		return;
	} catch (...) {
		MSG_WARNING(msg_module, "Failed to properly delete channel '%s%s': %s",
			channel_path, channel_name, "Unknown error has occurred");
		return;
	}

	MSG_INFO(msg_module, "Channel '%s%s' has been successfully closed.",
		channel_path, channel_name);
}

/**
 * \brief Update a channel configuration
 *
 * Update is sensitive only to change of storage directory. Other changes are
 * ignored. If the storage directory is changed, old RRD wrapped is destroyed
 * and replaces with the new one.
 * \param[in,out] ctx Event context (local and global data)
 * \param[in] flags Detected changes in configuration
 */
static void
channel_update_cb(struct pevents_ctx *ctx, uint16_t flags)
{
	if (!(flags & PEVENTS_CHANGE_DIR)) {
		// Directory is still the same i.e. no changes
		return;
	}

	void *channel_ptr = ctx->ptr.channel;
	const char *channel_path = channel_get_path(channel_ptr);
	const char *channel_name = channel_get_name(channel_ptr);
	MSG_DEBUG(msg_module, "Updating channel '%s%s'...", channel_path,
		channel_name);


	// Delete the previous instance of the wrapper
	channel_delete_cb(ctx);

	// Create new one
	RRD_wrapper *new_wrapper;
	new_wrapper = static_cast<RRD_wrapper *>(channel_create_cb(ctx));
	ctx->user.local = new_wrapper;
	if (!new_wrapper) {
		MSG_WARNING(msg_module, "Update process of channel '%s%s' failed.",
			channel_path, channel_name);
		return;
	}

	MSG_INFO(msg_module, "Channel '%s%s' has been successfully updated.",
		channel_path, channel_name);
}

/**
 * \brief Add a flow
 *
 * Statistics will be stored into an RRD wrapper that aggregates them and later
 * will be flushed to the appropriate RRD file when channel_flush_cb() is
 * called. In other words, by calling this function the database is not
 * immediately updated, only statistics are cached.
 * \param[in,out] ctx  Event context (local and global data)
 * \param[in]     data Pointer to parsed flow features
 */
static void
channel_data_cb(struct pevents_ctx *ctx, void *data)
{
	struct flow_stat *stat = static_cast<struct flow_stat *>(data);
	RRD_wrapper *rrd = static_cast<RRD_wrapper *>(ctx->user.local);
	if (!rrd) {
		return;
	}

	rrd->flow_add(*stat);
}

/**
 * \brief Flush aggregated statistics to an RRD
 *
 * All statistics aggregated since last flush will be stored to the database
 * and the local statistics will be reset to zeros.
 * \param[in,out] ctx Event context (local and global data)
 */
static void
channel_flush_cb(struct pevents_ctx *ctx)
{
	plugin_data *instance = static_cast<plugin_data *>(ctx->user.global);
	RRD_wrapper *rrd = static_cast<RRD_wrapper *>(ctx->user.local);
	if (!rrd) {
		return;
	}

	void *channel_ptr = ctx->ptr.channel;
	const char *channel_path = channel_get_path(channel_ptr);
	const char *channel_name = channel_get_name(channel_ptr);
	MSG_DEBUG(msg_module, "Updating RRD of channel '%s%s'...", channel_path,
		channel_name);

	try {
		rrd->file_update(instance->interval_start);
	} catch (std::exception &ex) {
		MSG_WARNING(msg_module, "Failed to update RRD of channel '%s%s': %s",
			channel_path, channel_name, ex.what());
		return;
	} catch (...) {
		MSG_WARNING(msg_module, "Failed to update RRD of channel '%s%s': %s",
			channel_path, channel_name, "Unknown error has occurred");
		return;
	}

	MSG_INFO(msg_module, "RRD of channel '%s%s' has been successfully updated.",
		channel_path, channel_name);
}

/**
 * \brief Create a new profile
 *
 * Generate a new file name of the profile based on profiling configuration
 * and try to create appropriate RRD database.
 * \param[in] ctx Event context (local and global data)
 * \return Pointer to newly created RRD wrapper instance
 */
static void *
profile_create_cb(struct pevents_ctx *ctx)
{
	plugin_data *instance = static_cast<plugin_data *>(ctx->user.global);
	void *profile_ptr = ctx->ptr.profile;
	const char *profile_path = profile_get_path(profile_ptr);
	PROFILE_PATH_FIX(profile_path);
	MSG_DEBUG(msg_module, "Creating profile '%s'...", profile_path);

	RRD_wrapper *rrd = nullptr;

	try {
		std::string file;
		file += profile_get_directory(profile_ptr);
		file += "/rrd/";
		file += profile_get_name(profile_ptr);
		file += ".rrd";

		rrd = new RRD_wrapper(instance->cfg->base_dir, file, instance->cfg->interval);
		// Note: If create operation fails, we will still have a wrapper
		rrd->file_create(instance->interval_start, false);

	} catch (std::exception &ex) {
		MSG_WARNING(msg_module, "Failed to create profile '%s': %s",
			profile_path, ex.what());
		return rrd;
	} catch (...) {
		MSG_WARNING(msg_module, "Failed to create profile '%s': %s",
			profile_path, "Unknown error has occurred");
		return rrd;
	}

	MSG_INFO(msg_module, "Profile '%s' has been successfully created.",
		profile_path);
	return rrd;
}

/**
 * \brief Destroy a profile
 *
 * First, flush remaining statistics to the RRD file and then delete RRD
 * wrapper instance. The RRD file will be preserved.
 * \param[in,out] ctx Event context (local and global data)
 */
static void
profile_delete_cb(struct pevents_ctx *ctx)
{
	plugin_data *instance = static_cast<plugin_data *>(ctx->user.global);
	RRD_wrapper *rrd = static_cast<RRD_wrapper *>(ctx->user.local);
	void *profile_ptr = ctx->ptr.profile;
	const char *profile_path = profile_get_path(profile_ptr);
	PROFILE_PATH_FIX(profile_path);
	MSG_DEBUG(msg_module, "Deleting profile '%s'...", profile_path);

	if (!rrd) {
		// Nothing to delete
		return;
	}

	try {
		// Flush up to now statistics and delete the channel
		std::unique_ptr<RRD_wrapper> wrapper(rrd);
		wrapper->file_update(instance->interval_start);
	} catch (std::exception &ex) {
		MSG_WARNING(msg_module, "Failed to properly delete profile '%s': %s",
			profile_path, ex.what());
		return;
	} catch (...) {
		MSG_WARNING(msg_module, "Failed to properly delete profile '%s': %s",
			profile_path, "Unknown error has occurred");
		return;
	}

	MSG_INFO(msg_module, "Profile '%s' has been successfully closed.",
		profile_path);
}

/**
 * \brief Update a profile configuration
 *
 * Update is sensitive only to change of storage directory. Other changes are
 * ignored. If the storage directory is changed, old RRD wrapped is destroyed
 * and replaces with the new one.
 * \param[in,out] ctx Event context (local and global data)
 * \param[in] flags Detected changes in configuration
 */
static void
profile_update_cb(struct pevents_ctx *ctx, uint16_t flags)
{
	if (!(flags & PEVENTS_CHANGE_DIR)) {
		// Directory is still the same i.e. no changes
		return;
	}

	void *profile_ptr = ctx->ptr.profile;
	const char *profile_path = profile_get_path(profile_ptr);
	PROFILE_PATH_FIX(profile_path);
	MSG_DEBUG(msg_module, "Updating profile '%s'...", profile_path);


	// Delete the previous instance of the wrapper
	profile_delete_cb(ctx);

	// Create new one
	RRD_wrapper *new_wrapper;
	new_wrapper = static_cast<RRD_wrapper *>(profile_create_cb(ctx));
	ctx->user.local = new_wrapper;
	if (!new_wrapper) {
		MSG_WARNING(msg_module, "Update process of profile '%s' failed.",
			profile_path);
		return;
	}

	MSG_INFO(msg_module, "Profile '%s' has been successfully updated.",
		profile_path);
}

/**
 * \brief Add a flow
 *
 * Statistics will be stored into an RRD wrapper that aggregates them and later
 * will be flushed to the appropriate RRD file when profile_flush_cb() is
 * called. In other words, by calling this function the database is not
 * immediately updated, only statistics are cached.
 * \param[in,out] ctx  Event context (local and global data)
 * \param[in]     data Pointer to parsed flow features
 */
static void
profile_data_cb(struct pevents_ctx *ctx, void *data)
{
	struct flow_stat *stat = static_cast<struct flow_stat *>(data);
	RRD_wrapper *rrd = static_cast<RRD_wrapper *>(ctx->user.local);
	if (!rrd) {
		return;
	}

	rrd->flow_add(*stat);
}

/**
 * \brief Flush aggregated statistics to an RRD
 *
 * All statistics aggregated since last flush will be stored to the database
 * and the local statistics will be reset to zeros.
 * \param[in,out] ctx Event context (local and global data)
 */
static void
profile_flush_cb(struct pevents_ctx *ctx)
{
	plugin_data *instance = static_cast<plugin_data *>(ctx->user.global);
	RRD_wrapper *rrd = static_cast<RRD_wrapper *>(ctx->user.local);
	if (!rrd) {
		return;
	}

	void *profile_ptr = ctx->ptr.profile;
	const char *profile_path = profile_get_path(profile_ptr);
	PROFILE_PATH_FIX(profile_path);
	MSG_DEBUG(msg_module, "Updating RRD of profile '%s'...", profile_path);

	try {
		rrd->file_update(instance->interval_start);
	} catch (std::exception &ex) {
		MSG_WARNING(msg_module, "Failed to update RRD of profile '%s': %s",
			profile_path, ex.what());
		return;
	} catch (...) {
		MSG_WARNING(msg_module, "Failed to update RRD of profile '%s': %s",
			profile_path, "Unknown error has occurred");
		return;
	}

	MSG_INFO(msg_module, "RRD of profile '%s' has been successfully updated.",
		profile_path);
}

/**
 * \brief Plugin initialization
 *
 * \param[in] params xml   Configuration
 * \param[in] ip_config    Intermediate process config
 * \param[in] ip_id        Intermediate process ID for template manager
 * \param[in] template_mgr Template manager
 * \param[out] config      Config storage
 * \return 0 on success
 */
int
intermediate_init(char* params, void* ip_config, uint32_t ip_id,
	ipfix_template_mgr* template_mgr, void** config)
{
	// Suppress compiler warnings
	(void) ip_id;
	(void) template_mgr;

	if (!params) {
		MSG_ERROR(msg_module, "Missing plugin configuration.", NULL);
		return 1;
	}

	try {
		std::unique_ptr<struct plugin_data> data(new struct plugin_data());
		// Parse parameters
		data.get()->cfg = new plugin_config(params);

		// Create a profile event manager
		struct pevent_cb_set channel_cb;
		memset(&channel_cb, 0, sizeof(channel_cb));
		channel_cb.on_create = channel_create_cb;
		channel_cb.on_delete = channel_delete_cb;
		channel_cb.on_update = channel_update_cb;
		channel_cb.on_data =   channel_data_cb;

		struct pevent_cb_set profile_cb;
		memset(&profile_cb, 0, sizeof(profile_cb));
		profile_cb.on_create = profile_create_cb;
		profile_cb.on_delete = profile_delete_cb;
		profile_cb.on_update = profile_update_cb;
		profile_cb.on_data =   profile_data_cb;

		data.get()->events = pevents_create(profile_cb, channel_cb);
		if (!data.get()->events) {
			throw std::runtime_error("Failed to initialize a manager of "
				"profile events");
		}
		// Global data will be pointer to the plugin instance
		pevents_global_set(data.get()->events, data.get());

		// Save configuration
		data.get()->ip_config = ip_config;
		*config = data.release();

	} catch (const std::exception &ex) {
		// Standard exceptions
		MSG_ERROR(msg_module, "%s", ex.what());
	} catch (...) {
		// Non-standard exceptions
		MSG_ERROR(msg_module, "Unknown exception has occurred.", NULL);
		return 1;
	}

	MSG_DEBUG(msg_module, "Successfully initialized.", NULL);
	return 0;
}

/**
 * \brief Process IPFIX message
 *
 * \param[in] config  Plugin configuration
 * \param[in] message IPFIX message
 * \return 0 on success
 */
int
intermediate_process_message(void* config, void* message)
{
	struct plugin_data *instance = static_cast<struct plugin_data *>(config);
	struct ipfix_message *msg = static_cast<struct ipfix_message *>(message);

	// Catch closing message
	if (msg->source_status == SOURCE_STATUS_CLOSED) {
		pass_message(instance->ip_config, msg);
		return 0;
	}

	// Are we still in the same interval or we should create a new one
	time_t now = time(NULL);
	if (difftime(now, instance->interval_start) > instance->cfg->interval) {
		time_t new_time = now;

		if (instance->cfg->alignment) {
			// We expect that time is integer value
			new_time /= instance->cfg->interval;
			new_time *= instance->cfg->interval;
		}

		// Use the old timestamp to store statistics
		pevents_for_each(instance->events, profile_flush_cb, channel_flush_cb);
		instance->interval_start = new_time;
	}

	// Process all IPFIX records
	struct flow_stat flow_stats;
	for (uint16_t i = 0; i < msg->data_records_count; ++i) {
		struct metadata *mdata = &msg->metadata[i];
		if (!mdata->channels) {
			// No channels -> skip
			continue;
		}

		// Gather flow statistics
		if (flow_stat_prepare(&mdata->record, flow_stats) != 0) {
			continue;
		}

		// Pass the statistics to all affected channels/profiles
		pevents_process(instance->events, (const void **)mdata->channels,
			&flow_stats);
	}

	// Always pass a message
	pass_message(instance->ip_config, msg);
	return 0;
}

/**
 * \brief Close intermediate plugin
 *
 * \param[in] config Plugin configuration
 * \return 0 on success
 */
int
intermediate_close(void *config)
{
	MSG_DEBUG(msg_module, "Closing...", NULL);
	struct plugin_data *instance = static_cast<struct plugin_data *>(config);

	// Flush and destroy all profiles and channels
	pevents_destroy(instance->events);
	instance->events = nullptr;

	delete(instance);
	return 0;
}