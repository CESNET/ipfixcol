/**
 * \file storage/ipfix/configuration.h
 * \author Lukas Hutak <xhutak01@stud.fit.vutbr.cz>
 * \brief Configuration parser (header file)
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

#include <stdint.h>
#include <stdbool.h>
#include <libxml/xmlstring.h>

#ifndef FILE_CONFIGURATION_H
#define FILE_CONFIGURATION_H

/**
 * \brief Parsed XML parameters of the plugin
 */
struct conf_params {
	struct {
		xmlChar *pattern;  /**< File pattern (path + strftime specifiers)    */
	} output;   /**< Output file */

	struct {
		bool     align; /**< Enable/disable window alignment                 */
		uint32_t size;  /**< Time window size (0 == infinite)                */
	} window;   /**< Window alignment */
};

/**
 * \brief Parse the plugin configuration
 *
 * \warning The configuration MUST be free by configuration_free() function.
 * \param[in] params XML configuration
 * \return On success returns a pointer to the configuration. Otherwise returns
 *   NULL.
 */
struct conf_params *
configuration_parse(const char *params);

/**
 * \brief Destroy the plugin configuration
 * \param[in,out] cfg Configuration
 */
void
configuration_free(struct conf_params *cfg);


#endif // FILE_CONFIGURATION_H
