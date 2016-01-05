/**
 * \file lnfstore.c
 * \author Imrich Stoffa <xstoff02@stud.fit.vutbr.cz>
 * \author Lukas Hutak <xhutak01@stud.fit.vutbr.cz>
 * \brief lnfstore plugin interface (source file)
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

#include "lnfstore.h"
#include "storage.h"
#include <ipfixcol.h>
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>
#include <stdlib.h>
#include <string.h>

// API version constant
IPFIXCOL_API_VERSION;

// Module identification
static const char* msg_module= "lnfstore";

/**
 * \brief Compare value of the node with string boolen value
 * \param[in] doc XML document
 * \param[in] node XML node
 * \param[in] bool_val type of expected bool value
 * \return If value is same as expected, returns non-zero value. Otherwise
 * returns 0.
 */
int xml_cmp_bool(xmlDocPtr doc, const xmlNodePtr node, bool bool_val)
{
	int ret_val;
	xmlChar *val = xmlNodeListGetString(doc, node->xmlChildrenNode, 1);

	ret_val = xmlStrcasecmp(val, (const xmlChar *) (bool_val ? "yes" : "no")) &&
		xmlStrcasecmp(val, (const xmlChar *) (bool_val ? "true" : "false")) &&
		xmlStrcasecmp(val, (const xmlChar *) (bool_val ? "1" : "0"));

	xmlFree(val);
	return !ret_val;
}

/**
 * \brief Convert a value of the node to a number
 * \param[in] doc XML document
 * \param[in] node XML node
 * \param[out] out Result
 * \return On success return 0, Otherwise return non-zero value.
 */
int xml_convert_number(xmlDocPtr doc, const xmlNodePtr node, uint32_t *out)
{
	xmlChar *val = xmlNodeListGetString(doc, node->xmlChildrenNode, 1);

	char *end_ptr = NULL;
	uint32_t result = strtoul((char *) val, &end_ptr, 10);

	if (end_ptr != NULL && *end_ptr != '\0') {
		// Conversion failed
		xmlFree(val);
		return 1;
	}

	xmlFree(val);
	*out = result;
	return 0;
}

/**
 * \brief Destroy parsed plugin configuration
 * \param[in,out] cnf Structure with parsed parameters
 */
void destroy_parsed_params(struct conf_params *cnf)
{
	if (cnf) {
		if (cnf->storage_path) {
			free(cnf->storage_path);
		}
		if (cnf->file_prefix) {
			free(cnf->file_prefix);
		}
		if (cnf->file_suffix) {
			free(cnf->file_suffix);
		}
		if (cnf->file_ident) {
			free(cnf->file_ident);
		}

		free(cnf);
	}
}

/**
 * \brief Parse plugin configuration
 * \param[in] params XML configuration
 * \return On success returns pointer to the parsed configuration. Otherwise
 * retuns NULL.
 */
struct conf_params *process_startup_xml(const char *params)
{
	xmlDocPtr doc;
	xmlNodePtr cur, cur_sub;

	struct conf_params *cnf = calloc(1, sizeof(struct conf_params));
	if (!cnf) {
		MSG_ERROR(msg_module, "Unable to allocate memory (%s:%d)",
			__FILE__, __LINE__);
		return NULL;
	}

	// Try to parse plugin configuration
	doc = xmlReadMemory(params, strlen(params), "nobase.xml", NULL, 0);
	if (doc == NULL) {
		MSG_ERROR(msg_module, "Plugin configuration not parsed successfully");
		free(cnf);
		return NULL;
	}

	cur = xmlDocGetRootElement(doc);
	if (cur == NULL) {
		MSG_ERROR(msg_module, "Empty configuration");
		goto err_xml;
	}

	if(xmlStrcmp(cur->name, (const xmlChar*) "fileWriter")){
		MSG_ERROR(msg_module, "Root node != fileWriter");
		goto err_xml;
	}

