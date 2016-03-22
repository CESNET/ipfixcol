/**
 * \file profilestats.cpp
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

extern "C" {
#include <ipfixcol.h>
#include <ipfixcol/profiles.h>

/* API version constant */
IPFIXCOL_API_VERSION;
}

#include <rrd.h>
#include <sys/stat.h>
#include <libxml2/libxml/xpath.h>
#include <libxml2/libxml/parser.h>
#include <libxml2/libxml/tree.h>

#include "stats.h"

#include <iostream>
#include <sstream>
#include <vector>
#include <stdexcept>

/* Identifier for verbose macros */
static const char *msg_module = "profilestats";

/* some auxiliary functions for extracting data of exact length */
#define read8(ptr) (*((uint8_t *) (ptr)))
#define read16(ptr) (*((uint16_t *) (ptr)))
#define read32(ptr) (*((uint32_t *) (ptr)))
#define read64(ptr) (*((uint64_t *) (ptr)))

static std::vector<const char*> templ_fields = {
	"flows",	"flows_tcp", "flows_udp", "flows_icmp", "flows_other",
	"packets",	"packets_tcp", "packets_udp", "packets_icmp", "packets_other",
	"traffic",	"traffic_tcp", "traffic_udp", "traffic_icmp", "traffic_other",
				"packets_max", "packets_avg", "packets_dev",
	"traffic_min", "traffic_max", "traffic_avg", "traffic_dev"
};

/**
 * Statistic field
 */
struct stats_field {
	uint64_t sum[PROTOCOLS_PER_GROUP];	/**< Summary fields */
	uint64_t max;	/**< Max value */
	uint64_t min;	/**< Min value */
	uint64_t avg;	/**< Average value */
	uint64_t dev;	/**< Standard deviation */
};

/**
 * Stats data per profile/channel
 */
struct stats_data {
	uint64_t last;		/**< Last RRD update */
	std::string file;	/**< Path to RRD file */
	stats_field fields[GROUPS];	/**< Stats fields */
};

/**
 * Plugin configuration
 */
struct plugin_conf {
	std::string path;	/**< Path to RRD data */
	uint32_t interval;	/**< Update interval */
	void *ip_config;	/**< Internal process configuration */
	std::string templ;	/**< RRD template */
	std::map<std::string, stats_data*> stats;	/**< Stats data */
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
			aux_char = xmlNodeListGetString(doc, node->children, 1);
			conf->path = std::string((char *)aux_char);
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
	
