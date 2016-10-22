/**
 * \file stats.cpp
 * \author Michal Kozubik <kozubik@cesnet.cz>
 * \brief intermediate plugin for profiling data
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

extern "C" {
#include <ipfixcol.h>

/* API version constant */
IPFIXCOL_API_VERSION;
}

#include <libxml2/libxml/xpath.h>
#include <libxml2/libxml/parser.h>
#include <libxml2/libxml/tree.h>
#include <rrd.h>
#include <sys/stat.h>

#include "stats.h"

#include <iostream>
#include <sstream>
#include <vector>
#include <stdexcept>

/* Identifier for verbose macros */
static const char *msg_module = "stats";

/* some auxiliary functions for extracting data of exact length */
#define read8(ptr) (*((uint8_t *) (ptr)))
#define read16(ptr) (*((uint16_t *) (ptr)))
#define read32(ptr) (*((uint32_t *) (ptr)))
#define read64(ptr) (*((uint64_t *) (ptr)))

/*
 * Statistics fields
 *
 * number of this fields must equal to the GROUPS * PROTOCOLS_PER_GROUP
 *
 * GROUPS = flows, packets, traffic
 * PROTOCOLS = total, tcp, udp, icmp, other
 *
*/
static const std::vector<const char*> fields{
	"flows",	"flows_tcp",	"flows_udp",	"flows_icmp",	"flows_other",
	"packets",	"packets_tcp",	"packets_udp",	"packets_icmp",	"packets_other",
	"traffic",	"traffic_tcp",	"traffic_udp",	"traffic_icmp",	"traffic_other"
};

/**
 * \brief Process startup configuration
 *
 * \param[in] conf plugin configuration structure
 * \param[in] params configuration xml data
 */
