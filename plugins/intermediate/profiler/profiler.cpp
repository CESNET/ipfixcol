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
#include <libxml2/libxml/xpath.h>
#include "parser.h"
#include "scanner.h"
}

#include "profiler.h"

#include "Profile.h"
#include "Channel.h"

#include <iostream>

#define profile_id(_profile_) (_profile_) ? (_profile_)->getName().c_str() : "live"
#define throw_empty throw std::runtime_error(std::string(""));

/* Identifier for verbose macros */
static const char *msg_module = "profiler";

/**
 * \struct plugin_conf
 * 
 * Plugin configuration with list of organizations
 */
struct plugin_conf {
	void *ip_config;	/**< intermediate process config */
	Profile *live;		/**< Live profile */
};

/**
 * \brief Parse filter string
 * 
 * \param[in] pdata parser data
 * \return 0 on success
 */
int parse_filter(filter_parser_data* pdata)
{
	int ret = 0;
	
	/* Prepare scanner */
	yylex_init(&(pdata->scanner));
	YY_BUFFER_STATE bp = yy_scan_string(pdata->filter, pdata->scanner);
	yy_switch_to_buffer(bp, pdata->scanner);
    
	/* Parse filter */
	ret = yyparse(pdata);
	
	/* Clear scanner */
	yy_flush_buffer(bp, pdata->scanner);
	yy_delete_buffer(bp, pdata->scanner);
	yylex_destroy(pdata->scanner);
	
	return ret;
}

/**
 * \brief Process channel's configuration and create new Channel object
 * 
 * \param[in] profile Channel's profile
 * \param[in] root channel xml configuration root
 * \return new Channel object
 */
Channel *process_channel(Profile *profile, xmlNode *root, struct filter_parser_data *pdata)
{
	/* Get channel ID */
	xmlChar *aux_char;
	aux_char = xmlGetProp(root, (const xmlChar *) "name");
	
	if (!aux_char) {
		MSG_ERROR(msg_module, "Profile %s: missing channel name", profile_id(profile));
		throw_empty;
	}
	
	/* Create new channel */
	Channel *channel = new Channel((char *) aux_char);
	xmlFree(aux_char);
	channel->setProfile(profile);
	
	/* Allocate space for filter */
	filter_profile *fp = (filter_profile *) calloc(1, sizeof(filter_profile));
	if (!fp) {
		MSG_ERROR(msg_module, "Profile %s: channel %s: unable to allocate memory (%s:%d)", profile_id(profile), channel->getName().c_str(), __FILE__, __LINE__);
		delete channel;
		throw_empty;
	}
	
	/* Initialize parser data */
	pdata->profile = fp;
	pdata->filter = NULL;
	
	/* Iterate through elements */
	for (xmlNode *node = root->children; node; node = node->next) {
		
		if (!xmlStrcmp(node->name, (const xmlChar *) "filter")) {
			/* Parse filter */
			pdata->filter = (char *) xmlNodeGetContent(node->children);
			if (parse_filter(pdata) != 0) {
				MSG_ERROR(msg_module, "Profile %s: channel %s: error while parsing filter", profile_id(profile), channel->getName().c_str());
				filter_free_profile(fp);
				free(pdata->filter);
				delete channel;
				throw_empty;
			}
			
			free(pdata->filter);
			
			/* Set filter to channel */
			channel->setFilter(pdata->profile);
			
		} else if (!xmlStrcmp(node->name, (const xmlChar *) "sources")) {
			/* Channel sources */
			aux_char = xmlNodeGetContent(node->children);
			channel->setSources((char *) aux_char);
			free(aux_char);
			
		}
	}
	
	return channel;
}

/**
 * \brief Process profile's configuration and create new Profile object
 * 
 * \param[in] parent Profile's parent
 * \param[in] root profile xml configuration root
 * \return new Profile object
 */
Profile *process_profile(Profile *parent, xmlNode *root, struct filter_parser_data *pdata)
{
	/* Get profile ID */
	xmlChar *aux_char;
	aux_char = xmlGetProp(root, (const xmlChar *) "name");
	
	if (!aux_char) {
		throw std::runtime_error("Missing profile name");
	}
	
	/* Create new profile */
	Profile *profile = new Profile((char *) aux_char);
	xmlFree(aux_char);
	
	profile->setParent(parent);
	
	/* Iterate through elements */
	for (xmlNode *node = root->children; node; node = node->next) {
		
		if (!xmlStrcmp(node->name, (const xmlChar *) "profile")) {
			/* add sub-profile */
			Profile *child = process_profile(profile, node, pdata);
			profile->addProfile(child);
			
		} else if (!xmlStrcmp(node->name, (const xmlChar *) "channel")) {
			/* add channel */
			Channel *channel = process_channel(profile, node, pdata);
			profile->addChannel(channel);
		}
	}
	
	return profile;
}

/**
 * \brief Free parser data (context and document)
 * 
 * \param[in] pdata parser data
 */
void free_parser_data(struct filter_parser_data *pdata)
{
	if (pdata->context) {
		xmlXPathFreeContext(pdata->context);
	}
	if (pdata->doc) {
		xmlFreeDoc(pdata->doc);
	}
}

/**
 * \brief Process startup configuration
 * 
 * \param[in] conf plugin configuration structure
 * \param[in] params configuration xml data
 */
void process_startup_xml(plugin_conf *conf, char *params) 
{	
	struct filter_parser_data pdata;
	
	/* Load XML configuration */
	xmlDoc *doc = xmlParseDoc(BAD_CAST params);
	if (!doc) {
		throw std::invalid_argument("Cannot parse config xml");
	}
	
	xmlNode *root = xmlDocGetRootElement(doc);
	if (!root) {
		throw std::invalid_argument("Cannot get document root element!");
	}
	
	/* Initialize IPFIX elements */
	filter_init_elements(&pdata);
	conf->live = NULL;
	
	try {
		/* Iterate throught all profiles */
		for (xmlNode *node = root->children; node; node = node->next) {
			if (node->type != XML_ELEMENT_NODE) {
				continue;
			}

			if (!xmlStrcmp(node->name, (const xmlChar *) "profile")) {
				conf->live = process_profile(NULL, node, &pdata);
			}
		}
	} catch (std::exception &e) {
		xmlFreeDoc(doc);
		free_parser_data(&pdata);
		throw;
	}
	
	/* Free resources */
	xmlFreeDoc(doc);
	free_parser_data(&pdata);
	
	if (!conf->live) {
		throw std::invalid_argument("No profile found in plugin configuration");
	}
	
}

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
		
		/* Process params */
		process_startup_xml(conf, params);
		
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
		
	struct metadata *mdata;
	
	/* Go through all data records */
	for (uint16_t i = 0; i < msg->data_records_count; ++i) {
		mdata = &(msg->metadata[i]);
		
		/* Get matching profiles and channels */
		std::vector<couple_id_t> profiles;
		conf->live->match(msg, mdata, profiles);
		
		/* Convert vector -> C array */
		if (profiles.empty()) {
			mdata->profiles = NULL;
			continue;
		}
		
		/* Add terminating zero */
		profiles.push_back(0);
		
		/* Allocate C space */
		mdata->profiles = (couple_id_t *) calloc(profiles.size(), sizeof(couple_id_t));
		if (!mdata->profiles) {
			MSG_ERROR(msg_module, "Unable to allocate memory (%s:%d)", __FILE__, __LINE__);
			continue;
		}
		
		/* Copy data */
		for (long unsigned int x = 0; x < profiles.size(); ++x) {
			mdata->profiles[x] = profiles[x];
		}
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
	delete conf->live;
	delete conf;
	return 0;
}
