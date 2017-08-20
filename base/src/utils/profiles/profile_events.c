/**
 * \file profiler_events.c
 * \author Lukas Hutak <xhutak01@stud.fit.vutbr.cz>
 * \brief Profiler events (source file)
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

#include <stdlib.h>
#include <ipfixcol.h>
#include <string.h>
#include <ipfixcol/profiles.h>
#include <ipfixcol/profile_events.h>
#include "bitset.h"

/*
 * TODO aka notes for future development
 * - allow postponed reconfiguration. USECASE: do reconfiguration only when
 *   an old timeslot is closed and a new one is opened.
 */

static const char *msg_module = "profile events";

/** \brief Minimal number of expected profiles */
#define PEVENTS_HINT_PROFILE  (8U)
/** \brief Minimal number of expected channels */
#define PEVENTS_HINT_CHANNELS (32U)
/** \brief Overlap for update (to prevent ofter reallocation) */
#define PEVENTS_HINT_OVERLAP  (8U)

/**
 * \brief Internal structure for a channel or a profile
 */
struct pevents_item {
	/** Pre-prepared context of a channel/profile for callbacks */
	struct pevents_ctx ctx;

	/** Pointer to a pointer (in an internal array) to a parent profile
	 *  (for a channel context) or NULL (for a profile context) i.e. useful
	 *  only for channels. We need the pointer to the pointer to determine
	 *  index in the internal array (see pevents_on_data()) */
	struct pevents_item *parent_ptr;

	/** Unique index of the item in the group (where it belongs) */
	size_t idx;
};

/**
 * \brief Internal group of channels or profiles
 */
struct pevents_group {
	/** Bitset for avoiding calling the same callback on the same
	 *  channel/profile i.e. we want to push data to a channel/profile
	 *  only ONCE per record. */
	bitset_t             *bitset;
	/** Set of callbacks for channels/profiles */
	struct pevent_cb_set  cbs;

	/** Sorted array (by memory address) of all channels/profiles  */
	struct pevents_item **all_ptr;
	/** Number of valid items (channels/profiles) in the array     */
	size_t                all_size;
	/** Number of pre-allocated items (real size of the array)     */
	size_t                all_prealloc;
};

/**
 * \brief Main structure of the event manager
 */
struct pevents_s {
	struct pevents_group channels; /**< Current channels  */
	struct pevents_group profiles; /**< Current profiles  */

	void *user_global; /**< Global user data for callback functions */
};

/**
 * \brief Auxiliary structure for update
 */
struct pevents_update {
	// Initialized nad filled by tree parser
	struct pevents_group channels; /**< New/mapped channels */
	struct pevents_group profiles; /**< New/mapped profiles */

	// Initialized by tree parser, filled by mapped
	uint16_t *chnl_flags; /**< Update flags for the channels */
	uint16_t *prfl_flags; /**< Update flags for the profiles */
};

// -----------------------------------------------------------------------------

/**
 * \brief Initialize a group structure for channels or profiles
 *
 * \param[out] grp       Pointer to the group
 * \param[in]  items_cnt Number of items (can be re-sized in the future)
 * \return On success returns 0. Otherwise returns a non-zero value and the
 *   content of the structure is undefined.
 */
static int
group_init(struct pevents_group *grp, size_t items_cnt)
{
	if (items_cnt == 0) {
		return 1;
	}

	// Clear everything
	memset(grp, 0, sizeof(*grp));

	// Allocate an array of items
	grp->all_prealloc = items_cnt;
	grp->all_ptr = calloc(items_cnt, sizeof(*(grp->all_ptr)));
	if (!grp->all_ptr) {
		MSG_ERROR(msg_module, "Unable to allocate memory (%s:%d)",
			__FILE__, __LINE__);
		return 1;
	}

	// Prepare a bitset
	grp->bitset = bitset_create(items_cnt);
	if (!grp->bitset) {
		MSG_ERROR(msg_module, "Unable to allocate memory (%s:%d)",
			__FILE__, __LINE__);
		free(grp->all_ptr);
		return 1;
	}

	return 0;
}

