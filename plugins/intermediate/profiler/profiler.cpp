/**
 * \file profiler.cpp
 * \author Michal Kozubik <kozubik@cesnet.cz>
 * \brief intermediate plugin for profiling data
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

extern "C" {
#include <ipfixcol.h>
#include <ipfixcol/profiles.h>
}

#include <stdexcept>
#include <iostream>

/* Identifier for verbose macros */
static const char *msg_module = "profiler";

/**
 * \struct plugin_conf
 * 
 * Plugin configuration with list of organizations
 */
struct plugin_conf {
	void *ip_config;	/**< intermediate process config */
};

/**
 * \brief Plugin initialization
 * 
 * \param[in] params xml configuration
 * \param[in] ip_config	intermediate process config
 * \param[in] ip_id	intermediate process ID for template manager
 * \param[in] template_mgr template manager
 * \param[out] config config storage
 * \return 0 on success
 */
int intermediate_init(char* params, void* ip_config, uint32_t ip_id, ipfix_template_mgr* template_mgr, void** config)
{	
	/* Suppress compiler warning */
	(void) ip_id; (void) template_mgr;
	
	if (!params) {
		MSG_ERROR(msg_module, "Missing plugin configuration");
		return 1;
	}
	
	try {
		/* Create configuration */
		plugin_conf *conf = new struct plugin_conf;

		/* Save configuration */
		conf->ip_config = ip_config;
		*config = conf;
		
	} catch (std::exception &e) {
		*config = NULL;
		if (!std::string(e.what()).empty()) {
			MSG_ERROR(msg_module, "%s", e.what());
		}
		
		return 1;
	}
	
	MSG_DEBUG(msg_module, "initialized");
	return 0;
}

/**
 * \brief Process IPFIX message
 * 
 * \param[in] config plugin configuration
 * \param[in] message IPFIX message
 * \return 0 on success
 */
int intermediate_process_message(void* config, void* message)
{
	plugin_conf *conf = reinterpret_cast<plugin_conf *>(config);
	struct ipfix_message *msg = reinterpret_cast<struct ipfix_message *>(message);
	
	if (msg->data_records_count == 0) {
		pass_message(conf->ip_config, msg);
		return 0;
	}

	if (!msg->live_profile) {
		MSG_WARNING(msg_module, "Missing profiles configuration");
		pass_message(conf->ip_config, msg);
		return 0;
	}

	/* Match channels for each data record */
	for (uint16_t i = 0; i < msg->data_records_count; ++i) {
		msg->metadata[i].channels = profile_match_data(msg->live_profile, msg, &(msg->metadata[i]));
	}

	pass_message(conf->ip_config, msg);
	return 0;
}

/**
 * \brief Close intermediate plugin
 * 
 * \param[in] config plugin configuration
 * \return 0 on success
 */
int intermediate_close(void *config)
{
	MSG_DEBUG(msg_module, "CLOSING");
	plugin_conf *conf = static_cast<plugin_conf*>(config);
	
	/* Destroy profiles and configuration */
	delete conf;

	return 0;
}
