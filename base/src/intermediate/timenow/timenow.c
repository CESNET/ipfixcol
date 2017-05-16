/**
 * \file timenow.c
 * \author Petr Velan <petr.velan@cesnet.cz>
 * \brief intermediate plugin for IPFIXcol
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

#include <ipfixcol.h>
#include <ipfixcol/intermediate.h>
#include <libxml/parser.h>
#include <libxml2/libxml/tree.h>

#include <string.h>

/* API version constant */
IPFIXCOL_API_VERSION;

/* Identifier for verbose macros */
static const char *msg_module = "timenow";

/**
 * \brief Plugin's configuration structure
 */
struct plugin_conf {
	void *ip_config;	/**< Intermediate process config */
};

/**
 * \brief Free configuration structure
 * 
 * \param[in] conf plugin's configuration
 */
void timenow_free_config(struct plugin_conf *conf)
{
	if (conf) {
		free(conf);
	}
}

/**
 * \brief Process startup configuration
 * 
 * \param[in] conf plugin configuration structure
 * \param[in] params configuration xml data
 * \return 0 on success
 */
int process_startup_xml(struct plugin_conf *conf, char *params)
{
	(void) conf;

	/* Load XML configuration */
	xmlDoc *doc = xmlParseDoc(BAD_CAST params);
	if (!doc) {
		MSG_ERROR(msg_module, "Unable to parse startup configuration!");
		return 1;
	}
	
	xmlNode *root = xmlDocGetRootElement(doc);
	if (!root) {
		MSG_ERROR(msg_module, "Cannot get document root element!");
		xmlFreeDoc(doc);
		return 1;
	}
	
	xmlFreeDoc(doc);
	return 0;
}

/**
 * \brief Plugin initialization
 * 
 * \param[in] params xml configuration
 * \param[in] ip_config	intermediate process config
 * \param[in] ip_id	intermediate process ID for template manager
 * \param[in] template_mgr template manager
 * \param[out] config config storage
 * \return 0 on success
 */
int intermediate_init(char* params, void* ip_config, uint32_t ip_id, struct ipfix_template_mgr* template_mgr, void** config)
{
	/* Suppress compiler warnings */
	(void) ip_id; (void) template_mgr;
	
	if (!params) {
		MSG_ERROR(msg_module, "Missing plugin's configuration");
		return 1;
	}
	
	/* Create configuration */
	struct plugin_conf *conf = calloc(1, sizeof(struct plugin_conf));
	if (!conf) {
		MSG_ERROR(msg_module, "Unable to allocate memory (%s:%d)", __FILE__, __LINE__);
		return 1;
	}
	
	/* Process configuration */
	if (process_startup_xml(conf, params) != 0) {
		timenow_free_config(conf);
		return 1;
	}
	
	/* Save configuration */
	conf->ip_config = ip_config;
	*config = conf;
	
	MSG_DEBUG(msg_module, "Initialized");
	return 0;
}

/**
 * \brief Update timestamps in flow records
 * 
 * \param[in] record record to work on
 * \param[in] time_diff number of milliseconds to be added to flow times
 * \return void
 */
void timenow_update_timestamps(struct ipfix_record *record, uint64_t time_diff)
{
	uint64_t *ts = NULL, *te = NULL;

	/* Get flow start time (element ID 152) */
	ts = (uint64_t *) data_record_get_field(record->record, record->templ, 0, 152, NULL);
	if (!ts) {
		return;
	}

	/* Get flow end time (element ID 153) */
	te = (uint64_t *) data_record_get_field(record->record, record->templ, 0, 153, NULL);
	if (!te) {
		return;
	}

	/* Do the conversion */
	*ts = htobe64(be64toh(*ts) + time_diff);
	*te = htobe64(be64toh(*te) + time_diff);
	
	return;
}

/**
 * \brief Process IPFIX message
 * 
 * \param[in] config plugin configuration
 * \param[in] message IPFIX message
 * \return 0 on success
 */
int intermediate_process_message(void* config, void* message)
{
	struct plugin_conf *conf = (struct plugin_conf *) config;
	struct ipfix_message *msg = (struct ipfix_message *) message;
	int flow_count = msg->data_records_count;

	/* Do nothing when there are no flow records */
	if (flow_count < 1) {
		pass_message(conf->ip_config, msg);
		return 0;
	}

	/* Compute number of milliseconds from packet export time */
	uint64_t time_diff = time(NULL) - ntohl(msg->pkt_header->export_time);
	time_diff *= 1000;

	/* Process each data record (except last) */
	for (int i = 0; i < flow_count - 1; ++i) {
		/* Prefetch future record now */
		char *p = msg->metadata[i+1].record.record;
		asm volatile ("prefetcht0 %[p]":[p] "+m"(*(volatile char *)p));

		/* Update timestamps */
		timenow_update_timestamps(&(msg->metadata[i].record), time_diff);
	}

	/* Do the last record here (no more prefetch) */
	timenow_update_timestamps(&(msg->metadata[flow_count - 1].record), time_diff);

	/* Pass message to the next plugin/Output Manager */
	pass_message(conf->ip_config, msg);
	return 0;
}

/**
 * \brief Close intermediate plugin
 * 
 * \param[in] config plugin configuration
 * \return 0 on success
 */
int intermediate_close(void *config)
{
	MSG_DEBUG(msg_module, "Closing");
	struct plugin_conf *conf = (struct plugin_conf *) config;
	
	/* Release configuration */
	timenow_free_config(conf);
	
	return 0;
}