/**
 * \brief Increase a number of pre-allocated items in a group
 *
 * \warning The size of the array can be only increased. If the functions
 *   is called with a smaller number of items, the function will do nothing
 *   and returns successfully.
 * \param[in,out] grp       Pointer to the group
 * \param[in]     items_cnt New of items
 * \return On success returns 0. Otherwise returns a non-zero value and the
 *   content of the group is unchanged.
 */
static int
group_resize(struct pevents_group *grp, size_t items_cnt)
{
	if (grp->all_prealloc >= items_cnt) {
		// Do not decrease the size
		return 0;
	}

	// Re-alloc the array
	struct pevents_item **new_items;
	new_items = realloc(grp->all_ptr, items_cnt * sizeof(*(grp->all_ptr)));
	if (!new_items) {
		MSG_ERROR(msg_module, "Unable to reallocate memory (%s:%d)",
			__FILE__, __LINE__);
		return 1;
	}

	grp->all_ptr = new_items;

	// Re-alloc the bitset
	if (bitset_resize(grp->bitset, items_cnt)) {
		// Failed to resize the bitset
		return 1;
	}

	grp->all_prealloc = items_cnt;
	return 0;
}

/**
 * \brief Free internal elements of a group
 * \warning The structure itself is not freed
 * \param[in,out] grp Pointer to the group
 */
static void
group_deinit(struct pevents_group *grp)
{
	if (grp->all_ptr != NULL) {
		// Delete items
		for (size_t i = 0; i < grp->all_size; ++i) {
			free(grp->all_ptr[i]);
		}
	}

	free(grp->all_ptr);
	bitset_destroy(grp->bitset);
}

/**
 * \brief Add a new item into a group
 *
 * If the group is not big enough, the internal array of items is automatically
 * resized. An index of the new item is set to corresponding value. Other value
 * are set to default values (zeros).
 * \param[in,out] grp Pointer to the group
 * \return On success returns a pointer the to new item. Otherwise (memory
 *   re-allocation error) returns NULL.
 */
static struct pevents_item *
group_item_new(struct pevents_group *grp)
{
	if (grp->all_size == grp->all_prealloc) {
		if (group_resize(grp, 2U * grp->all_prealloc)) {
			// Failed
			return NULL;
		}
	}

	struct pevents_item *new_item;
	if ((new_item = calloc(1, sizeof(*new_item))) == NULL) {
		MSG_ERROR(msg_module, "Unable to allocate memory (%s:%d)",
			__FILE__, __LINE__);
		return NULL;
	}

	new_item->idx = grp->all_size; // Set an index
	grp->all_ptr[grp->all_size++] = new_item;
	return new_item;
}

/**
 * \brief Compare two event items in an event group
 *
 * The items are compared by addresses of their profile/channel
 * \param[in] m1 Pointer to a pointer to the first group item
 * \param[in] m2 Pointer to a pointer to the second group item
 * \return The function returns an integer less than, equal to, or greater
 *   than zero if the address of the first item \p m1 is found, respectively,
 *   to be less than, to match, or be greater than the address of \p s2.
 */
static int
group_item_cmp(const void *m1, const void *m2)
{
	const struct pevents_item *val1 = *((struct pevents_item **) m1);
	const struct pevents_item *val2 = *((struct pevents_item **) m2);

	// Compare pointers (union)
	if (val1->ctx.ptr.channel == val2->ctx.ptr.channel) {
		return 0;
	}

	return (val1->ctx.ptr.channel < val2->ctx.ptr.channel) ? (-1) : (1);
}

/**
 * \brief Find an item in a group
 * \param[in] grp  Group
 * \param[in] item Pointer to the item key (channel/profile from a profiler)
 * \return On success returns a pointer to a pointer (in the internal array)
 *   to the item in the group. Otherwise returns NULL.
 */
static struct pevents_item *
group_item_find(struct pevents_group *grp, const void *item)
{
	// Prepare key (only an address of channel/profile is used in a compare fn.)
	struct pevents_item key;
	key.ctx.ptr.channel = (void *) item; // It is union (channel == profile)
	const struct pevents_item *key_ptr = &key;

	struct pevents_item **res;
	res = bsearch(&key_ptr, grp->all_ptr, grp->all_size,
		sizeof(struct pevents_item *), group_item_cmp);

	return (res != NULL) ? (*res) : NULL;
}

