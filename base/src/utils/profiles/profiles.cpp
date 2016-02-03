/**
 * \file profiles.cpp
 * \author Michal Kozubik <kozubik@cesnet.cz>
 * \author Lukas Hutak <xhutak01@stud.fit.vutbr.cz>
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
#include "parser.h"
#include "scanner.h"
#include <ipfixcol/profiles.h>
}

#include <libxml2/libxml/xpath.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "Profile.h"
#include "Channel.h"

#include "profiles_internal.h"
#include <iostream>
#include <set>
#include <queue>

/* Ignore BIG_LINES libxml2 option when it is not supported */
#ifndef XML_PARSE_BIG_LINES
#define XML_PARSE_BIG_LINES 0
#endif


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
 * \brief Get the uniq node's child node with given name.
 * \param[in] root Main node
 * \param[in] name Name of the child
 * \param[out] result Pointer to the searched child
 * \warning \p result is filled only when return value (number of matches) == 1
 * \return Number of matches
 */
int xml_find_uniq_element(xmlNodePtr root, const char *name, xmlNodePtr *result)
{
	xmlNodePtr uniq_node = NULL;
	int count = 0;

	for (xmlNodePtr node = root->children; node; node = node->next) {
		if (xmlStrcmp(node->name, BAD_CAST name)) {
			continue;
		}

		uniq_node = node;
		count++;
	}

	*result = (count == 1) ? uniq_node : NULL;
	return count;
}

/**
 * \brief Find and parse flow filter in the channel specification
 * \param[in] root Channel element
 * \param[in,out] pdata Filter parse aux. structure
 * \return Pointer to new filter or NULL
 */
static filter_profile *channel_parse_filter(xmlNodePtr root,
	struct filter_parser_data *pdata)
{
	filter_profile *filter = NULL;
	xmlNodePtr filter_node = NULL;

	// Get 'filter' node
	int cnt;
	cnt = xml_find_uniq_element(root, "filter", &filter_node);
	if (cnt < 1 || filter_node == NULL || filter_node->children == NULL) {
		// Missing or empty "filter" element is also valid
		MSG_DEBUG(msg_module, "'filter' is not set in the element on line %ld",
			xmlGetLineNo(root));
		return NULL;
	}

	if (cnt > 1) {
		MSG_ERROR(msg_module, "Multiple definitions of 'filter' in "
			"the node on line %ld", xmlGetLineNo(root));
		throw_empty;
	}

	// Check validity of the node
	if (!xmlNodeIsText(filter_node->children)) {
		MSG_ERROR(msg_module, "Filter node is not a text node (line: %ld)",
			xmlGetLineNo(filter_node));
		throw_empty;
	}

	xmlChar *aux_char = xmlNodeGetContent(filter_node);
	if (!aux_char) {
		MSG_ERROR(msg_module, "Failed to get the content of 'filter' node "
			"(line: %ld)", xmlGetLineNo(filter_node));
		throw_empty;
	}

	// Create new filter
	filter = (filter_profile *) calloc(1, sizeof(filter_profile));
	if (!filter) {
		MSG_ERROR(msg_module, "Unable to allocate memory");
		xmlFree(aux_char);
		throw_empty;
	}

	pdata->profile = filter;
	pdata->filter = (char *) aux_char;

	if (parse_filter(pdata) != 0 || pdata->profile == NULL) {
		MSG_ERROR(msg_module, "Error while parsing filter on line %ld",
			xmlGetLineNo(filter_node));
		filter_free_profile(filter);
		xmlFree(pdata->filter);
		throw_empty;
	}

	xmlFree(aux_char);
	return pdata->profile;
}

/**
 * \brief Find and parse the list of source channels
 * \param[in] root Channel element
 * \return String with names of channels (comma separated)
 */
