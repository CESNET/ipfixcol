/**
 * \file storage/ipfix/configuration.c
 * \author Lukas Hutak <xhutak01@stud.fit.vutbr.cz>
 * \brief Configuration parser (source file)
 */
/* Copyright (C) 2017 CESNET, z.s.p.o.
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
*/

#include "configuration.h"

#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include <ipfixcol.h>
#include <libxml/tree.h>
#include <libxml/xmlstring.h>
#include <libxml/parser.h>

#include "ipfix_file.h"

// This is wrong! It should be "file://"
// But to preserve backwards compatibility, we use this value.
#define FILE_URI_PREFIX "file:"

/**
 * \brief Compare a value of a node with string boolen value
 * \param[in] doc XML document
 * \param[in] node XML node
 * \return For "true" returns 1. For "false" returns -1. Otherwise (conversion
 *   failed) returns 0;
 */
static int
xml_cmp_bool(xmlDocPtr doc, const xmlNodePtr node)
{
	xmlChar *val = xmlNodeListGetString(doc, node->xmlChildrenNode, 1);
	if (!val) {
		MSG_ERROR(msg_module, "Configuration conversion failed "
			"(xmlNodeListGetString() returned NULL).");
		return 0;
	}

	// Is true?
	if (xmlStrcasecmp(val, (const xmlChar *) "yes") == 0 ||
			xmlStrcasecmp(val, (const xmlChar *) "true") == 0 ||
			xmlStrcasecmp(val, (const xmlChar *) "1") == 0) {
		xmlFree(val);
		return 1;
	}

	// Is false?
	if (xmlStrcasecmp(val, (const xmlChar *) "no") == 0 ||
			xmlStrcasecmp(val, (const xmlChar *) "false") == 0 ||
			xmlStrcasecmp(val, (const xmlChar *) "0") == 0) {
		xmlFree(val);
		return -1;
	}

	xmlFree(val);
	return 0;
}

/**
 * \brief Convert a value of the node to UINT64
 * \param[in]  doc XML document
 * \param[in]  node XML node
 * \param[out] out Result
 * \return On success return 0. Otherwise return a non-zero value.
 */
static int
xml_convert_number(xmlDocPtr doc, const xmlNodePtr node, uint64_t *out)
{
	xmlChar *val = xmlNodeListGetString(doc, node->xmlChildrenNode, 1);
	if (!val) {
		MSG_ERROR(msg_module, "Configuration conversion failed "
			"(xmlNodeListGetString() returned NULL).");
		return 1;
	}

	char *end_ptr = NULL;
	uint64_t result = strtoull((char *) val, &end_ptr, 10);

	if (end_ptr == NULL || *end_ptr != '\0') {
		// Conversion failed
		xmlFree(val);
		return 1;
	}

	xmlFree(val);
	*out = result;
	return 0;
}

/**
 * \brief Auxiliary match function for DumpInterval XML elements
 * \param[in]     doc XML document
 * \param[in]     cur XML node
 * \param[in,out] cfg Configuration
 * \return On success returns 0. Otherwise returns a non-zero value.
 */
static int
configuration_match_dump(xmlDocPtr doc, xmlNodePtr cur, struct conf_params *cfg)
{
	// Skip this node in case it's a comment or plain text node
	if (cur->type == XML_COMMENT_NODE || cur->type == XML_TEXT_NODE) {
		return 0;
	}

	if (!xmlStrcasecmp(cur->name, (const xmlChar*) "timeWindow")) {
		// Parse windows size
		uint64_t result;
		if (xml_convert_number(doc, cur, &result)) {
			MSG_ERROR(msg_module, "Configuration error (invalid value of "
				"<timeWindow> - expected unsigned integer).");
			return 1;
		}

		if (result > UINT32_MAX) {
			MSG_ERROR(msg_module, "Configuration error (invalid value of "
				"<timeWindow> - the value '%" PRIu64 "' is too high).", result);
			return 1;
		}

		cfg->window.size = (uint32_t) result;
		return 0;
	}

	if (!xmlStrcasecmp(cur->name, (const xmlChar*) "align")) {
		// Enable/disable alignment
		int result = xml_cmp_bool(doc, cur);
		if (result == 0) {
			MSG_ERROR(msg_module, "Configuration error (invalid value of "
				"<align> - expected true/false).", NULL);
			return 1;
		}

		cfg->window.align = (result > 0) ? true : false;
		return 0;
	}

	MSG_ERROR(msg_module, "Configuration error (unknown element \"%s\").",
		(char *) cur->name);
	return 1;
}

/**
 * \brief Match XML to appropriate configuration field and update it
 * \param[in]     doc XML document
 * \param[in]     cur XML node
 * \param[in,out] cfg Configuration
 * \return On success returns 0. Otherwise (invalid/unknown element) returns
 *   a non-zero value.
 */
