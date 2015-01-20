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

#include <libxml2/libxml/xpath.h>

#include "profiler.h"
#include "Organization.h"

#include <iostream>

/* Identifier for verbose macros */
static const char *msg_module = "profiler";

/* shortcut */
using orgVec = std::vector<Organization *>;

/**
 * \struct profiler_conf
 * 
 * Plugin configuration with list of organizations
 */
struct profiler_conf {
	orgVec organizations;	/**< list of organizations */
	void *ip_config;		/**< intermediate process config */
};

/**
 * \brief Process startup configuration
 * 
 * \param[in] conf plugin configuration structure
 * \param[in] params configuration xml data
 */
void process_startup_xml(profiler_conf *conf, char *params) 
{	
	xmlChar *aux_char;
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
	
	/* Iterate throught all organizations */
	for (xmlNode *node = root->children; node; node = node->next) {
		if (node->type != XML_ELEMENT_NODE) {
			continue;
		}
		
		/* Get organization's ID */
		aux_char = xmlGetProp(node, (const xmlChar *) "id");
		if (!aux_char) {
			MSG_WARNING(msg_module, "Missing organization's ID, skipping");
			continue;
		}
		
		/* Process organization's configuration */
		Organization *org = new Organization(atoi((char *) aux_char));
		xmlFree(aux_char);
		
		/* Go throught rules and profiles */
		for (xmlNode *subNode = node->children; subNode; subNode = subNode->next) {
			if (subNode->type != XML_ELEMENT_NODE) {
				continue;
			}
			
			if (!xmlStrcmp(subNode->name, (const xmlChar *) "rule")) {
				org->addRule(doc, subNode);
			} else if (!xmlStrcmp(node->name, (const xmlChar *) "profile")) {
				org->addProfile(&pdata, subNode);
			}
		}
		
		/* Store organization into the list */
		conf->organizations.push_back(org);
	}
	
	
	/* Free resources */
	xmlFree(root);
	xmlFreeDoc(doc);
	
	if (pdata.context) {
		xmlXPathFreeContext(pdata.context);
	}
	if (pdata.doc) {
		xmlFreeDoc(pdata.doc);
	}
}

/**
 * \brief Modify metadata
 * 
 * Add informations about organization, matching rule and 
 * profiles into the metadata structure.
 * 
 * \param[in] mdata Metadata structure
 * \param[in] org	Organization
 * \param[in] rule	Matching rule
 * \param[in] profiles Matching profiles
 * \return 0 on success
 */
int modifyMetadata(struct metadata *mdata, Organization *org, Rule *rule, profileVec &profiles)
{
	return 0;
}

void print_orgs(struct profiler_conf *conf)
{
	for (auto org: conf->organizations) {
		org->print();
		std::cout << "-------------------";
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
		profiler_conf *conf = new struct profiler_conf;
		
		/* Process params */
		process_startup_xml(conf, params);
		
		/* Save configuration */
		conf->ip_config = ip_config;
		*config = conf;
		
		print_orgs(conf);
	} catch (std::exception &e) {
		*config = NULL;
		MSG_ERROR(msg_module, "%s", e.what());
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
	profiler_conf *conf = static_cast<profiler_conf *>(config);
	struct ipfix_message *msg = static_cast<struct ipfix_message *>(message);
	struct metadata *mdata;
	
	Rule *rule;
	profileVec profiles;
	
	/* 
	 * Go through all data records and all organizations to find 
	 * matching rules and profiles
	 */
	for (int i = 0; i < msg->data_records_count; ++i) {
		for (auto org: conf->organizations) {
			mdata = &(msg->metadata[i]);
			
			/* Find matching rule */
			rule = org->matchingRule(msg, &(mdata->record));
			
			if (!rule) {
				/* No rule was found */
				continue;
			}
			
			/* Get matching profiles */
			profiles = org->matchingProfiles(msg, &(mdata->record));
			
			/* Update metadata */
			modifyMetadata(mdata, org, rule, profiles);
		}
	}
	
	/* Pass message to the next plugin/Output Manager */
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
	profiler_conf *conf = static_cast<profiler_conf*>(config);
	
	/* Remove all organizations from list */
	for (auto org: conf->organizations) {
		delete org;
	}
	
	/* Destroy configuration */
	delete conf;
	return 0;
}

