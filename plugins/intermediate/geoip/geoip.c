/**
 * \file geoip.cpp
 * \author Michal Kozubik <kozubik@cesnet.cz>
 * \brief intermediate plugin for geolocation
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
#include <libxml/tree.h>

#include <GeoIP.h>
#include <geoip.h>
#include "countrycode.h"

#define IPv4 4
#define IPv6 6

#define FIELD_IPV4_SRC 8
#define FIELD_IPV4_DST 12

#define FIELD_IPV6_SRC 27
#define FIELD_IPV6_DST 28

/* API version constant */
IPFIXCOL_API_VERSION;

/* Identifier for verbose macros */
static const char *msg_module = "geoip";

/**
 * \brief Plugin's configuration structure
 */
struct geoip_conf {
	void *ip_config;	/**< intermediate process config */
	char *path;			/**< path to database file */
	char *path6;		/**< path to IPv6 database file */
	GeoIP *country_db;	/**< MaxMind GeoIP DB */
	GeoIP *country_db6;	/**< IPv6 version */
};

/**
 * \brief Free configuration structure
 * 
 * \param[in] conf plugin's configuration
 */
void geoip_free_config(struct geoip_conf *conf)
{
	if (conf) {
		/* Close databases */
		if (conf->country_db) {
			GeoIP_delete(conf->country_db);
		}
		
		if (conf->country_db6) {
			GeoIP_delete(conf->country_db6);
		}
		
		/* Free paths */
		if (conf->path) {
			free(conf->path);
		}
		
		if (conf->path6) {
			free(conf->path6);
		}
		
		GeoIP_cleanup();
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
int process_startup_xml(struct geoip_conf *conf, char *params)
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
	
	/* Get database path */
	xmlNode *node;
	for (node = root->children; node; node = node->next) {
		if (node->type != XML_ELEMENT_NODE) {
			continue;
		}
		
		if (!xmlStrcmp(node->name, (const xmlChar *) "path")) {
			conf->path = (char *) xmlNodeListGetString(doc, node->children, 1);
		} else if (!xmlStrcmp(node->name, (const xmlChar *) "path6")) {
			conf->path6 = (char *) xmlNodeListGetString(doc, node->children, 1);
		} else {
			MSG_WARNING(msg_module, "Unknown element %s", (char *) node->name);
		}
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
	struct geoip_conf *conf = calloc(1, sizeof(struct geoip_conf));
	if (!conf) {
		MSG_ERROR(msg_module, "Unable to allocate memory (%s:%d)", __FILE__, __LINE__);
		return 1;
	}
	
	/* Process configuration */
	if (process_startup_xml(conf, params) != 0) {
		geoip_free_config(conf);
		return 1;
	}
	
	/* Initialize IPv4 GeoIP database */
	if (conf->path) {
		conf->country_db = GeoIP_open(conf->path, GEOIP_MEMORY_CACHE);
	} else {
		conf->country_db = GeoIP_new(GEOIP_MEMORY_CACHE);
	}
	
	if (!conf->country_db) {
		MSG_ERROR(msg_module, "Error while opening GeoIP database");
		geoip_free_config(conf);
		return 1;
	}
	
	/* Initialize IPv6 GeoIP database */
	if (conf->path6) {
		conf->country_db6 = GeoIP_open(conf->path6, GEOIP_MEMORY_CACHE);
	} else {
		conf->country_db6 = GeoIP_open(GEOIPV6_DAT, GEOIP_MEMORY_CACHE);
//		conf->country_db6 = GeoIP_open_type(GEOIP_COUNTRY_EDITION_V6, GEOIP_MEMORY_CACHE);
	}
	
	if (!conf->country_db6) {
		MSG_ERROR(msg_module, "Error while opening GeoIPv6 database");
		geoip_free_config(conf);
		return 1;
	}
	
	/* Save configuration */
	conf->ip_config = ip_config;
	*config = conf;
	
	MSG_DEBUG(msg_module, "Initialized");
	return 0;
}

/**
 * \brief Get country code for given data record and given address (source or destination)
 * 
 * \param[in] conf plugin's configuration
 * \param[in] mdata data record's metadata
 * \param[in] ipv4_field IPv4 field
 * \param[in] ipv6_field IPv6 alternative
 * \return country code
 */
uint16_t geoip_get_country_code(struct geoip_conf *conf, struct metadata *mdata, int ipv4_field, int ipv6_field)
{
	void *data;
	
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
	
	/* Country ID */
	uint16_t id = 0;
	
	/* Get country ID */
	if (ipv == IPv4) {
		id = GeoIP_id_by_ipnum(conf->country_db, *((uint32_t *) data));
	} else {
		/* IPv6 address */
		uint32_t *addr = (uint32_t *) data;
		
		geoipv6_t ipnum;
		ipnum.s6_addr32[0] = htonl(addr[3]);
		ipnum.s6_addr32[1] = htonl(addr[2]);
		ipnum.s6_addr32[2] = htonl(addr[1]);
		ipnum.s6_addr32[3] = htonl(addr[0]);
		
		id = GeoIP_id_by_ipnum_v6(conf->country_db6, ipnum);
	}
	
	/* Return numeric code */
	return iso3166_GeoIP_country_codes[id].num_code;
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
	struct geoip_conf *conf = (struct geoip_conf *) config;
	struct ipfix_message *msg = (struct ipfix_message *) message;
	
	struct metadata *mdata;
	
	/* Process each data record */
	for (int i = 0; i < msg->data_records_count; ++i) {
		mdata = &(msg->metadata[i]);
		
		/* Fill country codes */
		mdata->srcCountry = geoip_get_country_code(conf, mdata, FIELD_IPV4_SRC, FIELD_IPV6_SRC);
		mdata->dstCountry = geoip_get_country_code(conf, mdata, FIELD_IPV4_DST, FIELD_IPV6_DST);
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
	struct geoip_conf *conf = (struct geoip_conf *) config;
	
	/* Release configuration */
	geoip_free_config(conf);
	
	return 0;
}
