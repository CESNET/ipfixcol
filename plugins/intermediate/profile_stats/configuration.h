/**
 * \file configuration.h
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Configuration parser (header file)(
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

#ifndef RRD_CONFIGURATION_H
#define RRD_CONFIGURATION_H

#include <string>
#include <memory>
#include <cstdint>

extern "C" {
#include <libxml2/libxml/tree.h>
#include <libxml2/libxml/xmlstring.h>
}

/** Configuration parameters of the instance */
class plugin_config {
private:
	// Convert text to unsigned integer
	static uint64_t
	xml_value2uint(const xmlChar *str);
	// Convert text to boolean value
	static bool
	xml_value2bool(const xmlChar *str);

	// Set default values
	void set_defaults();
	// Validate the configuration
	void validate();
	// Match a parameter and update configuration
	void match_param(xmlDocPtr doc, xmlNodePtr node);
public:
	/**
 	 * \brief Constructor parses the plugin configuration
     * \param[in] params XML configuration
     * \throws In case of failure, throws an exception
     * \note In case of error, throws an exception.
     */
	explicit plugin_config(const char *params);
	/**
	 * \brief Destructor
	 */
	~plugin_config() = default;

	//----- Parsed parameters ----
	/** Update interval    */
	uint64_t interval;
	/** Interval alignment */
	bool alignment;
	/**
	 * Base storage directory
	 * \note Storage directory of each profile/channel MUST be within this
	 *   directory or the profile/channel will not be stored. If this parameter
	 *   is not defined, the string is empty and check is not performed.
	 */
	std::string base_dir;
};

#endif // RRD_CONFIGURATION_H
