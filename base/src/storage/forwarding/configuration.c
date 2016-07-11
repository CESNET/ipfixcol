/**
 * \file storage/forwarding/configuration.c
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Configuration of the forwarding plugin (source file)
 *
 * Copyright (C) 2016 CESNET, z.s.p.o.
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

#include <stdlib.h>
#include <strings.h>
// IPFIXcol API
#include <ipfixcol.h>
// libXML2 API
#include <libxml/parser.h>
#include <libxml/tree.h>
// Plugin's files
#include "configuration.h"

/** Default destination port                 */
#define DEF_PORT "4739"
/** Default retry interval (seconds)         */
#define DEF_RETRY_INT (5)
/** Default maximal packet size              */
#define DEF_PACKET_SIZE (4096)

static const char *msg_module = "forwarding(config)";

/**
 * \brief Auxiliary structure for the configuration parser
 */
struct parser_context {
	xmlDoc *doc;                /**< XML document         */
	xmlNode *node;              /**< XML "root" node      */
	struct plugin_config *cfg;  /**< Plugin configuration */
};

/**
 * \brief Prepare XML configuration
 * \param[in] str Configuration string
 * \param[out] doc  Parsed XML document
 * \param[out] root Root element of the configuration
 * \warning Document \p doc MUST be freed be xmlFreeDoc();
 * \return On success returns 0. Otherwise returns non-zero value.
 */
static int config_prepare_xml(const char *str, xmlDoc **doc_ptr,
	xmlNode **root_ptr)
{
	xmlDoc *doc = NULL;
	xmlNode *node = NULL;

	doc = xmlParseDoc(BAD_CAST str);
	if (!doc) {
		MSG_ERROR(msg_module, "Could not parse plugin configuration.");
		return -1;
	}

	node = xmlDocGetRootElement(doc);
	if (!node) {
		MSG_ERROR(msg_module, "Could not get the root element of the plugin "
			"configuration.");
		xmlFreeDoc(doc);
		return -1;
	}

	if (xmlStrcmp(node->name, (const xmlChar *) "fileWriter")) {
		MSG_ERROR(msg_module, "Root node of the configuration is not "
			"'fileWriter'.");
		xmlFreeDoc(doc);
		return -1;
	}

	*doc_ptr = doc;
	*root_ptr = node;
	return 0;
}

/**
 * \brief Get default destination port
 * \param[in] cfg Plugin configuration
 * \return Port
 */
static const char *config_def_port(const struct plugin_config *cfg) {
	return (cfg->def_port) ? cfg->def_port : DEF_PORT;
}

/**
 * \brief Parse mode of flow distribution
 * \param[in] str String
 * \return Type of distribution
 */
static enum DIST_MODE config_parse_distr(const char *str)
{
	if (!str) {
		return DIST_INVALID;
	}

	if (!strcasecmp(str, "all")) {
		return DIST_ALL;
	} else if (!strcasecmp(str, "roundrobin")) {
		return DIST_ROUND_ROBIN;
	} else {
		return DIST_INVALID;
	}
}

/**
 * \brief Parse only default values from the plugin configuration
 * \param[in,out] ctx Parser context
 * \return On sucess returns 0. Ohterwise returns non-zero value.
 */
static int config_parse_def_values(struct parser_context *ctx)
{
	xmlDoc *doc = ctx->doc;
	xmlNode *cur = ctx->node;
	struct plugin_config *cfg = ctx->cfg;

	while (cur) {
		if (!xmlStrcasecmp(cur->name, (const xmlChar *) "defaultPort")) {
			// Default port
			free(cfg->def_port);
			cfg->def_port = (char *) xmlNodeListGetString(doc,
				cur->xmlChildrenNode, 1);
		} else {
			// Other nodes -> skip
		}

		cur = cur->next;
	}

	return 0;
}

/**
 * \brief Convert string value to integer
 * \param[in] val String
 * \param[out] res Result integer
 * \return On success returns 0 and fill \p res with converted value. Otherwise
 * returns non-zero value.
 */
static int config_parse_int(const char *val, int *res)
{
	if (!val) {
		return 1;
	}

	char *end_ptr;
	int tmp_res;

	tmp_res = strtol(val, &end_ptr, 10);
	if (*end_ptr != '\0') {
		return 1;
	}

	*res = tmp_res;
	return 0;
}

/**
 * \brief Parse a destination node
 * \param[in] ctx Parser context (with destination node)
 * \return On success returns a new sender. Otherwise returns NULL.
 */
static fwd_sender_t *config_parse_destination(struct parser_context *ctx)
{
	xmlChar *str_ip = NULL;
	xmlChar *str_port = NULL;
	xmlNode *cur = ctx->node;

	// Find all related XML nodes & parse them
	while (cur) {
		if (cur->type == XML_COMMENT_NODE) {
			// Comments -> skip
		} else if (!xmlStrcasecmp(cur->name, (const xmlChar *) "ip")) {
			// Destination IP
			if (str_ip) {
				xmlFree(str_ip);
			}
			str_ip = xmlNodeListGetString(ctx->doc, cur->xmlChildrenNode, 1);
		} else if (!xmlStrcasecmp(cur->name, (const xmlChar *) "port")) {
			// Destination port
			if (str_port) {
				xmlFree(str_port);
			}
			str_port = xmlNodeListGetString(ctx->doc, cur->xmlChildrenNode, 1);
		} else {
			// Other unknown nodes
			MSG_WARNING(msg_module, "Unknown node '%s' in 'destination' node "
				"skipped.", cur->name);
		}

		cur = cur->next;
	}

	// Create new sender
	const char *dst_ip = (const char *) str_ip;
	const char *dst_port = (str_port != NULL)
			? (const char *) str_port : config_def_port(ctx->cfg);

	fwd_sender_t *new_sender;
	new_sender = sender_create(dst_ip, dst_port);

	// Clean up
	if (str_ip)
		xmlFree(str_ip);
	if (str_port)
		xmlFree(str_port);

	return new_sender;
}

