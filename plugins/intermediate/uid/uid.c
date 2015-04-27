/**
 * \file uid.c
 * \author Michal Kozubik <kozubik@cesnet.cz>
 * \brief intermediate plugin for IPFIXcol
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

#include <ipfixcol.h>
#include <ipfixcol/intermediate.h>
#include <libxml/parser.h>
#include <libxml2/libxml/tree.h>

#include <sqlite3.h>
#include <string.h>

#define IPv4 4
#define IPv6 6

#define FIELD_IPV4_SRC 8
#define FIELD_IPV4_DST 12

#define FIELD_IPV6_SRC 27
#define FIELD_IPV6_DST 28

#define FLOW_START_SECONDS 150
#define FLOW_START_MILLISECONDS 152

/* API version constant */
IPFIXCOL_API_VERSION;

/* Identifier for verbose macros */
static const char *msg_module = "uid";

/**
 * \brief Plugin's configuration structure
 */
struct plugin_conf {
	char name[32];		/**< Name field from DB */
	sqlite3 *db;		/**< DB config */
	char *db_path;		/**< Path to database file */
	void *ip_config;	/**< intermediate process config */
};

/**
 * \brief Free configuration structure
 * 
 * \param[in] conf plugin's configuration
 */
void uid_free_config(struct plugin_conf *conf)
{
	if (conf) {
		/* Free path */
		if (conf->db_path) {
			free(conf->db_path);
		}
		
		/* Close database */
		if (conf->db) {
			sqlite3_close(conf->db);
		}
		
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
	
	xmlNode *node;
	for (node = root->children; node; node = node->next) {
		if (node->type != XML_ELEMENT_NODE) {
			continue;
		}

		/* Path to database file */
		if (!xmlStrcasecmp(node->name, (const xmlChar *) "path")) {
			conf->db_path = (char *) xmlNodeListGetString(doc, node->children, 1);
		}
	}
	
	if (!conf->db_path) {
		MSG_ERROR(msg_module, "Missing path to database file!");
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
		uid_free_config(conf);
		return 1;
	}
	
	/* Open database */
	if (sqlite3_open(conf->db_path, &(conf->db))) {
		MSG_ERROR(msg_module, "Cannot open UID database: %s", sqlite3_errmsg(conf->db));
		uid_free_config(conf);
		return 1;
	}
	
	/* Save configuration */
	conf->ip_config = ip_config;
	*config = conf;
	
	MSG_DEBUG(msg_module, "Initialized");
	return 0;
}

/**
 * \brief Callback for each select
 * 
 * \param[in] data Plugin configuration
 * \param[in] argc
 * \param[in] argv DB fields
 * \param[in] azColName
 * \return 0 on success
 */
static int uid_callback(void *data, int argc, char **argv, char **azColName){
	/*
		item                argv[x]:
		id = 1                  0
		name = Paul             1
		ip = 192.168.1.1        2
		action = 1              3
		time = 1415204353       4
	*/

	struct plugin_conf *conf = (struct plugin_conf *) data;

	if (strcmp(argv[3], "1") == 0) {
		strncpy(conf->name, argv[1], 31);
	} else {
		strncpy(conf->name, "", 31);
	}

	return 0;
}

/**
 * \brief Convert IPv6 address to expanded form
 * 
 * \param[out] str Expanded address
 * \param[in] addr ipv6 address
 */
void convertToExpandedIPv6(char *str, const struct in6_addr *addr) {
	sprintf(str, "%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x",
			(int)addr->s6_addr[0], (int)addr->s6_addr[1],
			(int)addr->s6_addr[2], (int)addr->s6_addr[3],
			(int)addr->s6_addr[4], (int)addr->s6_addr[5],
			(int)addr->s6_addr[6], (int)addr->s6_addr[7],
			(int)addr->s6_addr[8], (int)addr->s6_addr[9],
			(int)addr->s6_addr[10], (int)addr->s6_addr[11],
			(int)addr->s6_addr[12], (int)addr->s6_addr[13],
			(int)addr->s6_addr[14], (int)addr->s6_addr[15]);
}

/**
 * \brief Get user informations for given data record and given address (source or destination)
 * 
 * \param[in] conf plugin's configuration
 * \param[in] mdata data record's metadata
 * \param[in] ipv4_field IPv4 field
 * \param[in] ipv6_field IPv6 alternative
 * \param[in] flow_start Flow start time
 * \return country code
 */
uint16_t uid_get_user_info(struct plugin_conf *conf, struct metadata *mdata, int ipv4_field, int ipv6_field, uint32_t flow_start)
{
	void *data = NULL;
	conf->name[0] = '\0';
	
	/* Get address */
	int ipv = IPv4;
	data = data_record_get_field(mdata->record.record, mdata->record.templ, 0, ipv4_field, NULL);
	if (!data) {
		data = data_record_get_field(mdata->record.record, mdata->record.templ, 0, ipv6_field, NULL);
		ipv = IPv6;
	}
	
	if (!data) {
		return 0;
	}
	
	char ipStr[INET6_ADDRSTRLEN];
	
	/* Get country ID */
	if (ipv == IPv4) {
		inet_ntop(AF_INET, data, ipStr, INET_ADDRSTRLEN);
	} else {
		convertToExpandedIPv6(ipStr, data);
	}
	
	char sqlQuery[150];
	snprintf(sqlQuery, 150, "SELECT * from logs WHERE ip == '%s' and time <= %u ORDER BY time DESC LIMIT 1", ipStr, flow_start);

	/* Execute query */
	char *err = NULL;
	int rc = sqlite3_exec(conf->db, sqlQuery, uid_callback, (void *) conf, &err);
	
	if (rc != SQLITE_OK) {
		MSG_ERROR(msg_module, "SQL error: %s", err);
		sqlite3_free(err);
	}
	
	conf->name[31] = '\0';
	
	return 0;
}

/**
 * \brief Get flow start time in seconds
 *
 * \param[in] record Data record
 * \return flow start time
 */
uint32_t get_flow_start(struct ipfix_record *record)
{
	/* Get time in milliseconds */
	void *data = data_record_get_field(record->record, record->templ, 0, FLOW_START_MILLISECONDS, NULL);
	if (data) {
		return (uint32_t) be64toh(*((uint64_t*) data)) / 1000;
	}

	/* Time in milliseconds not found, try seconds */
	data = data_record_get_field(record->record, record->templ, 0, FLOW_START_SECONDS, NULL);
	if (data) {
		return ntohl(*((uint32_t*) data));
	}

	/* Unknown flow start time */
	return time(NULL);
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
	
	struct metadata *mdata;
	
	/* Process each data record */
	for (int i = 0; i < msg->data_records_count; ++i) {
		mdata = &(msg->metadata[i]);

		uint32_t flowStart = get_flow_start(&(msg->metadata->record));

		/* Fill user names */
		uid_get_user_info(conf, mdata, FIELD_IPV4_SRC, FIELD_IPV6_SRC, flowStart);
		strncpy(mdata->srcName, conf->name, 32);
		
		uid_get_user_info(conf, mdata, FIELD_IPV4_DST, FIELD_IPV6_DST, flowStart);
		strncpy(mdata->dstName, conf->name, 32);
	}
	
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
	uid_free_config(conf);
	
	return 0;
}
