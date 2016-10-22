/**
 * \file dhcp.c
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

#include <sqlite3.h>
#include <string.h>

#define IP_MAC_PAIRS_MAX 16

/* API version constant */
IPFIXCOL_API_VERSION;

/* Identifier for verbose macros */
static const char *msg_module = "dhcp";

/**
 * \brief IPFIX element PEN and ID pair
 */
typedef struct {
	uint32_t en;
	uint16_t id;
} dhcp_ipfix_element_t;

/**
 * \brief IP-MAC pair
 */
typedef struct {
	dhcp_ipfix_element_t ip;
	dhcp_ipfix_element_t mac;
} dhcp_ip_mac_t;

/**
 * \brief Plugin's configuration structure
 */
struct plugin_conf {
	char mac[18];		/**< MAC field from DB */
	sqlite3 *db;		/**< DB config */
	char *db_path;		/**< Path to database file */
	void *ip_config;	/**< Intermediate process config */
	dhcp_ip_mac_t ip_mac_pairs[IP_MAC_PAIRS_MAX]; /**< IP-MAC pairs */
	uint8_t ip_mac_pairs_count; /**< IP-MAC pairs count*/
};

/**
 * \brief Free configuration structure
 * 
 * \param[in] conf plugin's configuration
 */
void dhcp_free_config(struct plugin_conf *conf)
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
		} else if (!xmlStrcasecmp(node->name, (const xmlChar *) "pair")) { /* IP-MAC pairs */

			if (conf->ip_mac_pairs_count >= IP_MAC_PAIRS_MAX) {
				MSG_WARNING(msg_module, "Too many IP-MAC pairs in configuration");
				continue;
			}

			/* Process IP-MAC pair */
			xmlNode *pair;
			for (pair = node->children; pair; pair = pair->next) {
				if (!xmlStrcasecmp(pair->name, (const xmlChar *) "ip")) {
					conf->ip_mac_pairs[conf->ip_mac_pairs_count].ip.en = atoi((char *) xmlGetProp(pair, BAD_CAST "en"));
					conf->ip_mac_pairs[conf->ip_mac_pairs_count].ip.id = atoi((char *) xmlGetProp(pair, BAD_CAST "id"));
				} else if (!xmlStrcasecmp(pair->name, (const xmlChar *) "mac")) {
					conf->ip_mac_pairs[conf->ip_mac_pairs_count].mac.en = atoi((char *) xmlGetProp(pair, BAD_CAST "en"));
					conf->ip_mac_pairs[conf->ip_mac_pairs_count].mac.id = atoi((char *) xmlGetProp(pair, BAD_CAST "id"));
				}
			}
			conf->ip_mac_pairs_count++;
		}
	}
	
	MSG_INFO(msg_module, "Found %u IP-MAC pairs", conf->ip_mac_pairs_count);

	if (!conf->db_path) {
		MSG_ERROR(msg_module, "Missing path to database file!");
		xmlFreeDoc(doc);
		return 1;
	}

	if (conf->ip_mac_pairs_count == 0) {
		MSG_ERROR(msg_module, "No IP-MAC pair found in configuration");
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
		dhcp_free_config(conf);
		return 1;
	}
	
	/* Open database */
	if (sqlite3_open(conf->db_path, &(conf->db))) {
		MSG_ERROR(msg_module, "Cannot open DHCP database: %s", sqlite3_errmsg(conf->db));
		dhcp_free_config(conf);
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
static int dhcp_callback(void *data, int argc, char **argv, char **azColName){
	struct plugin_conf *conf = (struct plugin_conf *) data;

	strncpy(conf->mac, argv[0], 17);

	return 0;
}

/**
 * \brief Replace existing MAC address with MAC from database
 * 
 * \param[in] conf plugin's configuration
 * \param[in] mdata data record's metadata
 * \param[in] ip_mac_pair IP-MAC pair
 * \return void
 */
void dhcp_replace_mac(struct plugin_conf *conf, struct metadata *mdata, dhcp_ip_mac_t *ip_mac_pair)
{
	void *ip_data = NULL, *mac_data = NULL;
	conf->mac[0] = '\0';
	
	/* Get IP address */
	ip_data = data_record_get_field(mdata->record.record, mdata->record.templ, ip_mac_pair->ip.en, ip_mac_pair->ip.id, NULL);
	if (!ip_data) {
		return;
	}

	/* Get MAC address pointer */
	mac_data = data_record_get_field(mdata->record.record, mdata->record.templ, ip_mac_pair->mac.en, ip_mac_pair->mac.id, NULL);
	if (!mac_data) {
		return;
	}

	/** Get the MAC from database **/

	char ipStr[INET_ADDRSTRLEN];
	
	/* Get IPv4 string */
	inet_ntop(AF_INET, ip_data, ipStr, INET_ADDRSTRLEN);
	
	char sqlQuery[150];
	snprintf(sqlQuery, 150, "SELECT mac FROM dhcp WHERE ip == '%s'", ipStr);

	/* Execute query */
	char *err = NULL;
	int rc = sqlite3_exec(conf->db, sqlQuery, dhcp_callback, (void *) conf, &err);
	
	if (rc != SQLITE_OK) {
		MSG_ERROR(msg_module, "SQL error: %s", err);
		sqlite3_free(err);
		return;
	}

	/* Check that we got a result */
	if (conf->mac[0] == '\0') {
		/* Write zeroes */
		uint64_t zero = 0;
		memcpy(mac_data, &zero, 6);
		return;
	}

	/* Convert MAC to number */
	unsigned char mac[6];
	sscanf(conf->mac, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);

	/* Fill the result back to the record */
	memcpy(mac_data, mac, 6);

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
	struct metadata *mdata;
	
	/* Process each data record */
	for (int i = 0; i < msg->data_records_count; ++i) {
		mdata = &(msg->metadata[i]);

		/* Replace MACs from database */
		for (int j=0; j < conf->ip_mac_pairs_count; j++) {
			dhcp_replace_mac(conf, mdata, &conf->ip_mac_pairs[j]);
			printf("ip: %u %u, mac %u %u\n", conf->ip_mac_pairs[j].ip.en, conf->ip_mac_pairs[j].ip.id, conf->ip_mac_pairs[j].mac.en, conf->ip_mac_pairs[j].mac.id);
		}
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
	dhcp_free_config(conf);
	
	return 0;
}