/**
 * \brief Get an item with specified index
 *
 * If the index is outside of the array, returns NULL.
 * \param[in] grp Group
 * \param[in] idx Index
 * \return On success returns a pointer to the item. Otherwise returns NULL.
 */
static struct pevents_item *
group_item_at(struct pevents_group *grp, size_t idx)
{
	if (idx >= grp->all_size) {
		return NULL;
	}

	return grp->all_ptr[idx];
}

/**
 * \brief Sort an internal array with items
 *
 * The array with items is sorted based on a key of each item. The key is
 * represented by an address of a channel/profile from a profiler. Thus,
 * items are sorted by their channel/profile memory addresses.
 *
 * \note After sort, indexes of items are recalculated.
 * \param[in,out] grp Group
 */
static void
group_sort(struct pevents_group *grp)
{
	qsort(grp->all_ptr, grp->all_size, sizeof(struct pevents_item *),
		group_item_cmp);

	// Update indexes
	for (size_t i = 0; i < grp->all_size; ++i) {
		struct pevents_item *item = grp->all_ptr[i];
		item->idx = i;
	}
}

/**
 * \brief Call "on_create" callback on selected items in a group of
 *   channels/profiles
 *
 * The function considers items with unset bits (in the group bitset) as
 * selected items.
 * \param[in,out] grp Group of channels or profiles
 */
static void
group_on_create(struct pevents_group *grp)
{
	pevents_create_cb create_fn = grp->cbs.on_create;
	if (create_fn == NULL) {
		// No callback -> do nothing
		return;
	}

	const size_t grp_size = grp->all_size;
	for (size_t i = 0; i < grp_size; ++i) {
		if (bitset_get_fast(grp->bitset, i)) {
			// Skip
			continue;
		}

		struct pevents_item *item = group_item_at(grp, i);
		item->ctx.user.local = (*create_fn)(&item->ctx);
	}
}

/**
 * \brief Call "on_delete" callback on selected items in a group of
 *   channels/profiles
 *
 * The function considers items with unset bits (in the group bitset) as
 * selected items.
 * \param[in,out] grp Group of channels or profiles
 */
static void
group_on_delete(struct pevents_group *grp)
{
	pevents_delete_cb delete_fn = grp->cbs.on_delete;
	if (delete_fn == NULL) {
		// No callback -> do nothing
		return;
	}

	const size_t grp_size = grp->all_size;
	for (size_t i = 0; i < grp_size; ++i) {
		if (bitset_get_fast(grp->bitset, i)) {
			// Skip
			continue;
		}

		struct pevents_item *item = group_item_at(grp, i);
		(*delete_fn)(&item->ctx);
	}
}

/**
 * \brief Call "on_update" callback on successfully mapped items with
 *   modified configuration (in a group of channels or profiles).
 *
 * An array of flags \p flag_arr MUST be at least the same size as number of
 * items in the group. Each field corresponds to an item in the group with
 * the same index. See #PEVENTS_CHANGE for meaning of flags.
 * The function considers items with a non-zero flag as the items to update.
 * \param[in,out] grp      Group of channels or profiles
 * \param[in]     flag_arr An array of flags. It MUST be
 */
static void
group_on_update(struct pevents_group *grp, uint16_t *flag_arr)
{
	pevents_update_cb update_fn = grp->cbs.on_update;
	if (update_fn == NULL) {
		// No callback -> do nothing
		return;
	}

	const size_t grp_size = grp->all_size;
	for (size_t i = 0; i < grp_size; ++i) {
		// Only successfully mapped items can have a non-zero flags
		if (flag_arr[i] == 0) {
			// No update -> skip
			continue;
		}

		struct pevents_item *item = group_item_at(grp, i);
		(*update_fn)(&item->ctx, flag_arr[i]);
	}
}

/**
 * \brief Call a callback function on all items in a group
 * \param[in,out] grp Group of channels or profiles
 * \param[in]     fn  Callback function
 */