	/* Sanitize path */
	if (conf->path[conf->path.length() - 1] != '/') {
		conf->path += "/";
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

		/* Create template */
		conf->templ = "";
		for (uint16_t i = 0; i < templ_fields.size(); ++i) {
			if (i > 0) {
				conf->templ += ":";
			}

			conf->templ += templ_fields[i];
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
 * \brief Create folder from file path
 *
 * \param[in] file path to the RRD stats file
 */
void stats_create_folder_from_file(std::string file)
{
	std::string::size_type lastSlash = file.find_last_of('/');

	if (lastSlash == std::string::npos) {
		return;
	}

	std::string command = "mkdir -p \"" + file.substr(0, lastSlash) + "\"";

	system(command.c_str());
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
			stats->fields[group].sum[field] = 0;
		}

		stats->fields[group].avg = 0;
		stats->fields[group].min = 0;
		stats->fields[group].max = 0;
		stats->fields[group].dev = 0;
	}

	/* Create file */
	struct stat sts;
	if (!(stat(file.c_str(), &sts) == -1 && errno == ENOENT)) {
		/* File already exists */
		return stats;
	}

	/* Create folder for file */
	stats_create_folder_from_file(file);

	char buffer[64];

	/* Create arguments field */
	std::vector<std::string> argv{};

	/* Create file */
	argv.push_back("create");
	argv.push_back(file);;

	/*
	 * Set start time
	 * time is decreased by 1 because immediately after RRD creation
	 * update is called and rrd library requires at least 1 time
	 * unit between updates
	 */
	snprintf(buffer, 64, "--start=%lld", (long long) time(NULL) - 1);
	argv.push_back(buffer);

	/* Set interval */
	snprintf(buffer, 64, "--step=%d", conf->interval);
	argv.push_back(buffer);

	/* Add all fields */
	for (auto field: templ_fields) {
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
std::string stats_counters_to_string(uint64_t last, stats_field fields[GROUPS])
{
	std::stringstream ss;

	/* Add update time */
	ss << last;

	/* Compute average */
	for (int group = 0; group < GROUPS; ++group) {
		fields[group].avg = fields[group].sum[ST_TOTAL] / fields[FLOWS].sum[ST_TOTAL];
	}

	/* Add sum statistics */
	for (int group = 0; group < GROUPS; ++group) {
		for (int proto = 0; proto < PROTOCOLS_PER_GROUP; ++proto) {
			ss << ":" << fields[group].sum[proto];
		}
	}

	/* Add rest */
	ss << ":" << fields[PACKETS].max << ":" << fields[PACKETS].avg << ":" << fields[PACKETS].dev;
	ss << ":" << fields[TRAFFIC].min << ":" << fields[TRAFFIC].max << ":" << fields[TRAFFIC].avg << ":" << fields[TRAFFIC].dev;

	/* Reset values */
	for (int group = 0; group < GROUPS; ++group) {
		for (int proto = 0; proto < PROTOCOLS_PER_GROUP; ++proto) {
			fields[group].sum[proto] = 0;
		}

		fields[group].min = fields[group].max = fields[group].avg = fields[group].dev = 0;
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
	case IG_UDP:	return ST_UDP;
	case IG_TCP:	return ST_TCP;
	case IG_ICMP:	return ST_ICMP;
	default:		return ST_OTHER;
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
 * \brief Update stats fields for one protocol
 *
 * \param[in] fields stats fields
 * \param[in] proto	protocol
 * \param[in] packets packet counter
 * \param[in] traffic traffic counter
 */
void stats_update_one_proto_fields(stats_field *fields, int proto, uint64_t packets, uint64_t traffic)
{
	fields[PACKETS].sum[proto] += packets;
	fields[TRAFFIC].sum[proto] += traffic;
	fields[FLOWS].sum[proto]   += 1;

	if (fields[PACKETS].max < packets) {
		fields[PACKETS].max = packets;
	}

	if (fields[TRAFFIC].max < traffic) {
		fields[TRAFFIC].max = traffic;
	}

	if (fields[PACKETS].min == 0 || fields[PACKETS].min > packets) {
		fields[PACKETS].min = 0;
	}

	if (fields[TRAFFIC].min == 0 || fields[TRAFFIC].min > packets) {
		fields[TRAFFIC].min = 0;
	}

	// TODO: .dev
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
	stats_update_one_proto_fields(stats->fields, ST_TOTAL, packets, traffic);
	stats_update_one_proto_fields(stats->fields, proto, packets, traffic);
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
			MSG_DEBUG(msg_module, "Updading statistics for: %s", st.second->file.c_str());
			stats_update(st.second, conf->templ);
			st.second->last = now;
		}
	}
}

stats_data *stats_get_rrd(plugin_conf *conf, std::string file)
{
	std::string fullPath = conf->path + file + ".rrd";
	stats_data *stats = conf->stats[fullPath];

	if (stats) {
		return stats;
	}

	/* Create new RRD file */
	stats = stats_rrd_create(conf, fullPath);
	conf->stats[fullPath] = stats;

	return stats;
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
	stats_data *profileStats, *channelStats;

	/* Go through all data records */
	for (uint16_t i = 0; i < msg->data_records_count; ++i) {
		struct metadata *mdata = &(msg->metadata[i]);

		if (!mdata || !mdata->channels) {
			continue;
		}

		for (int ch = 0; mdata->channels[ch]; ++ch) {
			void *channel = mdata->channels[ch];
			void *profile = channel_get_profile(channel);

			profileStats = stats_get_rrd(conf, std::string(profile_get_path(profile)) + profile_get_name(profile));
			channelStats = stats_get_rrd(conf, std::string(channel_get_path(channel)) + channel_get_name(channel));

			if (profileStats) {
				stats_update_counters(profileStats, mdata);
			}

			if (channelStats) {
				stats_update_counters(channelStats, mdata);
			}
		}
	}

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
	MSG_DEBUG(msg_module, "CLOSING");
	plugin_conf *conf = static_cast<plugin_conf*>(config);
	
	/* Force update counters */
	stats_flush_counters(conf, true);

	/* Destroy configuration */
	delete conf;

	return 0;
}