static std::string channel_parse_source_list(xmlNodePtr root)
{
	xmlNodePtr sources_node = NULL;
	std::string list;

	// Find the node with the list of sources
	int cnt;
	cnt = xml_find_uniq_element(root, "sourceList", &sources_node);
	if (cnt != 1 || sources_node == NULL) {
		MSG_ERROR(msg_module, "Invalid definition of the element 'sourceList' "
			"in the channel (line %ld). Expected single element.",
			xmlGetLineNo(root));
		throw_empty;
	}

	// Get names of channels
	for (xmlNode *node = sources_node->children; node; node = node->next) {
		if (node->type != XML_ELEMENT_NODE) {
			// Skip comments...
			continue;
		}

		if (xmlStrcmp(node->name, BAD_CAST "source")) {
			MSG_ERROR(msg_module, "Unexpected element on the line %ld",
				xmlGetLineNo(node));
			throw_empty;
		}

		if (!xmlNodeIsText(node->children)) {
			MSG_ERROR(msg_module, "The 'source' node is not valid text node "
				"(line: %ld)", xmlGetLineNo(node));
			throw_empty;
		}

		xmlChar *aux_char = xmlNodeGetContent(node->children);
		if (aux_char == NULL) {
			continue;
		}

		if (!list.empty()) {
			list += ",";
		}

		list += (const char *) aux_char;
		xmlFree(aux_char);
	}

	return list;
}

/**
 * \brief Process channel's configuration and create new Channel object
 * 
 * \param[in] profile Channel's profile
 * \param[in] root channel xml configuration root
 * \param[in] pdata Filter parser data
 * \return new Channel object
 */
Channel *process_channel(Profile *profile, xmlNode *root,
	struct filter_parser_data *pdata)
{
	/* Get channel ID */
	xmlChar *aux_char;
	aux_char = xmlGetProp(root, (const xmlChar *) "name");
	
	if (!aux_char) {
		MSG_ERROR(msg_module, "Profile %s: missing channel name (line: %ld)",
			profile_id(profile), xmlGetLineNo(root));
		throw_empty;
	}

	/* Create new channel */
	Channel *channel = new Channel((char *) aux_char);
	xmlFree(aux_char);
	channel->setProfile(profile);

	/* Initialize parser data */
	pdata->filter = NULL;
	
	try {
		/* Find and parse filter */
		filter_profile *filter = channel_parse_filter(root, pdata);
		channel->setFilter(filter);

		/* Find and process the list of source channels */
		std::string list = channel_parse_source_list(root);
		channel->setSources(list);

	} catch (std::exception &e) {
		// Delete channel and rethrow current exception
		delete channel;
		throw;
	}

	/* Check if there is at least one source */
	if (channel->getProfile()->getParent() && channel->getSources().empty()) {
		/* Channel does not belong to root profile and source set is empty */
		MSG_ERROR(msg_module, "Profile %s: channel %s: no data source(s)",
			profile_id(profile), channel->getName().c_str());
		delete channel;
		throw_empty;
	}
	
	return channel;
}

/**
 * \brief Find and parse type of a profile
 * \param[in] root Profile element
 * \return Type of the profile
 */
static enum PROFILE_TYPE profile_parse_type(xmlNodePtr root)
{
	xmlNodePtr type_node = NULL;
	enum PROFILE_TYPE type = PT_UNDEF;

	// Find the node with a type of the profile
	int cnt;
	cnt = xml_find_uniq_element(root, "type", &type_node);
	if (cnt != 1 || type_node == NULL) {
		MSG_ERROR(msg_module, "Invalid definition of the element 'type' "
			"in the profile (line %ld). Expected single element.",
			xmlGetLineNo(root));
		throw_empty;
	}

	// Get the text content
	xmlChar *aux_char = xmlNodeGetContent(type_node->children);
	if (aux_char == NULL) {
		MSG_ERROR(msg_module, "The content of 'type' node is not valid "
			"(line %ld)", xmlGetLineNo(type_node));
		throw_empty;
	}

	if (!xmlStrcmp(aux_char, BAD_CAST "normal")) {
		type = PT_NORMAL;
	} else if (!xmlStrcmp(aux_char, BAD_CAST "shadow")) {
		type = PT_SHADOW;
	} else {
		MSG_ERROR(msg_module, "The content of 'type' node is not valid type of "
			"a profile (line %ld)", xmlGetLineNo(type_node));
		xmlFree(aux_char);
		throw_empty;
	}

