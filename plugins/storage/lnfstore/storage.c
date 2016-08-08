/**
 * \file storage.c
 * \author Imrich Stoffa <xstoff02@stud.fit.vutbr.cz>
 * \author Lukas Hutak <xhutak01@stud.fit.vutbr.cz>
 * \brief Storage management (source file)
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

#include <ipfixcol.h>
#include <ipfixcol/profiles.h>

#include "storage.h"
#include "translator.h" 
#include "bitset.h"
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

int cmp_profile_file(const void* m1, const void* m2)
{
	const profile_file_t *val1 = m1;
	const profile_file_t *val2 = m2;
	
	if (val1->address == val2->address) {
		return 0;
	}

	return (val1->address < val2->address) ? (-1) : (1);
}

int mkdir_hierarchy(const char* path)
{
	struct stat s;
	char* pos = NULL;
	char* realp = NULL;
	unsigned subs_len = 0;
	while ((pos = strchr(path + subs_len + 1, '/')) != NULL ) {
		subs_len = pos - path;
		int status;
		bool failed = false;
		realp = strndup(path, subs_len);
try_again:
		status = stat(realp, &s);
		if (status == -1) {
			if (ENOENT == errno) {
				if (mkdir(realp, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)) {
					if (!failed) {
						failed = true;
						goto try_again;
					}
					MSG_ERROR(msg_module, "Failed to create directory: %s", realp);
					return 1;
				}
			}
		} else if (!S_ISDIR(s.st_mode)) {
			MSG_ERROR(msg_module, "Failed to create directory, %s is file", realp);
			return 2;
		}
		free(realp);
	}
	return 0;
}

/**
 * \brief Fill new record
 * \param[in] mdata IPFIX record
 * \param[out] record Record
 * \param[in,out] buffer Pointer to temporary buffer
 * \return Number of converted fields
 */
int fill_record(const struct metadata *mdata, lnf_rec_t *record, uint8_t *buffer)
{
	int added = 0;

	uint16_t offset = 0;
	uint16_t length;

	struct ipfix_template *templ = mdata->record.templ;
	uint8_t *data_record = (uint8_t*) mdata->record.record;

    // Process all fields
    for (uint16_t count = 0, index = 0; count < templ->field_count; ++count, ++index) {
		// Create a key - get Enterprise and Element ID
		struct ipfix_lnf_map key, *item;

		key.ie = templ->fields[index].ie.id;
		length = templ->fields[index].ie.length;
		key.en = 0;

		if (key.ie & 0x8000) {
			key.ie &= 0x7fff;
			key.en = templ->fields[++index].enterprise_number;
		}

		// Find conversion function
		item = bsearch(&key, tr_table, MAX_TABLE , sizeof(struct ipfix_lnf_map),
			ipfix_lnf_map_compare);

		int conv_failed = 1;
		if (item != NULL) {
			// Convert
			conv_failed = item->func(data_record, &offset, &length, buffer, item);
			if (!conv_failed) {
				lnf_rec_fset(record, item->lnf_id, buffer);
				added++;
			}
		}

		if (conv_failed) {
			length = real_length(data_record, &offset, length);
		}

		offset += length;
    }

    return added;
}

char *create_file_name(struct lnfstore_conf *conf)
{
	struct tm gm;
	if (gmtime_r(&conf->window_start, &gm) == NULL) {
		MSG_ERROR(msg_module, "Failed to convert time to UTC.");
		return NULL;
	}

	char time_path[13];
	size_t length;

	length = strftime(time_path, sizeof(time_path), "/%Y/%m/%d/", &gm);
	if (length == 0) {
		MSG_ERROR(msg_module, "Failed to fill file path template.");
		return NULL;
	}

	char file_suffix[1024];
	length = strftime(file_suffix, sizeof(file_suffix),
		conf->params->file_suffix, &gm);
	if (length == 0) {
		MSG_ERROR(msg_module, "Failed to fill file path template (suffix).");
		return NULL;
	}

	length = strlen(time_path) + strlen(conf->params->file_prefix)
		+ strlen(file_suffix) + 1;

	char *file_name = (char *) malloc(length * sizeof(char));
	if (!file_name) {
		MSG_ERROR(msg_module, "Unable to allocate memory (%s:%d)",
			__FILE__, __LINE__);
		return NULL;
	}

	snprintf(file_name, length, "%s%s%s", time_path, conf->params->file_prefix,
		file_suffix);
	return file_name;
}