static void
group_on_general(struct pevents_group *grp, pevents_fn fn)
{
	// Call the callback on all items in the group
	const size_t grp_cnt = grp->all_size;

	for (size_t i = 0; i < grp_cnt; ++i) {
		struct pevents_item *item = group_item_at(grp, i);
		(*fn)(&item->ctx);
	}
}

// -----------------------------------------------------------------------------

/**
 * \brief Create new groups of channel and profile from a new profile tree
 *
 * The function will fill and sort both groups (channels and profiles).
 * Arrays of change flags are also initialized (all flags are unset).
 *
 * \param[in,out] update    Update structure
 * \param[in]     tree_root Root of the new profile tree
 * \return On success returns 0. Otherwise returns a non-zero value, the
 *   content of the \p update is undefined and the \p update structure MUST
 *   be deleted by pevents_update_delete().
 */
static int
pevents_update_parse_tree(struct pevents_update *update, void *tree_root)
{
	void **profile_list = profile_get_all_profiles(tree_root);
	if (!profile_list) {
		return 1;
	}

	// Process the list of profiles
	for (size_t i = 0; profile_list[i] != NULL; ++i) {
		// Add each profile to the new profile group
		void *profile_ptr = profile_list[i];
		struct pevents_item *profile_item;

		if ((profile_item = group_item_new(&update->profiles)) == NULL) {
			// Failed to add a new item to the internal array
			free(profile_list);
			return 1;
		}

		profile_item->ctx.ptr.profile = profile_ptr;
		profile_item->parent_ptr = NULL;

		const uint16_t channel_cnt = profile_get_channels(profile_ptr);
		for (uint16_t j = 0; j < channel_cnt; ++j) {
			// Add each channel to the new channel group
			void *channel_ptr = profile_get_channel(profile_ptr, j);
			if (!channel_ptr) {
				free(profile_list);
				return 1;
			}

			struct pevents_item *channel_item;
			if ((channel_item = group_item_new(&update->channels)) == NULL) {
				// Failed to add a new item to the internal array
				free(profile_list);
				return 1;
			}

			channel_item->ctx.ptr.channel = channel_ptr;
			channel_item->parent_ptr = profile_item;
		}
	}

	free(profile_list);

	// Now we know number of items in groups -> initialize an array of flags
	const size_t chnl_cnt = update->channels.all_size;
	const size_t prfl_cnt = update->profiles.all_size;

	update->chnl_flags = calloc(chnl_cnt, sizeof(*update->chnl_flags));
	update->prfl_flags = calloc(prfl_cnt, sizeof(*update->prfl_flags));

	if (!update->prfl_flags || !update->chnl_flags) {
		// Memory allocation failed
		MSG_ERROR(msg_module, "Unable to allocate memory (%s:%d)",
				__FILE__, __LINE__);
		return 1;
	}

	// Sort the channels and profiles
	group_sort(&update->channels);
	group_sort(&update->profiles);
	return 0;
}

/**
 * \brief Delete an update structure
 *
 * Free internal auxiliary structures and then delete the structure.
 * \param[in,out] update Pointer to the structure
 */
static void
pevents_update_delete(struct pevents_update *update)
{
	group_deinit(&update->channels);
	group_deinit(&update->profiles);

	free(update->chnl_flags);
	free(update->prfl_flags);

	free(update);
}

/**
 * \brief Create a new update structure and parse a new profile tree
 *
 * Returned structure is prepared for mapping old channels/profiles to new
 * channels/profiles (those are store in this update structure).
 * \param[in] mgr          Event manager
 * \param[in] tree_root    Root profile of the new profile tree
 * \return On success returns a pointer to the structure. Otherwise (memory
 *   allocation error) returns NULL.
 */
