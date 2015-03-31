/**
 * \file remover.c
 * \author Michal Kozubik <kozubik@cesnet.cz>
 * \brief Tool for editing IPFIXcol internalcfg.xml
 *
 * Copyright (C) 2014 CESNET, z.s.p.o.
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

#include "remover.h"
#include "ipfixconf.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <libxml/xpathInternals.h>

/**
 * \brief Remove plugin from configuration file
 * 
 * \param info tool configuration
 * \param tag plugin type tag
 * \param nametag
 * \return 0 on success
 */
int remove_pl(conf_info_t *info, char *tag, char *nametag)
{
	xmlNodePtr plug = NULL;
	
	/* check if plugin exists */
	plug = get_plugin(info, tag, nametag, info->name);
	
	if (plug == NULL) {
		fprintf(stderr, "Plugin '%s' does not exists!\n", info->name);
		return 0;
	}
	
	/* remove node */
	xmlUnlinkNode(plug);
	xmlFreeNode(plug);
	
	return 0;
}

/**
 * \brief Remove input plugin from supportedCollectors tag
 * 
 * \param info tool configuration
 * \return 0 on success
 */
int remove_supported(conf_info_t *info)
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
				xmlUnlinkNode(children1);
				xmlFreeNode(children1);
				return 0;
			}
			children1 = children1->next;
		}
	}
	
	return 0;
}

int remove_plugin(conf_info_t *info)
{
	int ret = 0;
	
	/* check if everything is set */
	if (info->type == PL_NONE || !info->name) {
		fprintf(stderr, "Missing option '%s'\n", 
			(info->type == PL_NONE) ? "-p" : "-n");
		return 1;
	}
	
	/* remove plugin */
	switch (info->type) {
	case PL_INPUT:
		remove_supported(info);
		ret = remove_pl(info, TAG_INPUT, "name");
		break;
	case PL_INTERMEDIATE:
		ret = remove_pl(info, TAG_INTER, "name");
		break;
	case PL_OUTPUT:
		ret = remove_pl(info, TAG_OUTPUT, "fileFormat");
		break;
	default:
		break;
	}
	
	return ret;
}