	xmlFree(aux_char);
	return type;
}

/**
 * \brief Find and parse a directory of a profile
 * \param[in] root Profile element
 * \return Path of the directory
 */
static std::string profile_parse_directory(xmlNodePtr root)
{
	xmlNodePtr dir_node = NULL;

	// Find the node with a directory of the profile
	int cnt;
	cnt = xml_find_uniq_element(root, "directory", &dir_node);
	if (cnt != 1 || dir_node == NULL) {
		MSG_ERROR(msg_module, "Invalid definition of the element 'directory' "
			"in the profile (line %ld). Expected single element.",
			xmlGetLineNo(root));
		throw_empty;
	}

	// Get directory
	xmlChar *aux_char = xmlNodeGetContent(dir_node->children);
	if (aux_char == NULL) {
		MSG_ERROR(msg_module, "The content of 'directory' node is not valid "
			"(line %ld)", xmlGetLineNo(dir_node));
		throw_empty;
	}

	std::string result = (const char *) aux_char;
	xmlFree(aux_char);
	return result;
}

/**
 * \brief Find and parse a list of channels of a profile
 * \param[in,out] profile Parent profile
 * \param[in] root Profile XML node
 * \param[in,out] pdata Filter parser data
 * \return Number of added channels
 */
static int profile_parse_channels(Profile *profile, xmlNodePtr root,
	struct filter_parser_data *pdata)
{
	/* Find the list of channels */
	int cnt;
	xmlNodePtr channels_node = NULL;

	cnt = xml_find_uniq_element(root, "channelList", &channels_node);
	if (cnt != 1 || channels_node == NULL) {
		MSG_ERROR(msg_module, "Invalid definition of the element 'channelList' "
			"in the profile (line %ld). Expected single element.",
			xmlGetLineNo(root));
		throw_empty;
	}

	/* Add channels */
	int count = 0;
	for (xmlNode *node = channels_node->children; node; node = node->next) {
		if (node->type != XML_ELEMENT_NODE) {
			continue;
		}

		if (xmlStrcmp(node->name, BAD_CAST "channel")) {
			MSG_ERROR(msg_module, "Unexpected element on the line %ld.",
				xmlGetLineNo(node));
			throw_empty;
		}

		Channel *channel = process_channel(profile, node, pdata);
		profile->addChannel(channel);
		count++;
	}

	/* Check that the profile has at least one channel */
	if (profile->getChannels().empty()) {
		MSG_ERROR(msg_module, "List of channels is empty (line %ld)",
			xmlGetLineNo(channels_node));
		throw_empty;
	}

	return count;
}

// Function prototype, defined later.
Profile *process_profile(Profile *parent, xmlNode *root,
	struct filter_parser_data *pdata);

/**
 * \brief Find and parse a list of subprofiles of a profile
 * \param[in,out] profile Parent profile
 * \param[in] root Profile XML node
 * \param[in,out] pdata Filter parser data
 * \return Number of added subprofiles
 */
static int profile_parse_subprofiles(Profile *profile, xmlNodePtr root,
	struct filter_parser_data *pdata)
{
	/* Find the list of subprofiles */
	int cnt;
	xmlNodePtr subprofiles_node = NULL;

	cnt = xml_find_uniq_element(root, "subprofileList", &subprofiles_node);
	if (cnt > 1) {
		MSG_ERROR(msg_module, "Invalid definition of the element "
			"'subprofileList' in the profile (line %ld). Expected none or "
			"single element.", xmlGetLineNo(root));
		throw_empty;
	}

	if (cnt < 1) {
		// No subprofile is also valid
		return 0;
	}

	/* Add subprofiles */
	int count = 0;
	for (xmlNode *node = subprofiles_node->children; node; node = node->next) {
		if (node->type != XML_ELEMENT_NODE) {
			continue;
		}

		if (xmlStrcmp(node->name, BAD_CAST "profile")) {
			MSG_ERROR(msg_module, "Unexpected element on the line %ld.",
				xmlGetLineNo(node));
			throw_empty;
		}

		Profile *child = process_profile(profile, node, pdata);
		profile->addProfile(child);
	}