static struct pevents_update *
pevents_update_create(pevents_t *mgr, void *tree_root)
{
	struct pevents_update *update;
	update = (struct pevents_update *) calloc(1, sizeof(*update));
	if (!update) {
		MSG_ERROR(msg_module, "Unable to allocate memory (%s:%d)",
			__FILE__, __LINE__);
		return NULL;
	}

	struct pevents_group *chnl_grp = &update->channels;
	struct pevents_group *prfl_grp = &update->profiles;

	/* Prepare hints (expected number of channels/profiles)
	 * Helps to prevent expensive reallocations. */
	size_t profile_hint = mgr->profiles.all_size + PEVENTS_HINT_OVERLAP;
	size_t channel_hint = mgr->channels.all_size + PEVENTS_HINT_OVERLAP;

	if (profile_hint < PEVENTS_HINT_PROFILE) {
		profile_hint = PEVENTS_HINT_PROFILE;
	}

	if (channel_hint < PEVENTS_HINT_CHANNELS) {
		channel_hint = PEVENTS_HINT_CHANNELS;
	}

	// Initialize groups for channels and profiles
	if (group_init(prfl_grp, profile_hint)) {
		free(update);
		return NULL;
	}

	if (group_init(chnl_grp, channel_hint)) {
		group_deinit(prfl_grp);
		free(update);
		return NULL;
	}

	// Parse the profile tree
	if (pevents_update_parse_tree(update, tree_root)) {
		pevents_update_delete(update);
		return NULL;
	}

	// Set user global variables
	for (size_t i = 0; i < chnl_grp->all_size; ++i) {
		struct pevents_item *item = group_item_at(chnl_grp, i);
		item->ctx.user.global = mgr->user_global;
	}

	for (size_t i = 0; i < prfl_grp->all_size; ++i) {
		struct pevents_item *item = group_item_at(prfl_grp, i);
		item->ctx.user.global = mgr->user_global;
	}

	// Success
	return update;
}

/**
 * \brief Compare old and new channels and produce change flags
 *
 * This function compares channels and as result it produces flags with
 * changes. For more information see #PEVENTS_CHANGE.
 * \note The function MUST be successfully mapped, otherwise information
 *   do not make sense.
 * \param[in] ch_old Old channel (from a profiler)
 * \param[in] ch_new New channel (from a profiler)
 * \return Flags with changes.
 */
static uint16_t
pevents_update_mapper_change_flags(void *ch_old, void *ch_new)
{
	void *prfl_new = channel_get_profile(ch_new);
	void *prfl_old = channel_get_profile(ch_old);

	uint16_t flags = 0U;

	// Check storage directory
	const char *dir_new = profile_get_directory(prfl_new);
	const char *dir_old = profile_get_directory(prfl_old);
	if (strcmp(dir_new, dir_old) != 0) {
		// The directory has been changed
		flags |= PEVENTS_CHANGE_DIR;
	}

	// Check profile type
	enum PROFILE_TYPE type_new = profile_get_type(prfl_new);
	enum PROFILE_TYPE type_old = profile_get_type(prfl_old);
	if (type_new != type_old) {
		// The type has been changed
		flags |= PEVENTS_CHANGE_TYPE;
	}

	return flags;
}

/**
 * \brief Find an old channel record that corresponds to a new channel
 * \param[in] mgr    Event manager
 * \param[in] ch_new Pointer to the new channel (from a profiler)
 * \return If the record is in the manager, returns a pointer to the record.
 *   Otherwise (the record not found) returns NULL.
 */
static struct pevents_item *
pevents_update_mapper_find_old_channel(pevents_t *mgr, void *ch_new)
{
	// New channel path and name
	const char *path_new = channel_get_path(ch_new);
	const char *name_new = channel_get_name(ch_new);
	const char *path_old;
	const char *name_old;

	struct pevents_group *ch_grp = &mgr->channels; // Old channels
	const size_t old_chnl_cnt = ch_grp->all_size;  // Number of old channels

	for (size_t i = 0; i < old_chnl_cnt; ++i) {
		// Check if the channel has been already mapped (perform. optimization)
		if (bitset_get_fast(ch_grp->bitset, i)) {
			// Already mapped -> skip
			continue;
		}

		// Get the old channel
		struct pevents_item *item = group_item_at(ch_grp, i);
		void *ch_old = item->ctx.ptr.channel;

		// Compare names (shorter names -> faster)
		name_old = channel_get_name(ch_old);
		if (strcmp(name_old, name_new) != 0) {
			continue;
		}

		// Compare path
		path_old = channel_get_path(ch_old);
		if (strcmp(path_new, path_old) != 0) {
			continue;
		}

		return item;
	}

	// Not found
	return NULL;
}

