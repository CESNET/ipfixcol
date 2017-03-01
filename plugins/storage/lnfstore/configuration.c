/**
 * \file configuration.c
 * \author Lukas Hutak <xhutak01@stud.fit.vutbr.cz>
 * \brief Configuration parser (source file)
 */
 /* Copyright (C) 2016-2017 CESNET, z.s.p.o.
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

#include "configuration.h"
#include "lnfstore.h"
#include "idx_manager.h"

#include <ipfixcol.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <unistd.h>
#include <libxml2/libxml/tree.h>
#include <libxml2/libxml/xmlstring.h>
#include <libxml2/libxml/parser.h>

extern const char *msg_module;

#define SUFFIX_MASK             "%Y%m%d%H%M%S"
#define LNF_FILE_PREFIX         "lnf."
#define BF_FILE_PREFIX          "bfi."
#define BF_DEFAULT_FP_PROB      0.01
#define BF_DEFAULT_ITEM_CNT_EST 100000

#define WINDOW_SIZE             300U

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
			xmlStrcasecmp(val, (const xmlChar *) "1") == 0)
	{
		xmlFree(val);
		return 1;
	}

	// Is false?
	if (xmlStrcasecmp(val, (const xmlChar *) "no") == 0 ||
			xmlStrcasecmp(val, (const xmlChar *) "false") == 0 ||
			xmlStrcasecmp(val, (const xmlChar *) "0") == 0)
	{
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
 * \return On success return 0, Otherwise return a non-zero value.
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
 * \brief Convert a value of the node to double
 * \param[in]  doc  XML document
 * \param[in]  node XML node
 * \param[out] out  Result
 * \return On success returns 0. Otherwise returns a non-zero value.
 */
static int
xml_convert_double(xmlDocPtr doc, const xmlNodePtr node, double *out)
{
	xmlChar *val = xmlNodeListGetString(doc, node->xmlChildrenNode, 1);
	if (!val) {
		MSG_ERROR(msg_module, "Configuration conversion failed "
			"(xmlNodeListGetString() returned NULL).");
		return 1;
	}

	char *end_ptr = NULL;
	double result = strtod((char *) val, &end_ptr);

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
			MSG_ERROR(msg_module, "Configuration error - invalid value of "
				"<timeWindow> (expected unsigned integer).");
			return 1;
		}

		if (result > UINT32_MAX) {
			MSG_ERROR(msg_module, "Configuration error - invalid value of "
				"<timeWindow> (the value '%" PRIu32 "' is too high).", result);
			return 1;
		}

		cfg->window.size = (uint32_t) result;
		return 0;
	}

	if (!xmlStrcasecmp(cur->name, (const xmlChar*) "align")) {
		// Enable/disable alignment
		int result = xml_cmp_bool(doc, cur);
		if (result == 0) {
			MSG_ERROR(msg_module, "Configuration error - invalid value of "
					"<align> (expected true/false).");
			return 1;
		}

		cfg->window.align = (result > 0) ? true : false;
		return 0;
	}

	MSG_ERROR(msg_module, "Configuration error - Unknown element \"%s\".\n",
		(char *) cur->name);
	return 1;
}

/**
 * \brief Auxiliary match function for Bloom Filter Index XML elements
 * \param[in]     doc XML document
 * \param[in]     cur XML node
 * \param[in,out] cfg Configuration
 * \return On success returns 0. Otherwise returns a non-zero value.
 */
