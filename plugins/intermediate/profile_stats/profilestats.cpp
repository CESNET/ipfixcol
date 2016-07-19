/**
 * \file profilestats.cpp
 * \author Michal Kozubik <kozubik@cesnet.cz>
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
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

// API version constant
IPFIXCOL_API_VERSION
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

// Identifier for verbose macros
static const char *msg_module = "profilestats";

// Some auxiliary functions for extracting data of exact length
#define read8(ptr) (*((uint8_t *) (ptr)))
#define read16(ptr) (*((uint16_t *) (ptr)))
#define read32(ptr) (*((uint32_t *) (ptr)))
#define read64(ptr) (*((uint64_t *) (ptr)))

/**
 * \brief Maximum update interval of RRD (in seconds)
 */
#define UPDATE_INT_MAX (3600)

/**
 * \brief Minimum update interval of RRD (in seconds)
 */
#define UPDATE_INT_MIN (5)

/**
 * \brief Type of RRD data source
 */
enum RRD_DATA_SOURCE_TYPE {
	RRD_DST_GAUGE = 0,
	RRD_DST_COUNTER,
	RRD_DST_DCOUNTER,
	RRD_DST_DERIVE,
	RRD_DST_DDERIVE,
	RRD_DST_ABSOLUTE,
	RRD_DST_COMPUTE
};

// String alternative of the enum above
static const char *rrd_dst_str[] = {
	"GAUGE",
	"COUNTER",
	"DCOUNTER",
	"DERIVE",
	"DDERIVE",
	"ABSOLUTE",
	"COMPUTE"
};

/**
 * \brief Definition of a RRD source
 */
struct rrd_field {
	const char           *name; /**< Name of the source      */
	RRD_DATA_SOURCE_TYPE  type; /**< Type of the source      */
};

/**
 * \brief Names and types of RRD Data sources
 */
const static std::vector<rrd_field> templ_fields = {
	// Flows
	{"flows",            RRD_DST_ABSOLUTE},
	{"flows_tcp",        RRD_DST_ABSOLUTE},
	{"flows_udp",        RRD_DST_ABSOLUTE},
	{"flows_icmp",       RRD_DST_ABSOLUTE},
	{"flows_other",      RRD_DST_ABSOLUTE},
	// Packets
	{"packets",          RRD_DST_ABSOLUTE},
	{"packets_tcp",      RRD_DST_ABSOLUTE},
	{"packets_udp",      RRD_DST_ABSOLUTE},
	{"packets_icmp",     RRD_DST_ABSOLUTE},
	{"packets_other",    RRD_DST_ABSOLUTE},
	// Traffic
	{"traffic",          RRD_DST_ABSOLUTE},
	{"traffic_tcp",      RRD_DST_ABSOLUTE},
	{"traffic_udp",      RRD_DST_ABSOLUTE},
	{"traffic_icmp",     RRD_DST_ABSOLUTE},
	{"traffic_other",    RRD_DST_ABSOLUTE},
	// Others
	{"packets_max",      RRD_DST_GAUGE},
	{"packets_avg",      RRD_DST_GAUGE},
	{"traffic_max",      RRD_DST_GAUGE},
	{"traffic_avg",      RRD_DST_GAUGE}
};

/**
 * Statistic fields
 */
struct stats_field {
	uint64_t sum[PROTOCOLS_PER_GROUP];  /**< Summary fields                 */
	uint64_t max;                       /**< Max value                      */
	uint64_t avg;                       /**< Average value                  */
};

/**
 * Stats data per profile/channel
 */
struct stats_data {
	uint64_t    last_rrd_update;  /**< Last RRD update (unix timestamp)     */
	uint32_t    last_update_id;   /**< Identification of the last update    */
	std::string file;             /**< Path to RRD file                     */
	stats_field fields[GROUPS];   /**< Stats fields                         */
};

/**
 * Plugin configuration
 */
struct plugin_conf {
	/** Internal process configuration                 */
	void *ip_config;
	/** Update interval (in seconds)                   */
	uint32_t interval;
	/**< RRD template for database update              */
	std::string templ;
	/**< Stats data for a profile                      */
	std::map<std::string, stats_data*> stats;

	/** Sequence number for update identification      */
	uint32_t update_id;
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
	
	// Load XML configuration
	xmlDoc *doc = xmlParseDoc(BAD_CAST params);
	if (!doc) {
		throw std::invalid_argument("Cannot parse config xml");
	}
	
	xmlNode *root = xmlDocGetRootElement(doc);
	if (!root) {
		throw std::invalid_argument("Cannot get document root element!");
	}
	
	// Set default interval
	conf->interval = DEFAULT_INTERVAL;