/**
 * \brief Map old channels/profiles to new channels/profiles
 *
 * Mapping is trying to find the same channels/profiles in an old and a new
 * configuration based on their names and hierarchy position in a profile tree.
 * These channels/profiles should stay in an Event manager without
 * reinitialization. However, configuration of channels/profiles can be
 * changes e.g. new storage directory, different type (normal/shadow), etc.
 * Therefore, change flags are genereted for each successful mapping.
 *
 * After execution of this function: Set bits in old bitsets (mgr->channel and
 * mgr->profiles) represent successfully mapped OLD channels/profiles. Set
 * bits in new bitsets (update->channel and update->profiles) represent
 * successfully mapped NEW channels/profiles and corresponding change flags
 * (update->chnl_flags and update->prfl_flags) are filled (an index in
 * the "new" bitset is equal to the index in corresponding flag array).
 * Pointers to local user data of each successfully mapped channel/profile
 * are copied to new channels/profiles (update->channels and profiles).
 *
 * Thus, unset bits in the old bitsets represent channels/profiles to delete.
 * Unset bits in the new bitsets represent channels/profiles to create.
 * Non-zero flags in the flags array represent channels/profiles to update.
 *
 * \note The function expects that an update structure is already initialized
 *   and a profile tree has been parsed.
 * \param[in,out] mgr    Event manager
 * \param[in,out] update Update structure
 */
static void
pevents_update_mapper(pevents_t *mgr, struct pevents_update *update)
{
	/* Try to map new channels to the new profiles.
	 * Each profile must have at least one channel, so we just need to remap
	 * new channels to old channels and simultaneously remap also their
	 * profiles.
	 *
	 * Note:
	 * - mgr->channels and mgr->profiles represents old channels/profiles
	 * - update->channels and update->profiles represents new channels/profiles
	 */

	if (update->channels.all_size == 0) {
		// Nothing for mapping -> skip
		return;
	}

	// Old/new bitset represents successfully mapped old/new channels/profiles
	bitset_clear(mgr->channels.bitset);
	bitset_clear(mgr->profiles.bitset);

	bitset_clear(update->channels.bitset);
	bitset_clear(update->profiles.bitset);

	struct pevents_item *item_chnl_new;
	struct pevents_item *item_chnl_old;

	const size_t new_chnl_cnt = update->channels.all_size;
	for (size_t i = 0; i < new_chnl_cnt; ++i) {
		// Get one of the new channels
		item_chnl_new = group_item_at(&update->channels, i);
		void *ch_new = item_chnl_new->ctx.ptr.channel;

		// Find corresponding old channel
		item_chnl_old = pevents_update_mapper_find_old_channel(mgr, ch_new);
		if (!item_chnl_old) {
			// Failed to find the old channel
			continue;
		}

		// Get flags of changes
		void *ch_old = item_chnl_old->ctx.ptr.channel;
		uint16_t flags = pevents_update_mapper_change_flags(ch_old, ch_new);

		// Indicate successful channel mapping and move local/global data
		bitset_set_fast(mgr->channels.bitset, item_chnl_old->idx, true);
		bitset_set_fast(update->channels.bitset, item_chnl_new->idx, true);
		item_chnl_new->ctx.user.local = item_chnl_old->ctx.user.local;
		update->chnl_flags[i] = flags;

		// Check if the profile has been mapped
		struct pevents_item *item_prfl_new = item_chnl_new->parent_ptr;
		struct pevents_item *item_prfl_old = item_chnl_old->parent_ptr;

		if (bitset_get_fast(update->profiles.bitset, item_prfl_new->idx)) {
			// Already mapped -> skip
			continue;
		}

		// Indicate successful profile mapping and move local/global data
		bitset_set_fast(mgr->profiles.bitset, item_prfl_old->idx, true);
		bitset_set_fast(update->profiles.bitset, item_prfl_new->idx, true);
		item_prfl_new->ctx.user.local = item_prfl_old->ctx.user.local;
		update->prfl_flags[item_prfl_new->idx] = flags;
	}
}



