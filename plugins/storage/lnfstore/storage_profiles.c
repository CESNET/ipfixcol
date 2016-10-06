/**
 * \file storage.c
 * \author Imrich Stoffa <xstoff02@stud.fit.vutbr.cz>
 * \author Lukas Hutak <xhutak01@stud.fit.vutbr.cz>
 * \author Pavel Krobot <Pavel.Krobot@cesnet.cz>
 * \brief Storage management (source file)
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
#include <ipfixcol/profiles.h>

#include "storage_basic.h"
#include "storage_profiles.h"
#include "translator.h"
#include "bitset.h"
#include <bf_index.h>
#include <libnf.h>
#include <string.h>
#include <libxml/xmlstring.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <stdbool.h>

// Module identification
static const char* msg_module = "lnfstore";



// Profile storage only ---------------------------------------------------- >>>
static void update_profiles_states(struct lnfstore_conf *conf,
								indexing_profiles_events_t event)
{
	if (!conf->profiles_ptr) {
		return;
	}

	for (int i = 0; i < conf->profiles_size; ++i) {
		profile_file_t *profile = &conf->profiles_ptr[i];

		switch(event){
			case IPE_NEW_WINDOW_START:
				if (profile->lnf_index->state == BF_IN_PROGRESS){
					profile->lnf_index->state = BF_CLOSING;
				} else if (profile->lnf_index->state == BF_IN_PROGRESS_FIRST){
					profile->lnf_index->state = BF_CLOSING_FIRST;
				}
				break;

			case IPE_NEW_WINDOW_END:
				if (profile->lnf_index->state == BF_CLOSING ||
						profile->lnf_index->state == BF_CLOSING_FIRST){
					profile->lnf_index->state = BF_IN_PROGRESS;
				} else if (profile->lnf_index->state == BF_INIT){
					profile->lnf_index->state = BF_IN_PROGRESS_FIRST;
				}
				break;

			case IPE_RELOAD_PROFILES:
				if (profile->lnf_index->state == BF_INIT){
					profile->lnf_index->state = BF_IN_PROGRESS_FIRST;
				}
				break;

			case IPE_CLEANUP:
				profile->lnf_index->state = BF_CLOSING_LAST;
				break;

			default:
				break;
		}
	}
}


static int cmp_profile_file(const void* m1, const void* m2)
{
	const profile_file_t *val1 = m1;
	const profile_file_t *val2 = m2;

	if (val1->address == val2->address) {
		return 0;
	}

	return (val1->address < val2->address) ? (-1) : (1);
}


static int open_storage_files(struct lnfstore_conf *conf)
{
	if (!conf->profiles_ptr) {
		return 0;
	}

	// Prepare filenames
	char *index_file;
	char *file_str = create_file_name(conf, &index_file);
	if (!file_str) {
		return 1;
	}

	size_t file_len = strlen(file_str);
	unsigned int flags = LNF_WRITE;
	if (conf->params->compress) {
		flags |= LNF_COMP;
	}

	for (int i = 0; i < conf->profiles_size; ++i) {
		profile_file_t *profile = &conf->profiles_ptr[i];

		const char *dir = profile_get_directory(profile->address);
		size_t size = file_len + strlen(dir) + 2; // '/' + '\0'

		char *total_path = (char *) malloc(size * sizeof(char));
		if (!total_path) {
			MSG_ERROR(msg_module, "Unable to allocate memory (%s:%d)",
				__FILE__, __LINE__);
			continue;
		}

		snprintf(total_path, size, "%s/%s", dir, file_str);
		if (mkdir_hierarchy(total_path) != 0) {
			free(total_path);
			continue;
		}

		int status = lnf_open(&profile->file, total_path, flags,
			conf->params->file_ident);
		if (status != LNF_OK) {
			MSG_ERROR(msg_module, "Failed to create new file '%s'",
				total_path);
			profile->file = NULL;
		}
		free(total_path);

		if (conf->params->bf.indexing) {
			// If lnf index creation failed before, try again
			if (!profile->lnf_index){
				profile->lnf_index = create_lnfstore_index(conf->params->bf);
				if (!profile->lnf_index){
					/// TODO identifikace profilu
					MSG_ERROR(msg_module, "Failed to initialize lnfstore index"
								"for profile TODO, last data file will not be indexed");
					continue;
				}
			}
			// Prepare index
			if (index_file){
				if (prepare_index(profile->lnf_index, conf->params->bf, (char *) dir,
						index_file) != 0){
					MSG_WARNING(msg_module, "Unable to prepare index, last data "
							"file will not be indexed");
					profile->lnf_index->state = BF_ERROR;
				} else {
					if (profile->lnf_index->state == BF_ERROR){
						profile->lnf_index->state = BF_IN_PROGRESS;
					}
				}
			} else {
				/// TODO identifikace profilu
				MSG_WARNING(msg_module, "Unable to get index file name for "
							"profile TODO, last data file will not be indexed");
				profile->lnf_index->state = BF_ERROR;
			}
		}
	}

	free(file_str);
	//free(dir); TODO nechybi??

	return 0;
}

static void close_storage_files(struct lnfstore_conf *conf)
{
	if (!conf->profiles_ptr) {
		return;
	}

	MSG_INFO(msg_module, "Closing files.");

	for (int i = 0; i < conf->profiles_size; ++i) {
		profile_file_t *profile = &conf->profiles_ptr[i];

		if (!profile->file) {
			// File not opened
			continue;
		}

		lnf_close(profile->file);
		profile->file = NULL;

		MSG_INFO(msg_module, "Profile %d indexing state: %d.", i, profile->lnf_index->state);

		if (profile->lnf_index->state == BF_CLOSING || // standard situation
			profile->lnf_index->state == BF_IN_PROGRESS || // reloading profiles inside a time window
			profile->lnf_index->state == BF_CLOSING_FIRST || // standard situation (after first window)
			profile->lnf_index->state == BF_IN_PROGRESS_FIRST || // reloading profiles (inside first window)
			profile->lnf_index->state == BF_CLOSING_LAST) // standard situation (cleaning up)
		{
			if (store_index(profile->lnf_index->index) != BFI_OK){
				print_last_index_error();
				MSG_WARNING(msg_module, "Storing index error, last data file "
							"will not be indexed");
			}
		}
	}
}


static void new_window(time_t now, struct lnfstore_conf *conf)
{
	// Close file(s)
	close_storage_files(conf);

	if (conf->profiles_ptr && conf->params->bf.indexing && conf->params->bf_index_autosize) {
		for (int i = 0; i < conf->profiles_size; ++i) {
			profile_file_t *profile = &conf->profiles_ptr[i];

			if (!profile->lnf_index){
				continue;
			} else {
				unsigned long act_cnt = stored_item_cnt(profile->lnf_index->index);
				double coeff = BF_TOL_COEFF(act_cnt);
				if ((BF_UPPER_TOLERANCE(act_cnt, coeff)) > profile->lnf_index->unique_item_cnt ||
						(profile->lnf_index->state == BF_CLOSING &&
						BF_LOWER_TOLERANCE(act_cnt, coeff) < profile->lnf_index->unique_item_cnt))

				{
//					fprintf(stderr, "Profile %d: State: %d, VAL: %d, COEFF: %f, UP: %d(%f), LOW: %d(%f), SET: %d(%f)\n",
//								i,
//								profile->lnf_index->state,
//								act_cnt,
//								coeff,
//								(act_cnt + (unsigned long)BF_UPPER_TOLERANCE(act_cnt, coeff)),
//								(double)act_cnt + BF_UPPER_TOLERANCE(act_cnt, coeff),
//								act_cnt + (unsigned long)BF_LOWER_TOLERANCE(act_cnt, coeff),
//								(double)act_cnt + BF_LOWER_TOLERANCE(act_cnt, coeff),
//								(unsigned long)(act_cnt*coeff),
//								act_cnt*coeff);
					// Higher act_cnt = make bigger bloom filter
					// Lower act_cnt = save space, make smaller bloom filter
					profile->lnf_index->unique_item_cnt = (unsigned long)(act_cnt * coeff);
					profile->lnf_index->params_changed = true;
				}
			}
		}
	}

	// Update time
	conf->window_start = now;
	if (conf->params->window_align) {
		conf->window_start = (now / conf->params->window_time) *
			conf->params->window_time;
	}

	// Create new files
	open_storage_files(conf);

	MSG_INFO(msg_module, "New time window created.");
}


/**
 * \brief Update the list of profiles
 * \param[in,out] conf Plugin configuration
 * \param[in] profiles List of all active profiles (null terminated)
 * \return On success returns 0. Otherwise returns non-zero value.
 */
