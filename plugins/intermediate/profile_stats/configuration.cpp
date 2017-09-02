/**
 * \file configuration.cpp
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Configuration parser (source file)
 */
/*
 * Copyright (C) 2017 CESNET, z.s.p.o.
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
 *
 */

#include <stdexcept>
#include <memory>
#include <cstring>

#include "configuration.h"

extern "C" {
#include <ipfixcol.h>
#include <libxml2/libxml/parser.h>
}

// Default values
constexpr uint32_t INTERVAL_DEF = 300;
constexpr uint32_t INTERVAL_MAX = 3600;
constexpr uint32_t INTERVAL_MIN = 5;
constexpr bool     ALIGNMENT_DEF = true;

// Using declarations
using unique_doc = std::unique_ptr<xmlDoc, decltype(&::xmlFreeDoc)>;

/**
 * \brief Convert a value of a node to boolean value
 * \param[in] str XML string
 * \throws invalid_argument Invalid boolean value
 * \throws runtime_error    XML library error
 * \return Converted value
 */
bool
plugin_config::xml_value2bool(const xmlChar *str)
{
	if (!str) {
		throw std::invalid_argument("Value is not defined!");
	}

	// Is true?
	if (xmlStrcasecmp(str, (const xmlChar *) "yes") == 0
		|| xmlStrcasecmp(str, (const xmlChar *) "true") == 0
		|| xmlStrcasecmp(str, (const xmlChar *) "1") == 0) {
		return true;
	}

	// Is false?
	if (xmlStrcasecmp(str, (const xmlChar *) "no") == 0
		|| xmlStrcasecmp(str, (const xmlChar *) "false") == 0
		|| xmlStrcasecmp(str, (const xmlChar *) "0") == 0) {
		return false;
	}

	std::string err_value = reinterpret_cast<const char *>(str);
	throw std::invalid_argument("Invalid boolean value \"" + err_value + "\"");
}

/**
 * \brief Convert a value of the node to UINT64
 * \param[in] str XML string
 * \throws invalid_argument Invalid boolean value
 * \throws runtime_error    XML library error
 * \return Converted value
 */
uint64_t
plugin_config::xml_value2uint(const xmlChar *str)
{
	if (!str) {
		throw std::invalid_argument("Value is not defined!");
	}

	uint64_t result;
	char *end_ptr = nullptr;
	const char *value_cstr = reinterpret_cast<const char *>(str);

	errno = 0;
	result = strtoull(value_cstr, &end_ptr, 10);
	if (errno != 0 || end_ptr == nullptr || *end_ptr != '\0') {
		// Conversion failed
		throw std::invalid_argument("Invalid unsigned integer value \""
			+ std::string(value_cstr) + "\"");
	}

	return result;
}

plugin_config::plugin_config(const char *params)
{
	if (!params) {
		throw std::runtime_error("An XML configuration not defined!");
	}

	// Set defaults
	set_defaults();

	// Parse the document
	unique_doc doc(xmlReadMemory(params, strlen(params), "nobase.xml", NULL, 0),
		&::xmlFreeDoc);
	if (!doc) {
		throw std::runtime_error("Failed to parse an XML configuration!");
	}

	xmlNodePtr cur = xmlDocGetRootElement(doc.get());
	if (!cur) {
		throw std::runtime_error("Configuration is empty!");
	}

	// Process the configuration
	cur = cur->xmlChildrenNode;
	while (cur != nullptr) {
		match_param(doc.get(), cur);
		cur = cur->next;
	}

	// Validate the configuration
	validate();
}

/**
 * \brief Check if the current configuration is valid
 */
void
plugin_config::validate()
{
	if (interval < INTERVAL_MIN || interval > INTERVAL_MAX) {
		throw std::runtime_error("Interval value is out of allowed range ("
			+ std::to_string(INTERVAL_MIN) + " - "
			+ std::to_string(INTERVAL_MAX) + ")");
	}
}

/**
 * \brief Set default parameters
 */
void
plugin_config::set_defaults()
{
	interval = INTERVAL_DEF;
	alignment = ALIGNMENT_DEF;
	base_dir.clear();
}

/**
 * \brief Match a parameter and update configuration
 * \param[in] doc  XML document
 * \param[in] node Current XML node within the document
 * \throws runtime_error    XML library error
 * \throws invalid_argument Invalid argument
 */
void
plugin_config::match_param(xmlDocPtr doc, xmlNodePtr node)
{
	// Skip this node in case it's a comment or plain text node
	if (node->type == XML_COMMENT_NODE || node->type == XML_TEXT_NODE) {
		return;
	}

	if (!xmlStrcasecmp(node->name, (const xmlChar*) "fileFormat")) {
		// fileFormat - Skip...
		return;
	}

	// Warning: xml_val can be NULL if the content is empty!
	auto deleter = [](xmlChar *data) {xmlFree(data);};
	std::unique_ptr<xmlChar, decltype(deleter)>
		xml_val(xmlNodeListGetString(doc, node->xmlChildrenNode, 1), deleter);

	if (!xmlStrcasecmp(node->name, (const xmlChar *) "interval")) {
		// Get update interval
		try {
			interval = xml_value2uint(xml_val.get());
		} catch (std::exception &ex) {
			throw std::runtime_error("Conversion of parameter \"interval\" "
				"failed: " + std::string(ex.what()));
		}

		return;
	}

	if (!xmlStrcasecmp(node->name, (const xmlChar *) "align")) {
		// Get alignment
		try {
			alignment = xml_value2bool(xml_val.get());
		} catch (std::exception &ex) {
			throw std::runtime_error("Conversion of parameter \"align\" "
				"failed: " + std::string(ex.what()));
		}

		return;
	}

	if (!xmlStrcasecmp(node->name, (const xmlChar *) "baseDir")) {
		// Get the base storage directory
		if (!xml_val.get()) {
			// Value is not defined
			base_dir.clear();
			return;
		}

		const char *str_ptr = reinterpret_cast<const char *>(xml_val.get());
		char *tmp_str = utils_path_preprocessor(str_ptr);
		if (tmp_str) {
			base_dir = tmp_str;
			free(tmp_str);
			return;
		}

		throw std::runtime_error("Path preprocessor failed during "
			"processing of the name of the base storage directory.");
	}

	// Unknown XML element
	const char *err_name = reinterpret_cast<const char *>(node->name);
	throw std::runtime_error("Unknown configuration parameter \""
		+ std::string(err_name) + "\"");
}