/**
 * \brief Apply the update structure to the current configuration and execute
 *   callbacks (create/update/delete) on channels/profiles
 * \note After update, the \p update structure is automatically freed.
 * \param[in,out] mgr    Event manager
 * \param[in]     update Update structure
 */
static void
pevents_update_apply(pevents_t *mgr, struct pevents_update *update)
{
	// Copy callbacks from old channels/profiles to new channels/profiles
	update->profiles.cbs = mgr->profiles.cbs;
	update->channels.cbs = mgr->channels.cbs;

	// Call "on_delete" callbacks on OLD profiles/channels
	group_on_delete(&mgr->profiles);
	group_on_delete(&mgr->channels);

	// Call "on_update" callbacks on successfully mapped profiles/channels
	group_on_update(&update->profiles, update->prfl_flags);
	group_on_update(&update->channels, update->chnl_flags);

	// Call "on_create" callbacks on NEW profiles/channels
	group_on_create(&update->profiles);
	group_on_create(&update->channels);

	// Delete old information about channels/profiles and apply the update
	group_deinit(&mgr->profiles);
	group_deinit(&mgr->channels);
	mgr->profiles = update->profiles;
	mgr->channels = update->channels;

	// Cleanup after the update
	bitset_clear(mgr->profiles.bitset);
	bitset_clear(mgr->channels.bitset);

	free(update->chnl_flags);
	free(update->prfl_flags);
	free(update);
}

// -----------------------------------------------------------------------------

/**
 * \brief Load a new profile tree and apply changes to the current
 *   configuration.
 * \warning All bitsets will be cleared because order of profiles/channels can
 *   be changed.
 * \param[in] mgr     Event manager
 * \param[in] channel Any channel from the new profile tree
 * \return On success returns 0. Otherwise returns a non-zero value and
 *   the current configuration is not modified.
 */
static int
pevents_reload(pevents_t *mgr, const void *channel)
{
	// Find root profile of the new tree
	void *tree_root = channel_get_profile((void *)channel); // UGLY!
	void *tmp_profile = tree_root;

	while (tmp_profile) {
		tree_root = tmp_profile;
		tmp_profile = profile_get_parent(tmp_profile);
	}

	// Create an update structure and parse the tree
	struct pevents_update *update;
	update = pevents_update_create(mgr, tree_root);
	if (!update) {
		return 1;
	}

	// Map the current channels/profiles to the new channels/profiles
	pevents_update_mapper(mgr, update);
	// Apply the update
	pevents_update_apply(mgr, update);
	// The update structure has been adsorbed by update... (no free)
	return 0;
}

/**
 * \brief Push a record to a one channel and its parent profile
 *
 * First, find an internal record of channel the and if it hasn't been
 * processed earlier call its data callback. Second, determine the parent
 * profile and if it hasn't been processed earlier call its data callback.
 *
 * \param[in,out] mgr     Event manager
 * \param[in]     channel Pointer to channel (from a profiler)
 * \param[in]     rec     Record for the channel and the profile
 * \return If the record has been pushed to the channel and the profile returns
 *   0. If the channel (and the profile) is not present in the event manager
 *   returns a non-zero value (i.e. new profile tree MUST be parsed).
 */
static int
pevents_data(pevents_t *mgr, const void *channel, void *rec)
{
	// Find the channel
	struct pevents_group *grp_chnl = &mgr->channels;
	struct pevents_item  *item_chnl;

	if ((item_chnl = group_item_find(grp_chnl, channel)) == NULL) {
		// Not found -> reconfigure
		return 1;
	}

	// Has the channel been processed?
	if (bitset_get_and_set(grp_chnl->bitset, item_chnl->idx, true) == true) {
		// It has ... skip
		return 0;
	}

	if (grp_chnl->cbs.on_data != NULL) {
		(*grp_chnl->cbs.on_data)(&item_chnl->ctx, rec);
	}

	// Process the profile
	struct pevents_group *grp_prfl = &mgr->profiles;
	struct pevents_item  *item_prfl = item_chnl->parent_ptr;

	// Has the profile been processed?
	if (bitset_get_and_set(grp_chnl->bitset, item_prfl->idx, true) == true) {
		// It has ...
		return 0;
	}

	if (grp_prfl->cbs.on_data != NULL) {
		(*grp_prfl->cbs.on_data)(&item_prfl->ctx, rec);
	}

	return 0;
}

