/**
 * \file dummy_output.c
 * \author Petr Velan <petr.velan@cesnet.cz>
 * \brief Storage plugin that does not write any data, used for tests
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

/**
 * \defgroup dummyStoragePlugin Storage plugin for testing that does not write any data
 * \ingroup storagePlugins
 *
 * @{
 */

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

#include "ipfixcol.h"

/* API version constant */
IPFIXCOL_API_VERSION;

/** Identifier to MSG_* macros */
static char *msg_module = "dummy storage";

struct dummy_config {
	int delay;	  /**< how long should store_packet sleep in us */
};

/**
 * \brief Storage plugin initialization.
 *
 * \param[in] params parameters for this storage plugin
 * \param[out] config the plugin specific configuration structure
 * \return 0 on success, negative value otherwise
 */
int storage_init(char *params, void **config)
{
	MSG_INFO(msg_module, "Dummy plugin: storage_init called");

	struct dummy_config *conf;
	xmlDocPtr doc;
	xmlNodePtr cur;

	/* allocate space for config structure */
	conf = (struct dummy_config *) malloc(sizeof(*conf));
	if (conf == NULL) {
		MSG_ERROR(msg_module, "Not enough memory (%s:%d)", __FILE__, __LINE__);
		return -1;
	}

	/* try to parse configuration file */
	doc = xmlReadMemory(params, strlen(params), "nobase.xml", NULL, 0);
	if (doc == NULL) {
		MSG_ERROR(msg_module, "Plugin configuration parsing failed");
		goto err_read_conf;
	}
	cur = xmlDocGetRootElement(doc);
	if (cur == NULL) {
		MSG_ERROR(msg_module, "Empty configuration");
		goto err_init;
	}
	if (xmlStrcmp(cur->name, (const xmlChar *) "fileWriter")) {
		MSG_ERROR(msg_module, "Root node != fileWriter");
		goto err_init;
	}
	
	/* default delay */
	conf->delay = 0;

	cur = cur->xmlChildrenNode;
	while (cur != NULL) {
		/* find out the desired delay */
		if ((!xmlStrcmp(cur->name, (const xmlChar *) "delay"))) {
			conf->delay = atoi((char *) xmlNodeListGetString(doc, cur->xmlChildrenNode, 1));
			break;
		}
		cur = cur->next;
	}

	MSG_INFO(msg_module, "Dummy plugin: delay set to %ius", conf->delay);

	/* we don't need this xml tree anymore */
	xmlFreeDoc(doc);

	/* pass config to core */
	*config = conf;

	return 0;
	
	err_init:
	xmlFreeDoc(doc);
	err_read_conf:
	free(conf);
	return -1;
}

int store_packet(void *config, const struct ipfix_message *ipfix_msg,
	 const struct ipfix_template_mgr *template_mgr)
{
	(void) ipfix_msg;
	(void) template_mgr;
	struct dummy_config *conf = (struct dummy_config*) config;
	usleep(conf->delay);
	return 0;
}

int store_now(const void *config)
{
	(void) config;
	return 0;
}

/**
 * \brief Remove storage plugin.
 *
 * This function is called when we don't want to use this storage plugin
 * anymore. All it does is that it cleans up after the storage plugin.
 *
 * \param[in] config the plugin specific configuration structure
 * \return 0 on success, negative value otherwise
 */
int storage_close(void **config)
{
	MSG_INFO(msg_module, "Dummy plugin: storage_close called\n");

	free(*config);
	*config = NULL;

	return 0;
}

/**@}*/