/**
 * \brief Parse XML configuration
 * \param[in,out] ctx Parser context
 * \return On success returns 0. Otherwise returns non-zero value.
 */
static int config_parse_xml(struct parser_context *ctx)
{
	// Parse default values first
	if (config_parse_def_values(ctx)) {
		return 1;
	}

	xmlDoc *doc = ctx->doc;
	xmlNode *cur = ctx->node->children;
	xmlChar *aux_str = NULL;
	int failed = false;
	unsigned int added_dest = 0;

	while (cur && !failed) {
		if (!xmlStrcasecmp(cur->name, (const xmlChar *) "defaultPort")) {
			// Default values were already processed -> skip
		} else if (!xmlStrcasecmp(cur->name, (const xmlChar *) "fileFormat")) {
			// Useless node -> skip
		} else if (cur->type == XML_COMMENT_NODE) {
			// Comments -> skip;
		} else if (!xmlStrcasecmp(cur->name, (const xmlChar *) "distribution")) {
			// Distribution type
			aux_str = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			ctx->cfg->mode = config_parse_distr((char *) aux_str);
		} else if (!xmlStrcasecmp(cur->name, (const xmlChar *) "packetSize")) {
			// Maximal packet size
			int result;
			aux_str = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			if (config_parse_int((char *) aux_str, &result)) {
				// Conversion failed
				MSG_ERROR(msg_module, "Failed to parse 'packetSize' node.");
				failed = true;
			} else if (result < 256 || result > 65535) {
				// Out of range
				MSG_ERROR(msg_module, "Packet size is out of range (min: 256, "
					"max: 65535)");
				failed = true;
			} else {
				ctx->cfg->packet_size = (uint16_t) result;
			}
		} else if (!xmlStrcasecmp(cur->name, (const xmlChar *) "destination")) {
			// Destination address & port
			struct parser_context dst_ctx = {doc, cur->children, ctx->cfg};
			fwd_sender_t *dst = config_parse_destination(&dst_ctx);
			if (!dst || dest_add(ctx->cfg->dest_mgr, dst)) {
				// Conversion failed
				MSG_ERROR(msg_module, "Failed to parse 'destination' node.");
				sender_destroy(dst);
				failed = true;
			} else {
				added_dest++;
			}
		} else if (!xmlStrcasecmp(cur->name, (const xmlChar *) "reconnectionPeriod")) {
			// Reconnection period
			int result;
			aux_str = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			if (config_parse_int((char *) aux_str, &result)) {
				// Conversion failed
				MSG_ERROR(msg_module, "Failed to parse 'reconnectionPeriod' "
					"node.");
				failed = true;
			} else if (result <= 0) {
				// Out of range
				MSG_ERROR(msg_module, "Reconnection period cannot be zero or "
					"negative.");
				failed = true;
			} else {
				ctx->cfg->reconn_period = result;
			}
		} else {
			// Other unknown nodes
			MSG_WARNING(msg_module, "Unknown node '%s' skipped.", cur->name);
		}

		if (aux_str) {
			xmlFree(aux_str);
			aux_str = NULL;
		}

		cur = cur->next;
	}

	if (failed) {
		return 1;
	}

	// Check validity
	if (ctx->cfg->mode == DIST_INVALID) {
		// Invalid distribution type
		MSG_ERROR(msg_module, "Invalid distribution type.");
		return 1;
	}

	if (added_dest == 0) {
		// No destination added
		MSG_ERROR(msg_module, "No valid destinations.");
		return 1;
	}

	return 0;
}

/** Parse a configuration of the plugin */
struct plugin_config *config_parse(const char *cfg_string)
{
	if (!cfg_string) {
		return NULL;
	}

	// Prepare XML document
	xmlDoc *doc = NULL;
	xmlNode *node_root = NULL;

	if (config_prepare_xml(cfg_string, &doc, &node_root)) {
		return NULL;
	}

	// Create a configuration structure
	struct plugin_config *config;
	config = (struct plugin_config *) calloc(1, sizeof(struct plugin_config));
	if (!config) {
		MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)", __FILE__,
			__LINE__);
		xmlFreeDoc(doc);
		return NULL;
	}

	// Set default values
	config->mode = DIST_ALL;
	config->packet_size = DEF_PACKET_SIZE;
	config->reconn_period = 1000; // milliseconds

	config->builder_all = bldr_create();
	config->builder_tmplt = bldr_create();
	config->tmplt_mgr = tmplts_create();
	config->dest_mgr = dest_create(config->tmplt_mgr);
	if (!config->dest_mgr      || !config->builder_all ||
		!config->builder_tmplt || !config->tmplt_mgr) {
		// Failed to initialize one or more components
		config_destroy(config);
		xmlFreeDoc(doc);
		return NULL;
	}

	// Parse XML document
	struct parser_context ctx = {doc, node_root, config};
	if (config_parse_xml(&ctx)) {
		config_destroy(config);
		xmlFreeDoc(doc);
		return NULL;
	}

	xmlFreeDoc(doc);
	return config;
}

/** Destroy a configuration of the plugin */
void config_destroy(struct plugin_config *cfg)
{
	if (!cfg) {
		return;
	}

	// Disconnect everyone
	dest_destroy(cfg->dest_mgr);

	free(cfg->def_port);
	tmplts_destroy(cfg->tmplt_mgr);
	bldr_destroy(cfg->builder_all);
	bldr_destroy(cfg->builder_tmplt);

	free(cfg);
}