static int update_profiles(struct lnfstore_conf *conf, void **profiles)
{
	if (!profiles) {
		return 1;
	}

	// Delete old profiles
	if (conf->profiles_ptr) {
		close_storage_files(conf);
		free(conf->profiles_ptr);
		conf->profiles_ptr = NULL;
		conf->profiles_size = 0;
		bitset_destroy(conf->bitset);
		conf->bitset = NULL;
		/// TODO destroy indexes -> nebude, budou inteligentni profily
	}

	// Create new profiles
	int size;
	for (size = 0; profiles[size] != 0; size++);
	if (size == 0) {
		MSG_WARNING(msg_module, "List of active profiles is empty!");
		return 1;
	}

	conf->profiles_ptr = (profile_file_t *) calloc(size, sizeof(profile_file_t));
	if (!conf->profiles_ptr) {
		MSG_ERROR(msg_module, "Unable to allocate memory (%s:%d)",
			__FILE__, __LINE__);
		return 1;
	}

	conf->bitset = bitset_create(size);
	if (!conf->bitset) {
		MSG_ERROR(msg_module, "Failed to allocate internal bitset.");
		free(conf->profiles_ptr);
		conf->profiles_ptr = NULL;
		return 1;
	}

	conf->profiles_size = size;

	// Initialize profile pointers
	for (int i = 0; i < conf->profiles_size; ++i) {
		profile_file_t *profile = &conf->profiles_ptr[i];
		profile->address = profiles[i];
		profile->file = NULL;
		profile->lnf_index = create_lnfstore_index(conf->params->bf);
		if (!profile->lnf_index){
			MSG_ERROR(msg_module, "Failed to initialize lnfstore index "
						"for profile TODO, last data file will not be indexed");
			continue;
		}
	}

	// Sort profiles for further binary search
	qsort(conf->profiles_ptr, conf->profiles_size, sizeof(profile_file_t),
		cmp_profile_file);

	// Open storage files
	open_storage_files(conf);

	MSG_DEBUG(msg_module, "List of profiles successfully updated.");
	return 0;
}