	// Process the configuration elements
	cur = cur->xmlChildrenNode;
	while(cur != NULL){
		// Skip this node in case it's a comment or plain text node
		if (cur->type == XML_COMMENT_NODE || cur->type == XML_TEXT_NODE) {
			cur = cur->next;
			continue;
		}

		// Check conf
		if (!xmlStrcmp(cur->name, (const xmlChar*) "fileFormat")) {
			// fileFormat - Skip...
		} else if (!xmlStrcmp(cur->name, (const xmlChar*) "profiles")) {
			// Enable/disable profiler
			cnf->profiles = xml_cmp_bool(doc, cur, true);
		} else if (!xmlStrcmp(cur->name, (const xmlChar*) "compress")) {
			// Enable/disable compression
			cnf->compress = xml_cmp_bool(doc, cur, true);
		} else if (!xmlStrcmp(cur->name, (const xmlChar*) "storagePath")) {
			// Get storage path
			cnf->storage_path = (char *) xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
		} else if (!xmlStrcmp(cur->name, (const xmlChar*) "prefix")) {
			// Get file prefix
			cnf->file_prefix = (char *) xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
		} else if (!xmlStrcmp(cur->name, (const xmlChar*) "suffixMask")) {
			// Get file suffix
			cnf->file_suffix = (char *) xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
		} else if (!xmlStrcmp(cur->name, (const xmlChar*) "identificatorField")) {
			// Get internal file identification
			cnf->file_ident = (char *) xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
		} else if (!xmlStrcmp(cur->name, (const xmlChar*) "dumpInterval")) {
			// Dump interval subsection
			cur_sub = cur->xmlChildrenNode;

			while (cur_sub != NULL) {
				if (!xmlStrcmp(cur_sub->name, (const xmlChar*) "timeWindow")) {
					// Parse windows size
					if (xml_convert_number(doc, cur_sub, &cnf->window_time) != 0) {
						MSG_ERROR(msg_module, "Failed to parse the value of "
							"\"timeWindow\" node");
						goto err_xml;
					}
				} else if (!xmlStrcmp(cur_sub->name, (const xmlChar*) "align")) {
					// Parse window alignment
					cnf->window_align = !xml_cmp_bool(doc, cur_sub, false);
				}

				cur_sub = cur_sub->next;
			}
		} else {
			MSG_WARNING(msg_module, "Unknown element \"%s\" in configuration "
				"skipped.", cur->name);
		}

		// Next element
		cur = cur->next;
	}

	// Check if all required elements is filled
	if (!cnf->file_prefix) {
		MSG_ERROR(msg_module, "File prefix is not set.");
		goto err_xml;
	}

	if (!cnf->file_suffix) {
		MSG_ERROR(msg_module, "File suffix is not set.");
		goto err_xml;
	}

	if (!cnf->profiles && !cnf->storage_path) {
		MSG_ERROR(msg_module, "Storage path is not set.");
		goto err_xml;
	}

	if (cnf->window_time == 0) {
		MSG_WARNING(msg_module, "Time windows is not set. Using default value"
			"(300 seconds).");
	}

	xmlFreeDoc(doc);
	return cnf;

err_xml:
	destroy_parsed_params(cnf);
	xmlFreeDoc(doc);
	return NULL;
}

// Storage plugin initialization function.
int storage_init (char *params, void **config)
{	
	// Process XML configuration
	struct conf_params *parsed_params = process_startup_xml(params);
	if (!parsed_params) {
		return 1;
	}

	// Create new plugin configuration
	struct lnfstore_conf *conf = calloc(1, sizeof(struct lnfstore_conf));
	if (!conf) {
		MSG_ERROR(msg_module, "Unable to allocate memory (%s:%d)",
			__FILE__, __LINE__);
		destroy_parsed_params(parsed_params);
		return 1;
	}

	conf->params = parsed_params;

	// Init a record
	if (lnf_rec_init(&conf->rec_ptr) != LNF_OK) {
		MSG_ERROR(msg_module, "Failed to initialize structure for conversion "
			"of records");
		destroy_parsed_params(parsed_params);
		free(conf);
		return 1;
	}

	/* Save configuration */
	*config = conf;

	MSG_DEBUG(msg_module, "initialized...");
	return 0;
}

/*
 * Pass IPFIX data with supplemental structures from ipfixcol core into
 * the storage plugin.
 */
int store_packet (void *config, const struct ipfix_message *ipfix_msg,
	const struct ipfix_template_mgr *template_mgr)
{
	(void) template_mgr;
	struct lnfstore_conf *conf = (struct lnfstore_conf *) config;
	
	for (int i = 0; i < ipfix_msg->data_records_count; i++)
	{
		store_record(&(ipfix_msg->metadata[i]), conf);
	}
	
	return 0;
}

// Announce willing to store currently processing data
int store_now (const void *config)
{
	(void) config;
	return 0;
}

// Storage plugin "destructor"
int storage_close (void **config)
{
	MSG_DEBUG(msg_module, "closing...");
	struct lnfstore_conf *conf = (struct lnfstore_conf *) *config;
	
	/* Destroy configuration */
	lnf_rec_free(conf->rec_ptr);
	close_storage_files(conf);

	if (conf->profiles_ptr) {
		free(conf->profiles_ptr);
	}

	if (conf->bitset) {
		bitset_destroy(conf->bitset);
	}

	destroy_parsed_params(conf->params);

	free(conf);
	*config = NULL;
	return 0;
}
