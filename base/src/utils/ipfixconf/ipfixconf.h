/**
 * \file ipfixconf.h
 * \author Michal Kozubik <kozubik@cesnet.cz>
 * \brief Tool for editing IPFIXcol internalcfg.xml
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

#ifndef IPFIXCONF_H
#define IPFIXCONF_H

#include <libxml/xpathInternals.h>

#define TAG_INPUT  "inputPlugin"
#define TAG_INTER  "intermediatePlugin"
#define TAG_OUTPUT "storagePlugin"
#define TAG_SUPPORTED "supportedCollectors"

#define ITEMS_CNT 3

/**
 * \brief Plugin type enumeration
 */
enum plugin_type {
        PL_NONE,         /**< no plugin specified */
	PL_INPUT,        /**< input plugin */
	PL_INTERMEDIATE, /**< intermediate plugin */
	PL_OUTPUT,       /**< storage plugin */
};

/**
 * \brief Informations about plugin
 */
typedef struct conf_info_s {
        int force;               /**< force flag */
	char *name;				 /**< plugin name */
	char *sofile;			 /**< .so file path */
	char *thread;			 /**< thread name */
	enum plugin_type type;   /**< plugin type */
	xmlDocPtr doc;           /**< intermalcfg.xml file */
	xmlXPathContextPtr ctxt; /**< xml file context */
} conf_info_t;

/**
 * \brief Evaluate xml xpath
 * 
 * \param info tool configuration
 * \param tag plugin tag
 * \return xpath object
 */
xmlXPathObjectPtr eval_xpath(conf_info_t *info, char *tag);

/**
 * \brief Check if element exists
 * 
 * \param xpath_obj_file xpath object
 * \param name tag content
 * \param search tag name
 * \return 0 if element exists
 */
int check_exists(xmlXPathObjectPtr xpath_obj_file, char *name, char *search);

/**
 * \brief Get root node of xml
 * 
 * \param info tool configuration
 * \return root node
 */
xmlNodePtr get_root(conf_info_t *info);

/**
 * \brief Get plugin node
 * 
 * \param info tool configuration
 * \param tag plugin type tag
 * \param nametag name tag
 * \param nameval name value
 * \return plugin node
 */
xmlNodePtr get_plugin(conf_info_t *info, char *tag, char *nametag, char *nameval);

#endif /* IPFIXCONF_H */