static int
configuration_match_idx(xmlDocPtr doc, xmlNodePtr cur, struct conf_params *cfg)
{
	// Skip this node in case it's a comment or plain text node
	if (cur->type == XML_COMMENT_NODE || cur->type == XML_TEXT_NODE) {
		return 0;
	}

	if (!xmlStrcasecmp(cur->name, (const xmlChar*) "enable")) {
		// Enable/disable alignment
		int result = xml_cmp_bool(doc, cur);
		if (result == 0) {
			MSG_ERROR(msg_module, "Configuration error - invalid value of "
					"<enable> (expected true/false).");
			return 1;
		}

		cfg->file_index.en = (result > 0) ? true : false;
		return 0;
	}

	if (!xmlStrcasecmp(cur->name, (const xmlChar*) "prefix")) {
		// Index file prefix
		if (cfg->file_index.prefix) {
			xmlFree((xmlChar *) cfg->file_index.prefix);
		}

		xmlChar *result = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
		cfg->file_index.prefix = (char *) result;
		return 0;
	}

	if (!xmlStrcasecmp(cur->name, (const xmlChar*) "autosize")) {
		// Enable/disable autosize
		int result = xml_cmp_bool(doc, cur);
		if (result == 0) {
			MSG_ERROR(msg_module, "Configuration error - invalid value of "
				"<autosize> (expected true/false).");
			return 1;
		}

		cfg->file_index.autosize = (result > 0) ? true : false;
		return 0;
	}

	if (!xmlStrcasecmp(cur->name, (const xmlChar*) "estimatedItemCount")) {
		// Estimated item count
		uint64_t result;
		if (xml_convert_number(doc, cur, &result)) {
			MSG_ERROR(msg_module, "Configuration error - invalid value of "
				"<estimatedItemCount> (expected unsigned integer).");
			return 1;
		}

		cfg->file_index.est_cnt = result;
		return 0;
	}

	if (!xmlStrcasecmp(cur->name, (const xmlChar*) "falsePositiveProbability")) {
		// False positive probability
		double result;
		if (xml_convert_double(doc, cur, &result)) {
			MSG_ERROR(msg_module, "Configuration error - invalid value of "
				"<falsePositiveProbability> (expected decimal number).");
			return 1;
		}

		cfg->file_index.fp_prob = result;
		return 0;
	}

	MSG_ERROR(msg_module, "Configuration error - Unknown element \"%s\".\n",
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

	if (!xmlStrcmp(cur->name, (const xmlChar*) "profiles")) {
		// Enable/disable profiler
		int result = xml_cmp_bool(doc, cur);
		if (result == 0) {
			MSG_ERROR(msg_module, "Configuration error - invalid value of "
				"<profiles> (expected true/false).");
			return 1;
		}

		cfg->profiles.en = (result > 0) ? true : false;
		return 0;
	}

	if (!xmlStrcasecmp(cur->name, (const xmlChar*) "storagePath")) {
		// Get LNF and Index storage path (only non-profile)
		xmlChar *original = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
		if (!original) {
			MSG_ERROR(msg_module, "Configuration conversion failed "
				"(xmlNodeListGetString() returned NULL).");
			return 1;
		}

		cfg->files.path = utils_path_preprocessor((const char *) original);
		xmlFree(original);
		if (!cfg->files.path) {
			return 1;
		}

		return 0;
	}

	if (!xmlStrcasecmp(cur->name, (const xmlChar*) "suffixMask")) {
		// Get LNF & Index file suffix
		if (cfg->files.suffix) {
			xmlFree((xmlChar *) cfg->files.suffix);
		}

		xmlChar *result = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
		cfg->files.suffix = (char *) result;
		return 0;
	}

	if (!xmlStrcasecmp(cur->name, (const xmlChar*) "prefix")) {
		// Get LNF file prefix
		if (cfg->file_lnf.prefix) {
			xmlFree((xmlChar *) cfg->file_lnf.prefix);
		}

		xmlChar *result = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
		cfg->file_lnf.prefix = (char *) result;
		return 0;
	}

	if (!xmlStrcasecmp(cur->name, (const xmlChar*) "identificatorField")) {
		// Get internal LNF file identification
		if (cfg->file_lnf.ident) {
			xmlFree((xmlChar *) cfg->file_lnf.ident);
		}

		xmlChar * result = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
		cfg->file_lnf.ident = (char *) result;
		return 0;
	}

	if (!xmlStrcasecmp(cur->name, (const xmlChar*) "compress")) {
		// Enable/disable compression
		int result = xml_cmp_bool(doc, cur);
		if (result == 0) {
			MSG_ERROR(msg_module, "Configuration error - invalid definition of "
					"<compress> (expected true/false).");
			return 1;
		}

		cfg->file_lnf.compress = (result > 0) ? true : false;
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

	if (!xmlStrcasecmp(cur->name, (const xmlChar*) "index")) {
		// Get Bloom Filter Index configuration
		xmlNodePtr cur_sub = cur->xmlChildrenNode;
		while (cur_sub != NULL) {
			if (configuration_match_idx(doc, cur_sub, cfg)) {
				return 1;
			}

			cur_sub = cur_sub->next;
		}

		return 0;
	}

	// Unknown XML element
	MSG_ERROR(msg_module, "Configuration error - Unknown element \"%s\".\n",
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
	int ret_code = 0;

	if (!cfg->profiles.en && !cfg->files.path) {
		MSG_ERROR(msg_module, "Storage path is not set.");
		ret_code = 1;
	}

	if (!cfg->files.suffix) {
		MSG_ERROR(msg_module, "File suffix is not set.");
		ret_code = 1;
	}

	if (!cfg->file_lnf.prefix) {
		MSG_ERROR(msg_module, "LNF file prefix is not set.");
		ret_code = 1;
	}

	if (cfg->file_index.en) {
		if (!cfg->file_index.prefix) {
			MSG_ERROR(msg_module, "Index file prefix is not set.");
			ret_code = 1;
		}

		if (cfg->file_index.est_cnt == 0) {
			MSG_ERROR(msg_module, "Estimated item count in Bloom Filter Index "
				"must be greater than 0.");
			ret_code = 1;
		}

		if (cfg->file_index.fp_prob > FPP_MAX ||
				cfg->file_index.fp_prob < FPP_MIN) {
			MSG_ERROR(msg_module, "Wrong false positive probability value. "
				"Use a value from %f to %f.", FPP_MIN, FPP_MAX);
			ret_code = 1;
		}

		// Check output prefixes
		if (xmlStrcmp((const xmlChar *) cfg->file_index.prefix,
				(const xmlChar *) cfg->file_lnf.prefix) == 0) {
			MSG_ERROR(msg_module, "The same file prefix for LNF and Index file "
				"is not allowed");
			ret_code = 1;
		}
	}

	if (cfg->window.size == 0) {
		MSG_ERROR(msg_module, "Window size must be greater than 0.");
		ret_code = 1;
	}

	return ret_code;
}

/**
 * \brief Set default configuration params
 * \param[in,out] cnf Configuration
 * \return On success returns 0. Otherwise returns a non-zero value, the
 *   content of the configuration is undefined and it MUST be destroyed
 *   by configuration_free()
 */
static int
configuration_set_defaults(struct conf_params *cnf)
{
	// Dump interval
	cnf->window.align = true;
	cnf->window.size = WINDOW_SIZE;

	// Files (common)
	cnf->files.suffix = (char *) (xmlCharStrdup(SUFFIX_MASK));
	if (!cnf->files.suffix) {
		MSG_ERROR(msg_module, "Unable to allocate memory (%s:%d)",
			__FILE__, __LINE__);
		return 1;
	}

	// LNF file
	cnf->file_lnf.prefix = (char *) (xmlCharStrdup(LNF_FILE_PREFIX));
	if (!cnf->file_lnf.prefix) {
		MSG_ERROR(msg_module, "Unable to allocate memory (%s:%d)",
			__FILE__, __LINE__);
		return 1;
	}

	// Index file
	cnf->file_index.en = false;
	cnf->file_index.autosize = true;
	cnf->file_index.est_cnt = BF_DEFAULT_ITEM_CNT_EST;
	cnf->file_index.fp_prob = BF_DEFAULT_FP_PROB;
	cnf->file_index.prefix = (char *) (xmlCharStrdup(BF_FILE_PREFIX));
	if (!cnf->file_index.prefix) {
		MSG_ERROR(msg_module, "Unable to allocate memory (%s:%d)",
			__FILE__, __LINE__);
		return 1;
	}

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

	struct conf_params *cnf = calloc(1, sizeof(struct conf_params));
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

	if(xmlStrcasecmp(cur->name, (const xmlChar*) "fileWriter")){
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

	return cnf;
}

void
configuration_free(struct conf_params *config)
{
	if (!config) {
		return;
	}

	free(config->files.path);

	if (config->files.suffix) {
		xmlFree((xmlChar *) config->files.suffix);
	}

	if (config->file_lnf.prefix) {
		xmlFree((xmlChar *) config->file_lnf.prefix);
	}

	if (config->file_lnf.ident) {
		xmlFree((xmlChar *) config->file_lnf.ident);
	}

	if (config->file_index.prefix) {
		xmlFree((xmlChar *) config->file_index.prefix);
	}

	free(config);
}
