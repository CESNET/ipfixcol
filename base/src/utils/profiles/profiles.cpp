/**
 * \file profile_tree.cpp
 * \author Michal Kozubik <kozubik@cesnet.cz>
 * \brief Code for loading profile tree from XML file
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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ipfixcol/profiles.h>
}

#include "Profile.h"
#include "Channel.h"

#include "profiles_internal.h"
#include <iostream>

#define profile_id(_profile_) (_profile_) ? (_profile_)->getName().c_str() : "live"
#define throw_empty throw std::runtime_error(std::string(""));

static const char *msg_module = "profile_tree";

extern "C" int yyparse (struct filter_parser_data *data);

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
	
	/* Initialize parser data */
	pdata->filter = NULL;
	
	/* Iterate through elements */
	for (xmlNode *node = root->children; node; node = node->next) {
		
		if (!xmlStrcmp(node->name, (const xmlChar *) "filter")) {
			/* Allocate space for filter */
			filter_profile *fp = (filter_profile *) calloc(1, sizeof(filter_profile));
			if (!fp) {
				MSG_ERROR(msg_module, "Profile %s: channel %s: unable to allocate memory (%s:%d)", profile_id(profile), channel->getName().c_str(), __FILE__, __LINE__);
				delete channel;
				throw_empty;
			}

			pdata->profile = fp;

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
 * \brief Process profile tree xml configuration
 * 
 * \param[in] file configuration xml file
 * \return Pointer to root profile
 */
Profile *process_profile_xml(const char *filename)
{	
	struct filter_parser_data pdata;

	/* Open file */
	int fd = open(filename, O_RDONLY);
	if (fd == -1) {
		MSG_ERROR(msg_module, "Unable to open configuration file %s (%s)", filename, strerror(errno));
		return NULL;
	}

	/* Load XML configuration */
	xmlDoc *doc = xmlReadFd(fd, NULL, NULL, XML_PARSE_NOERROR | XML_PARSE_NOWARNING | XML_PARSE_NOBLANKS);
	if (!doc) {
		close(fd);
		MSG_ERROR(msg_module, "Unable to parse configuration file %s", filename);
		return NULL;
	}
	
	xmlNode *root = xmlDocGetRootElement(doc);
	if (!root) {
		xmlFreeDoc(doc);
		close(fd);
		MSG_ERROR(msg_module, "Unable to get root element from file %s", filename);
		return NULL;
	}
	
	/* Initialize IPFIX elements */
	filter_init_elements(&pdata);
	
	Profile *rootProfile{NULL};

	try {
		/* Iterate throught all profiles */
		for (xmlNode *node = root; node; node = node->next) {
			if (node->type != XML_ELEMENT_NODE) {
				continue;
			}

			if (!xmlStrcmp(node->name, (const xmlChar *) "profile")) {
				rootProfile = process_profile(NULL, node, &pdata);
			}
		}
	} catch (std::exception &e) {
		xmlFreeDoc(doc);
		close(fd);
		free_parser_data(&pdata);
		MSG_ERROR(msg_module, "%s", e.what());
		return NULL;
	}

	xmlFreeDoc(doc);
	free_parser_data(&pdata);

	if (!rootProfile) {
		MSG_ERROR(msg_module, "No profile found in profile tree configuration");
		return NULL;
	}	

	rootProfile->updatePathName();

	return rootProfile;
}


/* API FUNCTIONS */

/**
 * Process XML configuration
 */
void *profiles_process_xml(const char *path)
{
	return (void*) process_profile_xml(path);
}

/* ==== PROFILE ==== */
/**
 * Get profile name
 */
const char *profile_get_name(void *profile)
{
	return ((Profile *) profile)->getName().c_str();
}

/**
 * Get profile path
 */
const char *profile_get_path(void *profile)
{
	return ((Profile *) profile)->getPathName().c_str();
}

/**
 * Get number of children
 */
uint16_t profile_get_children(void *profile)
{
	return ((Profile *) profile)->getChildren().size();
}

/**
 * Get number of channels
 */
uint16_t profile_get_channels(void *profile)
{
	return ((Profile *) profile)->getChannels().size();
}

/**
 * Get parent profile
 */
void *profile_get_parent(void *profile)
{
	return (void *) ((Profile *) profile)->getParent();
}

/**
 * Get child on given index
 */
void *profile_get_child(void *profile, uint16_t index)
{
	Profile *p = (Profile *) profile;
	return p->getChildren().size() > index ? p->getChildren()[index] : NULL;
}

/**
 * Get channel on given index
 */
void *profile_get_channel(void *profile, uint16_t index)
{
	Profile *p = (Profile *) profile;
	return p->getChannels().size() > index ? p->getChannels()[index] : NULL;
}

/**
 * Match profile with data record
 */
void **profile_match_data(void *profile, struct ipfix_message *msg, struct metadata *mdata)
{
	Profile *p = (Profile *) profile;

	/* Find matching channels */
	std::vector<Channel *> channels{};
	p->match(msg, mdata, channels);

	/* No matching channels found */
	if (channels.empty()) {
		return NULL;
	}

	/* Add terminating NULL */
	channels.push_back(NULL);

	/* Copy them to the C array */
	void **c_channels = (void **) calloc(sizeof(void *), channels.size());

	if (!c_channels) {
		MSG_ERROR(msg_module, "Unable to allocate memory (%s:%d)", __FILE__, __LINE__);
		return NULL;
	}

	for (uint16_t i = 0; i < channels.size(); ++i) {
		c_channels[i] = (void *) channels[i];
	}

	/* Done */
	return c_channels;
}

/**
 * Free profile with all it's channels and subprofiles
 */
void profiles_free(void *profile)
{
	delete ((Profile *) profile);
}

/* ==== CHANNEL ==== */
/**
 * Get channel name
 */
const char *channel_get_name(void *channel)
{
	return ((Channel *) channel)->getName().c_str();
}

/**
 * Get channel path
 */
const char *channel_get_path(void *channel)
{
	return ((Channel *) channel)->getPathName().c_str();
}

/**
 * Get channel profile
 */
void *channel_get_profile(void *channel)
{
	return ((Channel *) channel)->getProfile();
}

uint16_t channel_get_listeners(void *channel)
{
	return ((Channel *) channel)->getListeners().size();
}

/**
 * Get number of data sources
 */
uint16_t channel_get_sources(void *channel)
{
	return ((Channel *) channel)->getSources().size();
}