void process_startup_xml(plugin_conf *conf, char *params)
{	
	xmlChar *aux_char;
	
	/* Load XML configuration */
	xmlDoc *doc = xmlParseDoc(BAD_CAST params);
	if (!doc) {
		throw std::invalid_argument("Cannot parse config xml");
	}
	
	xmlNode *root = xmlDocGetRootElement(doc);
	if (!root) {
		throw std::invalid_argument("Cannot get document root element!");
	}
	
	/* Set default interval */
	conf->interval = DEFAULT_INTERVAL;

	/* Iterate throught all elements */
	for (xmlNode *node = root->children; node; node = node->next) {
		if (node->type != XML_ELEMENT_NODE) {
			continue;
		}
		
		if (!xmlStrcmp(node->name, (const xmlChar *) "path")) {
			/* Path to RRD files */
			aux_char = xmlNodeListGetString(doc, node->children, 1);
			conf->path = (const char *) aux_char;
			xmlFree(aux_char);
		} else if (!xmlStrcmp(node->name, (const xmlChar *) "interval")) {
			/* Statistics interval */
			aux_char = xmlNodeListGetString(doc, node->children, 1);
			conf->interval = atoi((const char *) aux_char);
			xmlFree(aux_char);
		}
	}
	
	/* Check if we have path to RRD folder */
	if (conf->path.empty()) {
		xmlFreeDoc(doc);
		throw std::invalid_argument("Path to RRD files must be set!");
	}
	
	/* Free resources */
	xmlFreeDoc(doc);
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
int intermediate_init(char* params, void* ip_config, uint32_t ip_id, ipfix_template_mgr* template_mgr, void** config)
{	
	/* Suppress compiler warning */
	(void) ip_id; (void) template_mgr;
	
	if (!params) {
		MSG_ERROR(msg_module, "Missing plugin configuration");
		return 1;
	}
	
	try {
		/* Create configuration */
		plugin_conf *conf = new plugin_conf;
		
		/* Process params */
		process_startup_xml(conf, params);


		/* Create RRD template */
		for (uint16_t i = 0; i < fields.size(); ++i) {
			if (i > 0) {
				conf->templ += ":";
			}
			conf->templ += fields[i];
		}

		/* Save configuration */
		conf->ip_config = ip_config;
		*config = conf;
		
	} catch (std::exception &e) {
		*config = NULL;
		MSG_ERROR(msg_module, "%s", e.what());
		return 1;
	}

	MSG_DEBUG(msg_module, "initialized");
	return 0;
}

/**
 * \brief Create new RRD database
 *
 * \param[in] conf plugin configuration
 * \param[in] file path to RRD file
 * \return stats_data structure
 */
stats_data *stats_rrd_create(plugin_conf *conf, std::string file)
{
	/* Create stats counters */
	stats_data *stats = new stats_data;
	stats->file = file;
	stats->last = time(NULL);

	for (int group = 0; group < GROUPS; ++group) {
		for (int field = 0; field < PROTOCOLS_PER_GROUP; ++field) {
			stats->fields[group][field] = 0;
		}
	}

	/* Create file */
	struct stat sts;
	if (!(stat(file.c_str(), &sts) == -1 && errno == ENOENT)) {
		/* File already exists */
		return stats;
	}

	char buffer[64];

	/* Create arguments field */
	std::vector<std::string> argv{};

	/* Create file */
	argv.push_back("create");
	argv.push_back(file);

	/*
	 * Set start time
	 * time is decreased by conf->interval because it is not possible to
	 * update the RRD for the next step time.
	 */
	snprintf(buffer, 64, "--start=%lu", stats->last - conf->interval);
	argv.push_back(buffer);

	/* Set interval */
	snprintf(buffer, 64, "--step=%d", conf->interval);
	argv.push_back(buffer);

	/* Add all fields */
	for (auto field: fields) {
		snprintf(buffer, 64, "DS:%s:ABSOLUTE:%u:U:U", field, conf->interval * 2); /* datasource definition, wait 2x the interval for data */
		argv.push_back(buffer);
	}

	/* Add statistics */
	argv.push_back("RRA:AVERAGE:0.5:1:51840"); /* 1 x 5min =  5 min samples 6 * 30 * 288 = 51840 => 6 * 30 days */
	argv.push_back("RRA:AVERAGE:0.5:6:8640"); /* 6 x 5min = 30 min samples 6 * 30 *  48 = 8640  => 6 * 30 day */
	argv.push_back("RRA:AVERAGE:0.5:24:2160"); /* 24 x 5min = 2 hour samples 6 * 30 *  12 = 2160  => 6 * 30 days */
	argv.push_back("RRA:AVERAGE:0.5:288:1825"); /* 288 x 5min = 1 day samples 5 * 365 *   1 = 1825  => 5 * 365 days */
	argv.push_back("RRA:MAX:0.5:1:51840");
	argv.push_back("RRA:MAX:0.5:6:8640");
	argv.push_back("RRA:MAX:0.5:24:2160");
	argv.push_back("RRA:MAX:0.5:288:1825");


	/* Create C style argv */
	const char **c_argv = new const char*[argv.size()];
	for (u_int16_t i = 0; i < argv.size(); ++i) {
		c_argv[i] = argv[i].c_str();
	}

	/* Create RRD database */
	if (rrd_create(argv.size(), (char **) c_argv)) {
		MSG_ERROR(msg_module, "Create RRD DB Error: %s", rrd_get_error());
		rrd_clear_error();
		delete stats;
		return NULL;
	}

	return stats;
}

/**
 * \brief Convert stats counters to string
 *			!! Also resets counters !!
 *
 * \param[in] last	update time
 * \param[in] fields stats fields
 * \return counters converted to string
 */
std::string stats_counters_to_string(uint64_t last, uint64_t fields[GROUPS][PROTOCOLS_PER_GROUP])
{
	std::stringstream ss;

	/* Add update time */
	ss << last;

	/* Go through all groups */
	for (int group = 0; group < GROUPS; ++group) {

		/* Add group separator */
		ss << ":";

		/* Go through all fields in group */
		for (int field = 0; field < PROTOCOLS_PER_GROUP; ++field) {
			/* Add field separator */
			if (field > 0) {
				ss << ":";
			}

			/* Add field */
			ss << fields[group][field];

			/* Reset counter */
			fields[group][field] = 0;
		}
	}

	return ss.str();
}

/**
 * \brief Update RRD stats file
 *
 * \param[in] stats Stats data
 * \param[in] templ RRD template
 */
void stats_update(stats_data *stats, std::string templ)
{
	std::vector<std::string> argv;

	/* Set RRD file */
	argv.push_back("update");
	argv.push_back(stats->file);

	/* Set template */
	argv.push_back("--template");
	argv.push_back(templ);

	/* Add counters */
	argv.push_back(stats_counters_to_string(stats->last, stats->fields));

	/* Create C style argv */
	const char **c_argv = new const char*[argv.size()];
	for (u_int16_t i = 0; i < argv.size(); ++i) {
		c_argv[i] = argv[i].c_str();
	}

	/* Update database */
	if (rrd_update(argv.size(), (char **) c_argv)) {
		MSG_ERROR(msg_module, "RRD Insert Error: %s", rrd_get_error());
		rrd_clear_error();
	}

	delete[] c_argv;
}

/**
 * \brief Create path to the rrd file on filesystem
 *
 * \param[in] file path with %o field
 * \param[in] odid Observation Domain ID
 * \return path to the file
 */
std::string stats_create_file(std::string path, uint32_t odid)
{
	std::stringstream ss;

	ss << odid;
	std::string domain_id = ss.str();

	size_t o_loc = path.find("%o");

	if (o_loc == std::string::npos) {
		if (path[path.size() - 1] != '/') {
			path += "/";
		}

		path += domain_id;
	} else {
		path.replace(o_loc, 2, domain_id);
	}

	/* Create directory */
	size_t last_slash = path.find_last_of("/");
	std::string command = "mkdir -p \"" + path.substr(0, last_slash) + "\"";

	system(command.c_str());

	return path;
}

/**
 * \brief Find or create RRD stats file for given ODID
 *
 * \param[in] conf plugin's configuration
 * \param[in] odid Observation Domain ID
 * \return stats data
 */
stats_data *stats_get_rrd_file(plugin_conf *conf, uint32_t odid)
{
	stats_data *stats = conf->stats[odid];

	if (stats) {
		/* RRD stats found */
		return stats;
	}

	/* Create new RRD file */
	std::string file = stats_create_file(conf->path, odid);

	stats = stats_rrd_create(conf, file);
	conf->stats[odid] = stats;

	return stats;
}

/**
 * \brief Converts IPFIX protocolIdentifier to stats protocol
 *
 * \param[in] rec Data record
 * \return stats protocol
 */
enum st_protocol stats_get_proto(ipfix_record *rec)
{
	/* Get protocolIdentifier */
	uint8_t *data = data_record_get_field((uint8_t*) rec->record, rec->templ, 0, PROTOCOL_ID, NULL);
	int ipfix_proto = data ? (*((uint8_t *) (data))) : 0;

	/* Decode value */
	switch (ipfix_proto) {
	case IG_UDP:	return UDP;
	case IG_TCP:	return TCP;
	case IG_ICMP:	
	case IG_ICMPv6:	return ICMP;
	default:		return OTHER;
	}
}

/**
 * \brief Get field value
 *
 * \param[in] rec Data record
 * \param[in] field_id	Field ID
 * \return field value (or zero if not found)
 */
uint64_t stats_field_val(ipfix_record *rec, int field_id)
{
	int dataSize = 0;
	uint8_t *data = data_record_get_field((uint8_t*) rec->record, rec->templ, 0, field_id, &dataSize);

	/* Field not found */
	if (!data) {
		return 0;
	}

	/* Convert value to host byte order */
	switch (dataSize) {
	case 1: return read8(data);
	case 2: return ntohs(read16(data));
	case 4: return ntohl(read32(data));
	case 8: return be64toh(read64(data));
	default: return 0;
	}
}

/**
 * \brief Update stats counters
 *
 * \param[in] stats stats data
 * \param[in] mdata Data record's metadata
 */
void stats_update_counters(stats_data *stats, metadata *mdata)
{
	/* Get stats values  */
	uint64_t packets = stats_field_val(&(mdata->record), PACKETS_ID);
	uint64_t traffic = stats_field_val(&(mdata->record), TRAFFIC_ID);

	/* Decode protocol */
	enum st_protocol proto = stats_get_proto(&(mdata->record));

	/* Update total stats */
	stats->fields[PACKETS][TOTAL]	+= packets;
	stats->fields[TRAFFIC][TOTAL]	+= traffic;
	stats->fields[FLOWS][TOTAL]		+= 1;

	/* Update protocol's stats */
	stats->fields[PACKETS][proto]	+= packets;
	stats->fields[TRAFFIC][proto]	+= traffic;
	stats->fields[FLOWS][proto]		+= 1;
}

/**
 * \brief Update RRD files if interval passed
 *
 * \param[in] conf plugin config
 * \param[in] force ignore interval, always update files
 */
void stats_flush_counters(plugin_conf *conf, bool force = false)
{
	/* Update stats */
	uint64_t now = time(NULL);
	for (auto st: conf->stats) {
		/* Some pointers can be NULL after unsuccessfull creation */
		if (!st.second) {
			continue;
		}

		if (force || ((st.second->last / conf->interval + 1) * conf->interval <= now)) {
			stats_update(st.second, conf->templ);
			st.second->last = now;
		}
	}
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
	plugin_conf *conf = reinterpret_cast<plugin_conf *>(config);
	struct ipfix_message *msg = reinterpret_cast<struct ipfix_message *>(message);

	/* Catch closing message */
	if (msg->source_status == SOURCE_STATUS_CLOSED) {
		pass_message(conf->ip_config, msg);
		return 0;
	}

	/* Update counters */
	stats_flush_counters(conf);

	/* Process message */
	stats_data *stats = stats_get_rrd_file(conf, htonl(msg->pkt_header->observation_domain_id));
	if (!stats) {
		/* Error occured */
		goto err;
	}

	/* Update counters */
	for (uint16_t i = 0; i < msg->data_records_count; ++i) {
		stats_update_counters(stats, &(msg->metadata[i]));
	}

	pass_message(conf->ip_config, msg);
	return 0;

err:
	pass_message(conf->ip_config, msg);
	return 1;
}

/**
 * \brief Close intermediate plugin
 *
 * \param[in] config plugin configuration
 * \return 0 on success
 */
int intermediate_close(void *config)
{
	MSG_DEBUG(msg_module, "CLOSING");
	plugin_conf *conf = static_cast<plugin_conf*>(config);
	
	/* Force update counters */
	stats_flush_counters(conf, true);

	/* Destroy configuration */
	delete conf;

	return 0;
}
