/**
 * \file adder.c
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

#include "adder.h"
#include "ipfixconf.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <libxml/xpathInternals.h>

/**
 * \brief Update plugin informations
 * 
 * \param plug Plugin node
 * \param to_add New informations
 * \param to_search tag names
 * \return 0 on success
 */
int update_plugin(xmlNodePtr plug, char **to_add, char **to_search)
{
	xmlNodePtr children = plug->children;
	int i;
	
	while (children) {
		for (i = 0; i < ITEMS_CNT; ++i) {
			if (!strncmp ((char*) children->name, to_search[i], strlen (to_search[i]) + 1)) {
				/* element found*/
				xmlNodeSetContent(children, (xmlChar *) to_add[i]);
			}
			children = children->next;
		}
	}
	
	return 0;
}

/**
 * \brief Add plugin to configuration
 * 
 * \param info tool configuration
 * \param tag  plugin type tag
 * \param to_add elements contents
 * \param to_search elements tags
 * \return 0 on success
 */
int add(conf_info_t *info, char *tag, char **to_add, char **to_search)
{
	xmlNodePtr root = NULL, plug = NULL;
	xmlNodePtr subtags[ITEMS_CNT] = {0};
	int i;
	
	/* check if plugin already exists */
	plug = get_plugin(info, tag, to_search[0], to_add[0]);
	
	if (plug) {
		if (info->force) {
			update_plugin(plug, to_add, to_search);
			return 0;
		} else {
			fprintf(stderr, "Plugin '%s' already exists!\n", to_add[0]);
			return 1;
		}
	}
	
	/* get root */
	root = get_root(info);
	if (!root) {
		return 1;
	}
	
	/* plugin tag */
	plug = xmlNewNode(0, (xmlChar *) tag);
	for (i = 0; i < ITEMS_CNT; ++i) {
		subtags[i] = xmlNewNode(0, (xmlChar *) to_search[i]);
		xmlNodeSetContent(subtags[i], (xmlChar *) to_add[i]);
		xmlAddChild(plug, subtags[i]);
	}
	
	xmlAddChild(root, plug);
	
	return 0;
}

/**
 * \brief Add input plugin to supportedCollectors tag
 * 
 * \param info tool configuration
 * \return 0 on success
 */
int add_supported(conf_info_t *info)
{
	int i;
	xmlNodePtr children1;
	xmlXPathObjectPtr xpath_obj_file = eval_xpath(info, TAG_SUPPORTED);
	if (!xpath_obj_file) {
		return 1;
	}
	
	for (i = 0; i < xpath_obj_file->nodesetval->nodeNr; i++) {
		children1 = xpath_obj_file->nodesetval->nodeTab[i]->children;
		while (children1) {
			if ((!strncmp ((char*) children1->name, "name", strlen ("name") + 1))
			        && (!xmlStrncmp (children1->children->content, (xmlChar *) info->name, xmlStrlen ((xmlChar *)info->name) + 1))) {
				/* element found*/
				xmlXPathFreeObject(xpath_obj_file);
				return 0;
			}
			children1 = children1->next;
		}
	}
	
	children1 = xmlNewNode(0, (xmlChar *) "name");
	xmlNodeSetContent(children1, (xmlChar *) info->name);
	xmlAddChild(xpath_obj_file->nodesetval->nodeTab[0], children1);
	
	xmlXPathFreeObject(xpath_obj_file);

	return 0;
}

/**
 * \brief Add input plugin
 * 
 * \param info tool configuration
 * \return 0 on success
 */
int add_input(conf_info_t *info)
{
	char *to_search[ITEMS_CNT] = {"name", "file", "processName"};
	char *to_add[ITEMS_CNT];
	to_add[0] = info->name;
	to_add[1] = info->sofile;
	to_add[2] = info->thread;
	
	add_supported(info);
	return add(info, TAG_INPUT, to_add, to_search);
}

/**
 * \brief Add output plugin
 * 
 * \param info tool configuration
 * \return 0 on success
 */
int add_output(conf_info_t *info)
{
	char *to_search[ITEMS_CNT] = {"fileFormat", "file", "threadName"};
	char *to_add[ITEMS_CNT];
	to_add[0] = info->name;
	to_add[1] = info->sofile;
	to_add[2] = info->thread;
	
	return add(info, TAG_OUTPUT, to_add, to_search);
}

/**
 * \brief Add intermediate plugin
 * 
 * \param info tool configuration
 * \return 0 on success
 */
int add_intermediate(conf_info_t *info)
{
	char *to_search[ITEMS_CNT] = {"name", "file", "threadName"};
	char *to_add[ITEMS_CNT];
	to_add[0] = info->name;
	to_add[1] = info->sofile;
	to_add[2] = info->thread;
	
	return add(info, TAG_INTER, to_add, to_search);
}

/**
 * \brief Add new plugin
 * 
 * \param info tool configuration
 */
int add_plugin(conf_info_t *info)
{
	int ret = 0;
	
	/* check if everything is set */
	if (info->type == PL_NONE || !info->name || !info->sofile || !info->thread) {
		fprintf(stderr, "Missing option '%s'\n", 
			(info->type == PL_NONE) ? "-p" : !info->name ? "-n" : !info->sofile ? "-f" : "-t");
		return 1;
	}
	
	/* add plugin */
	switch (info->type) {
	case PL_INPUT:
		ret = add_input(info);
		break;
	case PL_INTERMEDIATE:
		ret = add_intermediate(info);
		break;
	case PL_OUTPUT:
		ret = add_output(info);
		break;
	default:
		break;
	}
	
	return ret;
}