static int
configuration_match(xmlDocPtr doc, xmlNodePtr cur, struct conf_params *cfg)
{
	// Skip this node in case it's a comment or plain text node
	if (cur->type == XML_COMMENT_NODE || cur->type == XML_TEXT_NODE) {
		return 0;
	}

	if (!xmlStrcasecmp(cur->name, (const xmlChar*) "fileFormat")) {
		// fileFormat - Skip...
		return 0;
	}

	if (!xmlStrcasecmp(cur->name, (const xmlChar*) "file")) {
		// Get a file patter
		if (cfg->output.pattern) {
			xmlFree(cfg->output.pattern);
		}

		xmlChar *file = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
		cfg->output.pattern = file;
		return 0;
	}

	if (!xmlStrcasecmp(cur->name, (const xmlChar*) "dumpInterval")) {
		// Get dump interval configuration
		xmlNodePtr cur_sub = cur->xmlChildrenNode;
		while (cur_sub != NULL) {
			if (configuration_match_dump(doc, cur_sub, cfg)) {
				return 1;
			}

			cur_sub = cur_sub->next;
		}

		return 0;
	}

	// Unknown XML element
	MSG_ERROR(msg_module, "Configuration error (unknown element \"%s\").",
		(char *) cur->name);
	return 1;
}

/**
 * \brief Check validity of a configuration
 * \param[in] cfg Configuration
 * \return If the configuration is valid, returns 0. Otherwise returns
 *   a non-zero value and prints an error message.
 */
static int
configuration_validate(const struct conf_params *cfg)
{
	if (!cfg->output.pattern) {
		MSG_ERROR(msg_module, "Storage path is not defined!", NULL);
		return 1;
	}

	if (xmlStrlen(cfg->output.pattern) == 0) {
		MSG_ERROR(msg_module, "Storage path is empty!", NULL);
		return 1;
	}

	return 0;
}

/**
 * \brief Patch an output file of the configuration
 *
 * Remove the URI identifier "file:" from the path
 * \param[in] cfg Configuration
 * \return On success returns 0. Otherwise returns a non-zero value.
 */
static int
configuration_patch(struct conf_params *cfg)
{
	if (!cfg->output.pattern) {
		return 1;
	}

	if (cfg->output.pattern[0] == '/') {
		// If the path is absolute, everything is OK.
		return 0;
	}

	// Check the file prefix
	xmlChar *path = cfg->output.pattern;
	const xmlChar *prefix = BAD_CAST FILE_URI_PREFIX;
	int path_len = xmlStrlen(path);
	int prefix_len = xmlStrlen(prefix);

	if (xmlStrncmp(path, prefix, prefix_len) != 0) {
		MSG_ERROR(msg_module, "Element \"file\": invalid URI - "
			"only allowed scheme is \"file:\" or an absolute path.", NULL);
		return 1;
	}

	// Remove the prefix
	xmlChar *new_path = xmlStrsub(path, prefix_len, path_len - prefix_len);
	if (!new_path) {
		MSG_ERROR(msg_module, "Elements \"file\": invalid URI - "
			"failed to remove URI identifier.", NULL);
		return 1;
	}

	// Replace the old path
	xmlFree(path);
	cfg->output.pattern = new_path;
	return 0;
}

/**
 * \brief Set default configuration params
 * \param[in,out] cnf Configuration
 * \return On success returns 0. Otherwise returns a non-zero value, the
 *   content of the configuration is undefined and it MUST be destroyed
 *   by configuration_free()
 */
static int
configuration_set_defaults(struct conf_params *cfg)
{
	cfg->output.pattern = NULL;

	cfg->window.align = false;
	cfg->window.size = 0; // Infinite
	return 0;
}

struct conf_params *
configuration_parse(const char *params)
{
	if (!params) {
		return NULL;
	}

	xmlDocPtr doc;
	xmlNodePtr cur;

	struct conf_params *cnf = calloc(1, sizeof(*cnf));
	if (!cnf) {
		MSG_ERROR(msg_module, "Unable to allocate memory (%s:%d)",
			__FILE__, __LINE__);
		return NULL;
	}

	if (configuration_set_defaults(cnf)) {
		// Failed
		configuration_free(cnf);
		return NULL;
	}

	// Try to parse plugin configuration
	doc = xmlReadMemory(params, strlen(params), "nobase.xml", NULL, 0);
	if (doc == NULL) {
		MSG_ERROR(msg_module, "Failed to parse the plugin configuration.");
		configuration_free(cnf);
		return NULL;
	}

	cur = xmlDocGetRootElement(doc);
	if (cur == NULL) {
		MSG_ERROR(msg_module, "Configuration is empty.");
		xmlFreeDoc(doc);
		configuration_free(cnf);
		return NULL;
	}

	if(xmlStrcasecmp(cur->name, (const xmlChar*) "fileWriter")) {
		MSG_ERROR(msg_module, "Root node != fileWriter");
		xmlFreeDoc(doc);
		configuration_free(cnf);
		return NULL;
	}

	// Process the configuration
	cur = cur->xmlChildrenNode;
	while (cur != NULL) {
		if (configuration_match(doc, cur, cnf)) {
			// Failed
			xmlFreeDoc(doc);
			configuration_free(cnf);
			return NULL;
		}

		// Next element
		cur = cur->next;
	}

	// We don't need the XML document anymore
	xmlFreeDoc(doc);

	// Check combinations
	if (configuration_validate(cnf)) {
		// Failed
		configuration_free(cnf);
		return NULL;
	}

	// Remove the URI identifier
	if (configuration_patch(cnf)) {
		configuration_free(cnf);
		return NULL;
	}

	return cnf;
}

void
configuration_free(struct conf_params *cfg)
{
	if (!cfg) {
		return;
	}

	if (cfg->output.pattern != NULL) {
		xmlFree(cfg->output.pattern);
	}

	free(cfg);
}