	return count;
}

/**
 * \brief Process profile's configuration and create new Profile object
 * 
 * \param[in] parent Profile's parent
 * \param[in] root Profile xml configuration root
 * \param[in] pdata Filter parser data
 * \return new Profile object
 */
Profile *process_profile(Profile *parent, xmlNode *root,
	struct filter_parser_data *pdata)
{
	/* Get type of the profile */
	enum PROFILE_TYPE type = profile_parse_type(root);

	/* Get profile ID */
	xmlChar *aux_char;
	aux_char = xmlGetProp(root, BAD_CAST "name");
	if (!aux_char) {
		MSG_ERROR(msg_module, "Subprofile of '%s' profile: missing profile "
			"name (line %ld)", profile_id(parent), xmlGetLineNo(root));
		throw_empty;
	}

	std::string name = (const char *) aux_char;
	xmlFree(aux_char);

	/* Configure new profile */
	Profile *profile = new Profile(name, type);

	try {
		profile->setParent(parent);
		profile->setDirectory(profile_parse_directory(root));
		profile_parse_channels(profile, root, pdata);
		profile_parse_subprofiles(profile, root, pdata);
 	} catch (std::exception &e) {
		// Delete new profile and rethrow current exception
		delete profile;
		throw;
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
 * \param[in] filename XML configuration file
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
	xmlDoc *doc = xmlReadFd(fd, NULL, NULL, XML_PARSE_NOERROR |
		XML_PARSE_NOWARNING | XML_PARSE_NOBLANKS | XML_PARSE_BIG_LINES);
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
		/* rootProfile must be considered as loop condition, since storage allocated
		   by process_profile will be leaked otherwise
		 */
		for (xmlNode *node = root; node && !rootProfile; node = node->next) {
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
		return NULL;
	}

	close(fd);
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
 * Get profile type
 */
PROFILE_TYPE profile_get_type(void *profile)
{
	return ((Profile *) profile)->getType();
}

/**
 * Get profile directory
 */
const char *profile_get_directory(void *profile)
{
	return ((Profile *) profile)->getDirectory().c_str();
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

	/* Fill data structure */
	struct match_data data;
	data.msg = msg;
	data.mdata = mdata;
	data.channels = NULL;
	data.channelsCounter = 0;
	data.channelsMax = 0;

	/* Find matching channels */
	p->match(&data);
	if (data.channels == NULL || data.channelsCounter == 0) {
		return NULL;
	}

	/* Add terminating NULL pointer */
	if (data.channelsCounter == data.channelsMax) {
		data.channels = (void**) realloc(data.channels, sizeof(void*) * (data.channelsCounter + 1));
		if (data.channels == NULL) {
			MSG_ERROR(msg_module, "Unable to allocate memory (%s:%d)", __FILE__, __LINE__);
			return NULL;
		}
	}

	data.channels[data.channelsCounter] = NULL;

	return data.channels;
}

/**
 * Get all profiles in the tree
 */
void **profile_get_all_profiles(void *profile)
{
	if (!profile) {
		return NULL;
	}

	// Find root profile
	void *tmp_profile = profile;
	while (tmp_profile) {
		profile = tmp_profile;
		tmp_profile = ((Profile *)profile)->getParent();
	}

	std::set<Profile *> all_profiles;
	std::queue<Profile *> next;
	next.push((Profile *) profile);

	// Get all profiles
	while (!next.empty()) {
		Profile *item = next.front();
		next.pop();

		all_profiles.insert(item);
		Profile::profilesVec sub_prof = ((Profile *) item)->getChildren();
		for (auto &i : sub_prof) {
			next.push(i);
		}
	}

	// Convert the set to an array
	Profile **all_array = (Profile **) calloc(all_profiles.size() + 1, sizeof(Profile *));
	if (!all_array) {
		MSG_ERROR(msg_module, "Unable to allocate memory (%s:%d)", __FILE__, __LINE__);
		return NULL;
	}

	int index = 0;
	for (auto &i : all_profiles) {
		all_array[index++] = i;
	}

	return (void **) all_array;
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