int open_storage_files(struct lnfstore_conf *conf)
{
	// Prepare filename(s)
	char *file_str = create_file_name(conf);
	if (!file_str) {
		return 1;
	}

	size_t file_len = strlen(file_str);
	unsigned int flags = LNF_WRITE;
	if (conf->params->compress) {
		flags |= LNF_COMP;
	}

	if (conf->params->profiles) {
		// With profiler
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
		}
	} else {
		// Without profiler
		size_t size = file_len + strlen(conf->params->storage_path) + 2; // '/' + '\0'
		char *total_path = (char *) malloc(size *sizeof(char));
		if (!total_path) {
			MSG_ERROR(msg_module, "Unable to allocate memory (%s:%d)",
				__FILE__, __LINE__);
			free(file_str);
			return 1;
		}

		snprintf(total_path, size, "%s/%s", conf->params->storage_path, file_str);
		if (mkdir_hierarchy(total_path) != 0) {
			free(total_path);
			free(file_str);
			return 1;
		}

		int status = lnf_open(&conf->file_ptr, total_path, flags,
			conf->params->file_ident);
		if (status != LNF_OK) {
			MSG_ERROR(msg_module, "Failed to create new file '%s'",
				total_path);
			conf->file_ptr = NULL;
		}

		free(total_path);
	}

	free(file_str);
	return 0;
}

void close_storage_files(struct lnfstore_conf *conf)
{
	if (conf->params->profiles) {
		// With profiler
		if (!conf->profiles_ptr) {
			return;
		}

		for (int i = 0; i < conf->profiles_size; ++i) {
			profile_file_t *profile = &conf->profiles_ptr[i];

			if (!profile->file) {
				// File not opened
				continue;
			}

			lnf_close(profile->file);
			profile->file = NULL;
		}
	} else {
		// Without profiler
		if (!conf->file_ptr) {
			return;
		}

		lnf_close(conf->file_ptr);
		conf->file_ptr = NULL;
	}
}


void new_window(time_t now, struct lnfstore_conf *conf)
{
	// Close file(s)
	close_storage_files(conf);

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


void store_to_file(lnf_file_t *file, lnf_rec_t *rec)
{
	if (!file) {
		return;
	}

	lnf_write(file, rec);
}

/**
 * \brief Update the list of profiles
 * \param[in,out] conf Plugin configuration
 * \param[in] profiles List of all active profiles (null terminated)
 * \return On success returns 0. Otherwise returns non-zero value.
 */
int update_profiles(struct lnfstore_conf *conf, void **profiles)
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
	}

	// Create new profiles
	int size;
	for (size = 0; profiles[size] != 0; size++);
	if (size == 0) {
		MSG_WARNING(msg_module, "List of active profile is empty!");
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

	// Init profile pointers
	for (int i = 0; i < conf->profiles_size; ++i) {
		profile_file_t *profile = &conf->profiles_ptr[i];
		profile->address = profiles[i];
		profile->file = NULL;
	}

	// Sort profiles for further binary search
	qsort(conf->profiles_ptr, conf->profiles_size, sizeof(profile_file_t),
		cmp_profile_file);

	// Open storage files
	open_storage_files(conf);

	MSG_DEBUG(msg_module, "List of profiles successfully updated.");
	return 0;
}

int reload_profiles(struct lnfstore_conf *conf, void **channels)
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

	return 0;
}


void store_to_profiles(struct lnfstore_conf *conf, void **channels)
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
		int index = (result - conf->profiles_ptr);
		if (bitset_get(conf->bitset, index) == true) {
			// Already written to the file
			continue;
		}

		// Store the record only if it is normal profile
		if (profile_get_type(result->address) == PT_NORMAL) {
			store_to_file(result->file, conf->rec_ptr);
		}

		bitset_set(conf->bitset, index, true);
	}
}


void store_record(const struct metadata *mdata, struct lnfstore_conf *conf)
{
	if (conf->params->profiles && !mdata->channels) {
		/*
		 * Record won't be stored, it does not belong to any channel and
		 * profiling is activated
		 */
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
		new_window(now, conf);
	}

	// Store record
	if (conf->params->profiles) {
		// With profiler
		store_to_profiles(conf, mdata->channels);
	} else {
		// Without profiler
		store_to_file(conf->file_ptr, conf->rec_ptr);
	}
}