static int reload_profiles(struct lnfstore_conf *conf, void **channels)
{
	// Get profiles
	void *channel = channels[0];
	void *profile = channel_get_profile(channel);
	void **profiles = profile_get_all_profiles(profile);
	if (!profiles) {
		MSG_ERROR(msg_module, "Failed to get the list of all profiles");
		return 1;
	}

	// Update profiles
	int ret_val;
	ret_val = update_profiles(conf, profiles);
	free(profiles);

	if (ret_val != 0) {
		MSG_ERROR(msg_module, "Failed to update the list of profiles");
		return 1;
	}

	MSG_INFO(msg_module, "Successfully reloaded profiles (%d profiles are "
				"active now).", conf->profiles_size);

	return 0;
}


static void store_to_profiles(struct lnfstore_conf *conf, void **channels)
{
	if (channels == NULL || channels[0] == NULL) {
		return;
	}

	if (conf->profiles_ptr == NULL) {
		// Load all active profiles
		if (reload_profiles(conf, channels) != 0) {
			MSG_ERROR(msg_module, "Failed to reload the list of profiles");
			return;
		}

		update_profiles_states(conf, IPE_RELOAD_PROFILES);
	}

	// Clear aux bitset
	bitset_clear(conf->bitset);

	// Fill bitset
	for (int i = 0; channels[i] != 0; i++) {
		// Create a key
		void *profile_ptr = channel_get_profile(channels[i]);
		profile_file_t key, *result;

		key.address = profile_ptr;
		result = bsearch(&key, conf->profiles_ptr, conf->profiles_size,
			sizeof(profile_file_t), cmp_profile_file);

		if (!result) {
			// Unknown profile... reload list
			if (reload_profiles(conf, channels) != 0) {
				MSG_ERROR(msg_module, "Failed to reload the list of profiles");
				return;
			}

			result = bsearch(&key, conf->profiles_ptr, conf->profiles_size,
				sizeof(profile_file_t), cmp_profile_file);

			if (!result) {
				MSG_ERROR(msg_module, "Failed to find a profile in internal "
					"structures. Something bad happend!");
				return;
			}
		}

		// Get index
		int r_index = (result - conf->profiles_ptr);
		if (bitset_get(conf->bitset, r_index) == true) {
			// Already written to the file
			continue;
		}

		// Store the record only if it is normal profile
		if (profile_get_type(result->address) == PT_NORMAL) {
			store_to_file(result->file, conf, result->lnf_index);
		}

		bitset_set(conf->bitset, r_index, true);
	}
}



void cleanup_storage_profiles(struct lnfstore_conf *conf){
	close_storage_files(conf);

	if (conf->params->bf.indexing){
		for (int i = 0; i < conf->profiles_size; ++i) {
			profile_file_t *profile = &conf->profiles_ptr[i];
			destroy_lnfstore_index(profile->lnf_index);
		}
	}
}
// <<< Profile storage only ------------------------------------------------ <<<

void store_record_profiles(const struct metadata *mdata, struct lnfstore_conf *conf)
{
	if (!mdata->channels) {
		/* Record won't be stored, it does not belong to any channel and
		 * profiling is activated */
		return;
	}

	// Fill record
	lnf_rec_clear(conf->rec_ptr);
	if (fill_record(mdata, conf->rec_ptr, conf->buffer) <= 0) {
		// Nothing to store
		return;
	}

	// Decide whether close files and create new time window
	time_t now = time(NULL);
	if (difftime(now, conf->window_start) > conf->params->window_time) {
		update_profiles_states(conf, IPE_NEW_WINDOW_START);

		new_window(now, conf);

		update_profiles_states(conf, IPE_NEW_WINDOW_END);
	}

	store_to_profiles(conf, mdata->channels);
}