// -----------------------------------------------------------------------------

pevents_t *
pevents_create(struct pevent_cb_set profiles, struct pevent_cb_set channels)
{
	pevents_t *events = calloc(1, sizeof(*events));
	if (!events) {
		MSG_ERROR(msg_module, "Unable to allocate memory (%s:%d)",
				__FILE__, __LINE__);
		return NULL;
	}

	/* Initialize an internal structures. Because we don't have information
	 * about profiles/channels we should prepare empty structures.
	 * However, pre-allocation for "0" items is not suitable, so we use "1".
	 */
	if (group_init(&events->profiles, 1)) {
		free(events);
		return NULL;
	}

	if (group_init(&events->channels, 1)) {
		group_deinit(&events->profiles);
		free(events);
		return NULL;
	}

	events->channels.cbs = channels;
	events->profiles.cbs = profiles;
	return events;
}

void
pevents_destroy(pevents_t *mgr)
{
	// Call on "on_delete" callbacks on all profiles and channels
	bitset_clear(mgr->profiles.bitset);
	bitset_clear(mgr->channels.bitset);

	group_on_delete(&mgr->profiles);
	group_on_delete(&mgr->channels);

	// Delete internal structures for profiles and channels
	group_deinit(&mgr->profiles);
	group_deinit(&mgr->channels);

	free(mgr);
}

void
pevents_global_set(pevents_t *mgr, void *global)
{
	mgr->user_global = global;

	// Change the pointer to the global variable for all channels
	for (size_t i = 0; i < mgr->channels.all_size; ++i) {
		struct pevents_item *item_chnl = mgr->channels.all_ptr[i];
		item_chnl->ctx.user.global = global;
	}

	// Change the pointer to the global variable for all profiles
	for (size_t i = 0; i < mgr->profiles.all_size; ++i) {
		struct pevents_item *item_prfl = mgr->profiles.all_ptr[i];
		item_prfl->ctx.user.global = global;
	}
}

void *
pevents_global_get(pevents_t *mgr)
{
	return mgr->user_global;
}

int
pevents_process(pevents_t *mgr, const void **channels, void *data)
{
	// Clear bitsets
	bitset_clear(mgr->profiles.bitset);
	bitset_clear(mgr->channels.bitset);

	for (unsigned int i = 0; channels[i] != NULL; i++) {
		// Push data to a channel and its parent profile
		if (pevents_data(mgr, channels[i], data) == 0) {
			// Success
			continue;
		}

		if (i != 0) {
			// Reconfiguration should happen only with the new record!!!
			// This is ugly, because the bitsets will be cleared during reconf.
			MSG_ERROR(msg_module, "Internal error: Reconfiguration request "
				"happened during processing of other channel that the first "
				"one.");
		}

		// Failed -> Reload configuration
		if (pevents_reload(mgr, channels[i])) {
			MSG_ERROR(msg_module, "Failed to reload profiling configuration. ");
			return 1;
		}

		// Try to push data again
		if (pevents_data(mgr, channels[i], data) == 0) {
			// Success
			continue;
		}

		// Failed -> skip the channel
		MSG_ERROR(msg_module, "(internal error) Failed to find a channel info "
			"after reconfiguration. A record will not be stored.");
		return 1;
	}

	return 0;
}

void
pevents_for_each(pevents_t *mgr, pevents_fn prfl_fn, pevents_fn chnl_fn)
{
	if (prfl_fn != NULL) {
		group_on_general(&mgr->profiles, prfl_fn);
	}

	if (chnl_fn != NULL) {
		group_on_general(&mgr->channels, chnl_fn);
	}
}
