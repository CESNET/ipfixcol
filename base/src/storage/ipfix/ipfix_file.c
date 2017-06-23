/**
 * \file storage/ipfix/ipfix_file.c
 * \author Michal Srb <michal.srb@cesnet.cz>
 * \author Lukas Hutak <xhutak01@stud.fit.vutbr.cz>
 * \brief Storage plugin for IPFIX file format (source file).
 */
/* Copyright (C) 2015-2017 CESNET, z.s.p.o.
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

/**
 * \defgroup ipfixFileFormat Storage plugin for IPFIX file format
 * \ingroup storagePlugins
 *
 * This is implementation of the storage plugin API for IPFIX file format.
 *
 * @{
 */

#include <unistd.h>
#include <sys/types.h>
#include <stdint.h>
#include <stdlib.h>

#include "configuration.h"
#include "ipfix_file.h"

#include <time.h>
#include <ipfixcol.h>

/* API version constant */
IPFIXCOL_API_VERSION;

const char *msg_module = "ipfix storage";

///**
// * \brief Open/create output file
// *
// * \param[in] conf  output plugin config structure
// * \return  0 on success, negative value otherwise.
// */
//static int prepare_output_file(struct ipfix_config *config)
//{
//	int fd;
//
//	/* file counter */
//	config->fcounter += 1;
//	/* byte counter */
//	config->bcounter = 0;
//
//
//	fd = open(config->file, O_WRONLY | O_CREAT | O_TRUNC,
//	          S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
//	if (fd == -1) {
//		config->fcounter -= 1;
//		MSG_ERROR(msg_module, "Unable to open output file");
//		return -1;
//	}
//
//	config->fd = fd;
//
//	return 0;
//}
//
///**
// * \brief Close output file
// *
// * \param[in] conf  output plugin config structure
// * \return  0 on success, negative value otherwise
// */
//static int close_output_file(struct ipfix_config *config)
//{
//	int ret;
//
//	ret = close(config->fd);
//	if (ret == -1) {
//		MSG_ERROR(msg_module, "Error when closing output file");
//		return -1;
//	}
//
//	config->fd = -1;
//
//	return 0;
//}


/*
 * Storage Plugin API implementation
 */

/**
 * \brief Storage plugin initialization.
 *
 * Initialize IPFIX storage plugin. This function allocates, fills and
 * returns a config structure.
 *
 * \param[in]  params Parameters for this storage plugin
 * \param[out] config The plugin specific configuration structure
 * \return 0 on success, negative value otherwise
 */
int
storage_init(char *params, void **config) {
	// Process XML configuration
	struct conf_params *parsed_params = configuration_parse(params);
	if (!parsed_params) {
		MSG_ERROR(msg_module, "Failed to parse the plugin configuration.");
		return 1;
	}

	// Create main plugin structure
	struct conf_plugin *conf = calloc(1, sizeof(*conf));
	if (!conf) {
		MSG_ERROR(msg_module, "Unable to allocate memory (%s:%d)",
			__FILE__, __LINE__);
		configuration_free(parsed_params);
		return 1;
	}

	// Create a storage manager
	files_t *storage = files_create((char *) parsed_params->output.pattern);
	if (!storage) {
		// Failed
		configuration_free(parsed_params);
		free(conf);
		return 1;
	}

	// Prepare a time window
	const uint32_t window_size = parsed_params->window.size;
	time_t now = time(NULL);

	if (parsed_params->window.align && window_size != 0) {
		// Alignment
		now /= window_size;
		now *= window_size;
	}

	conf->params = parsed_params;
	conf->window_start = now;
	conf->storage = storage;

	// Try to create a new window
	if (files_new_window(storage, now)) {
		// Failed to open file, generate filename, etc. -> skip (ignore)
		MSG_ERROR(msg_module, "Failed to create a new output file for a new "
			"time window. Flow records will be lost.", NULL);
	}

	// Success
	*config = conf;
	MSG_DEBUG(msg_module, "Initialized...", NULL);
	return 0;
}

/**
 * \brief Check duration of the current time window and eventually create a one
 *
 * Compare the start of the current window and the system time. If the window
 * has exceed configured size, create a new one.
 *
 * \param[in,out] conf Plugin configuration
 * \return When the duration of the current window is within the window size,
 *   the function returns 0. When the duration exceeded the size, the function
 *   will create a new window and it will return a positive value. When the
 *   function fails to create the new window, it will return a negative value.
 */
static int
check_window(struct conf_plugin *conf)
{
	const uint32_t window_size = conf->params->window.size;
	if (window_size == 0) {
		// Never change the window
		return 0;
	}

	time_t now = time(NULL);
	if (difftime(now, conf->window_start) < window_size) {
		// Still within the same window
		return 0;
	}

	if (conf->params->window.align) {
		// Align the new window
		now /= window_size;
		now *= window_size;
	}

	// Open the new file
	conf->window_start = now;
	if (files_new_window(conf->storage, now)) {
		MSG_ERROR(msg_module, "Failed to create a new output file for a new "
			"time window. Flow records will be lost.", NULL);
		return -1;
	}

	return 1;
}

/**
 * \brief Store received IPFIX message into a file.
 *
 * Store one IPFIX message into a output file.
 *
 * \param[in] config the plugin specific configuration structure
 * \param[in] ipfix_msg IPFIX message
 * \param[in] template_mgr Template manager
 * \return 0 on success, negative value otherwise
 */
int
store_packet(void *config, const struct ipfix_message *ipfix_msg,
	const struct ipfix_template_mgr *template_mgr)
{
	(void) template_mgr;
	struct conf_plugin *conf = (struct conf_plugin *) config;

	/*
	 * Decide whether close the current file and create new one
	 * Note: We don't care about the return value. The file manager MUST
	 *   process potential templates in this packet therefore we pass the
	 *   packet to the manager even if we know that the output file is not
	 *   ready.
	 */
	check_window(conf);
	files_add_packet(conf->storage, ipfix_msg);
	return 0;
}

/**
 * \brief Store everything we have immediately and close output file.
 * *
 * \param[in] config the plugin specific configuration structure
 * \return 0 on success, negative value otherwise
 */
int store_now(const void *config)
{
	// Do nothing
	(void) config;
	return 0;
}

/**
 * \brief Remove storage plugin.
 *
 * This function is called when we don't want to use this storage plugin
 * anymore. All it does is that it cleans up after the storage plugin.
 *
 * \param[in] config the plugin specific configuration structure
 * \return 0 on success, negative value otherwise
 */
int storage_close(void **config)
{
	struct conf_plugin *conf = (struct conf_plugin *) *config;

	files_destroy(conf->storage);
	configuration_free(conf->params);
	free(conf);

	*config = NULL;
	return 0;
}

/**@}*/

