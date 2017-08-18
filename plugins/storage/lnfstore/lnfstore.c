/**
 * \file lnfstore.c
 * \author Imrich Stoffa <xstoff02@stud.fit.vutbr.cz>
 * \author Lukas Hutak <xhutak01@stud.fit.vutbr.cz>
 * \author Pavel Krobot <Pavel.Krobot@cesnet.cz>
 * \brief lnfstore plugin interface (source file)
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

#include "lnfstore.h"
#include "storage_common.h"
#include <ipfixcol.h>

// API version constant
IPFIXCOL_API_VERSION;

// Module identification
const char* msg_module = "lnfstore";

// Storage plugin initialization function.
int
storage_init (char *params, void **config) {
	// Process XML configuration
	struct conf_params *parsed_params = configuration_parse(params);
	if (!parsed_params) {
		MSG_ERROR(msg_module, "Failed to parse the plugin configuration.",
			NULL);
		return 1;
	}

	// Create main plugin structure
	struct conf_lnfstore *conf = calloc(1, sizeof(*conf));
	if (!conf) {
		MSG_ERROR(msg_module, "Unable to allocate memory (%s:%d)",
			__FILE__, __LINE__);
		configuration_free(parsed_params);
		return 1;
	}

	conf->params = parsed_params;

	// Init a LNF record for conversion from IPFIX
	if (lnf_rec_init(&conf->record.rec_ptr) != LNF_OK) {
		MSG_ERROR(msg_module, "Failed to initialize an internal structure "
			"for conversion of records", NULL);
		configuration_free(parsed_params);
		free(conf);
		return 1;
	}

	conf->record.translator = translator_init();
	if (!conf->record.translator) {
		MSG_ERROR(msg_module, "Failed to initialize a record translator.",
			NULL);
		lnf_rec_free(conf->record.rec_ptr);
		configuration_free(parsed_params);
		free(conf);
		return 1;
	}

	// Init basic/profile file storage
	if (conf->params->profiles.en) {
		conf->storage.profiles = stg_profiles_create(parsed_params);
	} else {
		conf->storage.basic = stg_basic_create(parsed_params);
	}

	if (!conf->storage.basic && !conf->storage.profiles) {
		MSG_ERROR(msg_module, "Failed to initialize an internal structure "
			"for file storage(s).", NULL);
		translator_destroy(conf->record.translator);
		lnf_rec_free(conf->record.rec_ptr);
		configuration_free(parsed_params);
		free(conf);
	}

	// Save the configuration
	*config = conf;
	MSG_DEBUG(msg_module, "Initialized...", NULL);
	return 0;
}

// Pass IPFIX data with supplemental structures into the storage plugin.
int
store_packet (void *config, const struct ipfix_message *ipfix_msg,
	const struct ipfix_template_mgr *template_mgr)
{
	(void) template_mgr;
	struct conf_lnfstore *conf = (struct conf_lnfstore*) config;

	// Decide whether close files and create new time window
	time_t now = time(NULL);
	if (difftime(now, conf->window_start) > conf->params->window.size) {
		time_t new_time = now;

		if (conf->params->window.align) {
			// We expect that new_time is integer
			new_time /= conf->params->window.size;
			new_time *= conf->params->window.size;
		}

		conf->window_start = new_time;

		// Update storage files
		if (conf->params->profiles.en) {
			stg_profiles_new_window(conf->storage.profiles, new_time);
		} else {
			stg_basic_new_window(conf->storage.basic, new_time);
		}
	}

	for (unsigned int i = 0; i < ipfix_msg->data_records_count; i++) {
		// Get a pointer to the next record
		const struct metadata *mdata = &(ipfix_msg->metadata[i]);

		if (conf->params->profiles.en && !mdata->channels) {
			/*
			 * Record won't be stored, it does not belong to any channel and
		 	 * profiling is activated
		 	 */
			continue;
		}

		// Fill record
		lnf_rec_t *rec = conf->record.rec_ptr;
		if (translator_translate(conf->record.translator, mdata, rec) <= 0) {
			// Nothing to store
			continue;
		}

		if (conf->params->profiles.en) {
			// Profile mode
			stg_profiles_store(conf->storage.profiles, mdata, rec);
		} else {
			// Basic mode
			stg_basic_store(conf->storage.basic, rec);
		}
	}

	return 0;
}

// Announce willing to store currently processing data
int
store_now (const void *config)
{
	(void) config;
	return 0;
}

// Storage plugin "destructor"
int
storage_close (void **config)
{
	MSG_DEBUG(msg_module, "Closing...");
	struct conf_lnfstore *conf = (struct conf_lnfstore *) *config;

	// Destroy mode resources
	if (conf->params->profiles.en) {
		stg_profiles_destroy(conf->storage.profiles);
	} else {
		stg_basic_destroy(conf->storage.basic);
	}

	// Destroy a translator and a record
	translator_destroy(conf->record.translator);
	lnf_rec_free(conf->record.rec_ptr);

	// Destroy parsed XML configuration
	configuration_free(conf->params);

	// Destroy instance structure
	free(conf);
	*config = NULL;
	return 0;
}