	// Iterate throught all elements
	for (xmlNode *node = root->children; node; node = node->next) {
		if (node->type != XML_ELEMENT_NODE) {
			continue;
		}

		if (!xmlStrcmp(node->name, (const xmlChar *) "interval")) {
			// Statistics interval
			aux_char = xmlNodeListGetString(doc, node->children, 1);
			conf->interval = atoi((const char *) aux_char);
			xmlFree(aux_char);
		}
	}

	// Free resources
	xmlFreeDoc(doc);

	if (conf->interval < UPDATE_INT_MIN || conf->interval > UPDATE_INT_MAX) {
		throw std::invalid_argument("Interval is out of range (5 .. 3600)");
	}
}

/**
 * \brief Plugin initialization
 *
 * \param[in] params xml   Configuration
 * \param[in] ip_config    Intermediate process config
 * \param[in] ip_id        Intermediate process ID for template manager
 * \param[in] template_mgr Template manager
 * \param[out] config      Config storage
 * \return 0 on success
 */
int intermediate_init(char* params, void* ip_config, uint32_t ip_id,
	ipfix_template_mgr* template_mgr, void** config)
{	
	// Suppress compiler warnings
	(void) ip_id; (void) template_mgr;
	
	if (!params) {
		MSG_ERROR(msg_module, "Missing plugin configuration");
		return 1;
	}
	
	try {
		// Create configuration
		plugin_conf *conf = new plugin_conf;
		conf->update_id = 0;
		
		// Process params
		process_startup_xml(conf, params);

		// Create template
		conf->templ = "";
		for (uint16_t i = 0; i < templ_fields.size(); ++i) {
			if (i > 0) {
				conf->templ += ":";
			}

			conf->templ += templ_fields[i].name;
		}

		// Save configuration
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
 * \param[in] file Path to the RRD stats file
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
	// Create stats counters
	stats_data *stats = new stats_data;
	stats->file = file;
	stats->last_rrd_update = time(NULL);
	stats->last_update_id = 0;

	for (int group = 0; group < GROUPS; ++group) {
		for (int field = 0; field < PROTOCOLS_PER_GROUP; ++field) {
			stats->fields[group].sum[field] = 0;
		}

		stats->fields[group].avg = 0;
		stats->fields[group].max = 0;
	}

	// Create a file
	struct stat sts;
	if (!(stat(file.c_str(), &sts) == -1 && errno == ENOENT)) {
		// File already exists
		return stats;
	}

	// Create folder for file
	stats_create_folder_from_file(file);

	const size_t buffer_size = 128;
	char buffer[buffer_size];

	// Create arguments field
	std::vector<std::string> argv{};

	// Create the file
	argv.push_back("create");
	argv.push_back(file);;

	/*
	 * Set start time
	 * Time is decreased by 1 because immediately after RRD creation
	 * update is called and RRD library requires at least 1 time
	 * unit between updates
	 */
	snprintf(buffer, buffer_size, "--start=%lld", (long long) time(NULL) - 1);
	argv.push_back(buffer);

	// Set interval
	snprintf(buffer, buffer_size, "--step=%d", conf->interval);
	argv.push_back(buffer);

	// Add all fields
	for (const auto &field: templ_fields) {
		/*
		 * DS = Data Source definition,
		 * Heartbeat interval defines the maximum number of seconds that may
		 *   pass between two updates of this data source before the value of
		 *   the data source is assumed to be *UNKNOWN*
		 */
		const char *name = field.name;
		const char *type = rrd_dst_str[field.type];
		const unsigned heartbeat = 2 * conf->interval;

		snprintf(buffer, buffer_size, "DS:%s:%s:%u:0:U", name, type, heartbeat);
		argv.push_back(buffer);
	}

	/*
	 * Add statistics i.e. Round Robin Archives (RRA)
	 * For example: 5 minute update interval (300 seconds):
	 *   1 x 5m =  5 min samples, 6 *  30 * 288 (per day) = 51840 (6 * 30 days)
	 *   6 x 5m = 30 min samples, 6 *  30 *  48 (per day) = 8640  (6 * 30 days)
	 *  24 x 5m = 2 hour samples, 6 *  30 *  12 (per day) = 2160  (6 * 30 days)
	 * 288 x 5m = 1 day samples,  5 * 365 *   1 (per day) = 1825  (5 * 365 days)
	 */
	// FIXME: add to the configuration (long/short term)
	const unsigned history_long  = 5 * 365;   // days (5 years)
	const unsigned history_short = 3 * 30;    // days (approx. quarter a year)
	const unsigned secs_per_day = 24 * 60 * 60;
	const unsigned samples_per_day = secs_per_day / conf->interval;

	const unsigned total_rec_history_short = samples_per_day * history_short;

	const char *fmt_avg = "RRA:AVERAGE:0.5:%u:%u";
	const char *fmt_max = "RRA:MAX:0.5:%u:%u";

	snprintf(buffer, buffer_size, fmt_avg, 1U, total_rec_history_short);
	argv.push_back(buffer); // For 5 minute example: "RRA:AVERAGE:0.5:1:51840"
	snprintf(buffer, buffer_size, fmt_avg, 6U, total_rec_history_short / 6U);
	argv.push_back(buffer); // For 5 minute example: "RRA:AVERAGE:0.5:6:8640"
	snprintf(buffer, buffer_size, fmt_avg, 24U, total_rec_history_short / 24U);
	argv.push_back(buffer); // For 5 minute exmmple: "RRA:AVERAGE:0.5:24:2160"
	snprintf(buffer, buffer_size, fmt_avg, samples_per_day, history_long);
	argv.push_back(buffer); // For 5 minute exmmple: "RRA:AVERAGE:0.5:288:1825"
	snprintf(buffer, buffer_size, fmt_max, 1U, total_rec_history_short);
	argv.push_back(buffer); // For 5 minute example: "RRA:MAX:0.5:1:51840"
	snprintf(buffer, buffer_size, fmt_max, 6U, total_rec_history_short / 6U);
	argv.push_back(buffer); // For 5 minute example: "RRA:MAX:0.5:6:8640"
	snprintf(buffer, buffer_size, fmt_max, 24U, total_rec_history_short / 24U);
	argv.push_back(buffer); // For 5 minute exmmple: "RRA:MAX:0.5:24:2160"
	snprintf(buffer, buffer_size, fmt_max, samples_per_day, history_long);
	argv.push_back(buffer); // For 5 minute exmmple: "RRA:MAX:0.5:288:1825"

	// Create C style argv
	const char **c_argv = new const char*[argv.size()];
	for (u_int16_t i = 0; i < argv.size(); ++i) {
		c_argv[i] = argv[i].c_str();
	}

	// Create RRD database
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
 * \warning Also resets all counters
 * \param[in] last	 Update time (unix timestamp)
 * \param[in] fields Stats fields
 * \return Counters converted to string
 */
std::string stats_counters_to_string(uint64_t last, stats_field fields[GROUPS])
{
	std::stringstream ss;

	// Add update time
	ss << last;

	// Compute averages
	const uint64_t total_flows = fields[FLOWS].sum[ST_TOTAL];
	for (int group = 1; group < GROUPS; ++group) {
		if (total_flows == 0) {
			fields[group].avg = 0;
		} else {
			fields[group].avg = fields[group].sum[ST_TOTAL] / total_flows;
		}
	}

	// Add sum statistics
	for (int group = 0; group < GROUPS; ++group) {
		for (int proto = 0; proto < PROTOCOLS_PER_GROUP; ++proto) {
			ss << ":" << fields[group].sum[proto];
		}
	}

	// Add rest
	ss << ":" << fields[PACKETS].max << ":" << fields[PACKETS].avg;
	ss << ":" << fields[TRAFFIC].max << ":" << fields[TRAFFIC].avg;

	// Reset values
	for (int group = 0; group < GROUPS; ++group) {
		for (int proto = 0; proto < PROTOCOLS_PER_GROUP; ++proto) {
			fields[group].sum[proto] = 0;
		}

		fields[group].max = fields[group].avg = 0;
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

	// Set RRD file
	argv.push_back("update");
	argv.push_back(stats->file);

	// Set template
	argv.push_back("--template");
	argv.push_back(templ);

	// Add counters
	argv.push_back(stats_counters_to_string(stats->last_rrd_update, stats->fields));

	// Create C style argv
	const char **c_argv = new const char*[argv.size()];
	for (u_int16_t i = 0; i < argv.size(); ++i) {
		c_argv[i] = argv[i].c_str();
	}

	// Update database
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
	// Get "protocolIdentifier"
	uint8_t *data = data_record_get_field((uint8_t*) rec->record, rec->templ,
		0, PROTOCOL_ID, NULL);
	int ipfix_proto = data ? (*((uint8_t *) (data))) : 0;

	// Decode value
	switch (ipfix_proto) {
	case IG_UDP:	return ST_UDP;
	case IG_TCP:	return ST_TCP;
	case IG_ICMP:	return ST_ICMP;
	default:		return ST_OTHER;
	}
}

/**
 * \brief Get a field value
 *
 * \param[in] rec Data record
 * \param[in] field_id	Field ID
 * \return field value (or zero if not found)
 */
uint64_t stats_field_val(ipfix_record *rec, int field_id)
{
	int dataSize = 0;
	uint8_t *data = data_record_get_field((uint8_t*) rec->record, rec->templ,
		0, field_id, &dataSize);

	// Field not found
	if (!data) {
		return 0;
	}

	// Convert value to host byte order
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
void stats_update_one_proto_fields(stats_field *fields, int proto,
	uint64_t packets, uint64_t traffic)
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
}

/**
 * \brief Update stats counters
 *
 * \param[in] stats     Statistics
 * \param[in] mdata     Data record's metadata
 * \param[in] update_id Update ID (prevention of multiple updates from the same
 *   record)
 */
void stats_update_counters(stats_data *stats, metadata *mdata, uint32_t update_id)
{
	if (stats->last_update_id == update_id) {
		// Already updated
		return;
	}

	stats->last_update_id = update_id;

	// Get stats values
	uint64_t packets = stats_field_val(&(mdata->record), PACKETS_ID);
	uint64_t traffic = stats_field_val(&(mdata->record), TRAFFIC_ID);

	// Decode protocol
	enum st_protocol proto = stats_get_proto(&(mdata->record));

	// Update total stats
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
	// Update stats
	uint64_t now = time(NULL);
	for (auto st: conf->stats) {
		// Some pointers can be NULL after unsuccessfull creation
		if (!st.second) {
			continue;
		}

		uint64_t next_window; // Start of the next window
		next_window = (st.second->last_rrd_update / conf->interval) + 1;
		next_window *= conf->interval;

		if (force || next_window <= now) {
			MSG_DEBUG(msg_module, "Updating statistics for: %s",
				st.second->file.c_str());
			stats_update(st.second, conf->templ);
			st.second->last_rrd_update = now;
		}
	}
}

/**
 * \brief Get a statistic record for a RRD
 *
 * If the record doesn't exist, create a new one.
 * \param[in,out] conf Plugin configuration
 * \param[in]     file Path to the RRD
 * \return On success returns pointer to the record. Otherwise returns NULL.
 */
stats_data *stats_get_rrd(plugin_conf *conf, std::string file)
{
	std::string fullPath = file + ".rrd";
	stats_data *stats = conf->stats[fullPath];

	if (stats) {
		return stats;
	}

	// Create new RRD file
	stats = stats_rrd_create(conf, fullPath);
	conf->stats[fullPath] = stats;

	return stats;
}

/**
 * \brief Process IPFIX message
 *
 * \param[in] config  Plugin configuration
 * \param[in] message IPFIX message
 * \return 0 on success
 */
int intermediate_process_message(void* config, void* message)
{
	plugin_conf *conf = reinterpret_cast<plugin_conf *>(config);
	struct ipfix_message *msg = reinterpret_cast<struct ipfix_message *>(message);

	// Catch closing message
	if (msg->source_status == SOURCE_STATUS_CLOSED) {
		pass_message(conf->ip_config, msg);
		return 0;
	}

	// Update counters
	stats_flush_counters(conf);

	// Process message
	stats_data *profileStats, *channelStats;
	std::string profile_dir,   channel_dir;

	// Go through all data records
	for (uint16_t i = 0; i < msg->data_records_count; ++i) {
		struct metadata *mdata = &(msg->metadata[i]);
		conf->update_id++;

		if (!mdata || !mdata->channels) {
			continue;
		}

		for (int ch = 0; mdata->channels[ch]; ++ch) {
			void *channel = mdata->channels[ch];
			void *profile = channel_get_profile(channel);

			profile_dir = profile_get_directory(profile);
			profile_dir += "/rrd/";
			channel_dir = profile_dir + "channels/";

			profileStats = stats_get_rrd(conf, profile_dir +
				profile_get_name(profile));
			channelStats = stats_get_rrd(conf, channel_dir +
				channel_get_name(channel));

			if (profileStats) {
				stats_update_counters(profileStats, mdata, conf->update_id);
			}

			if (channelStats) {
				stats_update_counters(channelStats, mdata, conf->update_id);
			}
		}
	}

	pass_message(conf->ip_config, msg);
	return 0;
}

/**
 * \brief Close intermediate plugin
 *
 * \param[in] config Plugin configuration
 * \return 0 on success
 */
int intermediate_close(void *config)
{
	MSG_DEBUG(msg_module, "CLOSING");
	plugin_conf *conf = static_cast<plugin_conf*>(config);
	
	// Force update counters
	stats_flush_counters(conf, true);

	// Destroy configuration
	for (auto &stat : conf->stats) {
		stats_data *ptr = stat.second;
		if (!ptr) {
			continue;
		}

		delete ptr;
	}

	delete conf;

	return 0;
}
