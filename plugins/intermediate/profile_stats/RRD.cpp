/**
 * \file RRD.cpp
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief RRD file wrapper (source file)
 * \note Inspired by the previous implementation by Michal Kozubik
 */
/*
 * Copyright (C) 2017 CESNET, z.s.p.o.
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
 * This software is provided ``as is``, and any express or implied
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

#include <algorithm>
#include <memory>
#include <sstream>
#include <cstring>
#include <cinttypes>

#include <unistd.h>
#include <sys/stat.h>
#include <libgen.h>
#include <rrd.h>

#include "RRD.h"
#include "profilestats.h"


RRD_wrapper::RRD_wrapper(const std::string &base_dir, const std::string &path,
	uint64_t interval) : _interval(interval), _base_dir(base_dir), _path(path)
{
	stats_reset();
	directory_path_sanitize(_path);

	// Check if the path is subdirectory of the base directory
    directory_check_config(_base_dir, _path);

	// Create a storage template
	_rrd_tmplt.clear();
	for (const rrd_field &field : _tmplt_fields) {
		if (!_rrd_tmplt.empty()) {
			_rrd_tmplt += ":";
		}
		_rrd_tmplt += field.name;
	}
}

void
RRD_wrapper::file_create(uint64_t since, bool overwrite)
{
	// Check if file already exists
	if (!overwrite && access(_path.c_str(), F_OK) == 0) {
		// Exists -> skip
		return;
	}

	// Check if the base directory exists, if defined
	if (!_base_dir.empty() && !directory_exists(_base_dir)) {
		throw std::runtime_error("Base directory (" + _base_dir + ") is "
			"specified but doesn't exist. A RRD file will not be created.");
	}

	// Create directory hierarchy
	directory_create_for_file(_path);

	// Create C style array of arguments for RRD tool
	std::vector<std::string> argv;
	stats_get_create_args(since, _interval, argv);
	std::unique_ptr<const char *[]> c_argv(new const char *[argv.size()]);
	for (size_t i = 0; i < argv.size(); ++i) {
		c_argv[i] = argv[i].c_str();
	}

	// Create RRD database
	int param_size = static_cast<int>(argv.size());
	char **param_array = const_cast<char **>(c_argv.get());

	rrd_clear_error();
	if (rrd_create(param_size, param_array) != 0) {
		// Something went wrong
		throw std::runtime_error("Create error of RRD file '" + _path
			+ "': " + rrd_get_error());
	}
}

void
RRD_wrapper::file_update(uint64_t timestamp)
{
	// Make sure that the RRD file exists
	file_create(timestamp, false);

	// Create RRD update parameters
	std::vector<std::string> argv;
	argv.emplace_back("update");
	argv.emplace_back(_path);
	argv.emplace_back("--template");
	argv.emplace_back(_rrd_tmplt);
	argv.emplace_back(stats_to_string(timestamp));

	// Reset statistics
	stats_reset();

	// Create C style array
	std::unique_ptr<const char *[]> c_argv(new const char *[argv.size()]);
	for (size_t i = 0; i < argv.size(); ++i) {
		c_argv.get()[i] = argv[i].c_str();
	}

	// Update the database
	int param_size = static_cast<int>(argv.size());
	char **param_array = const_cast<char **>(c_argv.get());

	rrd_clear_error();
	if (rrd_update(param_size, param_array) != 0) {
		// Something went wrong
		throw std::runtime_error("Update error of RRD file '" + _path
			+ "': " + rrd_get_error());
	}
}

void
RRD_wrapper::flow_add(const struct flow_stat &stat)
{
	// Get protocol
	int proto;
	switch (stat.proto) {
	case IP_TCP:
		proto = ST_TCP;
		break;
	case IP_UDP:
		proto = ST_UDP;
		break;
	case IP_ICMP:
	case IP_ICMPv6:
		proto = ST_ICMP;
		break;
	default:
		proto = ST_OTHER;
		break;
	}

	// Update protocol specific statistics
	_fields[PACKETS].sum[proto] += stat.packets;
	_fields[BYTES].sum[proto] += stat.bytes;
	_fields[FLOWS].sum[proto] += 1;

	// Update total statistics
	_fields[PACKETS].sum[ST_TOTAL] += stat.packets;
	_fields[BYTES].sum[ST_TOTAL] += stat.bytes;
	_fields[FLOWS].sum[ST_TOTAL] += 1;

	if (_fields[PACKETS].max < stat.packets) {
		_fields[PACKETS].max = stat.packets;
	}

	if (_fields[BYTES].max < stat.bytes) {
		_fields[BYTES].max = stat.bytes;
	}

}

/**
 * \brief Create arguments for new RRD database
 * \param[in]  ts_start Specifies the time in seconds since 1970-01-01 UTC when
 *   the first value should be added to the RRD.
 * \param[in]  ts_step  Specifies the base interval in seconds with which
 *   data will be fed into the RRD.
 * \param[out] args Generated arguments
 */
void
RRD_wrapper::stats_get_create_args(uint64_t ts_start, uint64_t ts_step,
		std::vector<std::string> &args)
{
	args.clear();

	// Create arguments field
	constexpr size_t buffer_size = 128;
	char buffer[buffer_size];

	args.emplace_back("create");
	args.emplace_back(_path);

	/*
	 * Set start time
	 * Time is decreased because immediately after RRD creation update is called
	 * and RRD library requires at least 1 time unit between updates
	 */
	snprintf(buffer, buffer_size, "--start=%" PRIu64, ts_start - ts_step);
	args.emplace_back(buffer);

	// Set interval
	snprintf(buffer, buffer_size, "--step=%" PRIu64, ts_step);
	args.emplace_back(buffer);

	// Add all fields
	for (const rrd_field &field: _tmplt_fields) {
		/*
		 * DS = Data Source definition,
		 * Heartbeat interval defines the maximum number of seconds that may
		 *   pass between two updates of this data source before the value of
		 *   the data source is assumed to be *UNKNOWN*
		 */
		const char *name = field.name;
		const char *type = _rrd_dst_str[field.type];
		const unsigned heartbeat = 2 * ts_step;

		snprintf(buffer, buffer_size, "DS:%s:%s:%u:0:U", name, type, heartbeat);
		args.emplace_back(buffer);
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
	const unsigned samples_per_day = secs_per_day / ts_step;

	const unsigned total_rec_history_short = samples_per_day * history_short;

	const char *fmt_avg = "RRA:AVERAGE:0.5:%u:%u";
	const char *fmt_max = "RRA:MAX:0.5:%u:%u";

	snprintf(buffer, buffer_size, fmt_avg, 1U, total_rec_history_short);
	args.emplace_back(buffer); // For 5 minute example: "RRA:AVERAGE:0.5:1:51840"
	snprintf(buffer, buffer_size, fmt_avg, 6U, total_rec_history_short / 6U);
	args.emplace_back(buffer); // For 5 minute example: "RRA:AVERAGE:0.5:6:8640"
	snprintf(buffer, buffer_size, fmt_avg, 24U, total_rec_history_short / 24U);
	args.emplace_back(buffer); // For 5 minute example: "RRA:AVERAGE:0.5:24:2160"
	snprintf(buffer, buffer_size, fmt_avg, samples_per_day, history_long);
	args.emplace_back(buffer); // For 5 minute example: "RRA:AVERAGE:0.5:288:1825"
	snprintf(buffer, buffer_size, fmt_max, 1U, total_rec_history_short);
	args.emplace_back(buffer); // For 5 minute example: "RRA:MAX:0.5:1:51840"
	snprintf(buffer, buffer_size, fmt_max, 6U, total_rec_history_short / 6U);
	args.emplace_back(buffer); // For 5 minute example: "RRA:MAX:0.5:6:8640"
	snprintf(buffer, buffer_size, fmt_max, 24U, total_rec_history_short / 24U);
	args.emplace_back(buffer); // For 5 minute example: "RRA:MAX:0.5:24:2160"
	snprintf(buffer, buffer_size, fmt_max, samples_per_day, history_long);
	args.emplace_back(buffer); // For 5 minute example: "RRA:MAX:0.5:288:1825"mak
}

/**
 * \brief Convert statistics to an update string required by RRD tools
 * \param[in] timestamp The date used for updating the RRD
 * \return The string
 */
std::string
RRD_wrapper::stats_to_string(uint64_t timestamp)
{
	std::stringstream ss;

	// Add update time
	ss << timestamp;

	// Compute averages
	const uint64_t total_flows = _fields[FLOWS].sum[ST_TOTAL];
	for (size_t group = 1; group < ST_GROUP_CNT; ++group) {
		if (total_flows == 0) {
			_fields[group].avg = 0;
		} else {
			_fields[group].avg = _fields[group].sum[ST_TOTAL] / total_flows;
		}
	}

	// Add sum statistics
	for (size_t group = 0; group < ST_GROUP_CNT; ++group) {
		for (int proto = 0; proto < ST_PROTOCOL_CNT; ++proto) {
			ss << ":" << _fields[group].sum[proto];
		}
	}

	// Add rest
	ss << ":" << _fields[PACKETS].max << ":" << _fields[PACKETS].avg;
	ss << ":" << _fields[BYTES].max << ":" << _fields[BYTES].avg;
	return ss.str();
}

/**
 * \brief Reset statistics
 * \note All counters will be set to zeros
 */
void
RRD_wrapper::stats_reset()
{
	memset(_fields, 0, sizeof(_fields));
}

/**
 * \brief Check directory configuration
 *
 * If the \p base directory is not empty string then \p path must
 *   be its subdirectory.
 * \param[in] base Base directory
 * \param[in] path Full file path
 * \throws invalid_argument if the \p path is not subdirectory
 */
void
RRD_wrapper::directory_check_config(const std::string &base, const std::string &path)
{
	if (base.empty()) {
		return;
	}

	if (directory_is_subdir(base, path)) {
		// Everything is OK...
		return;
	}

	throw std::invalid_argument("Failed to create a RRD. Base storage "
		"directory (" + base + ") is specified, but the RRD file (" + path
		+ ") of this profile/channel is outside of the base directory. Change "
		"storage directory of the profile/channel or omit storage directory "
		"in the plugin's configuration");
}

/**
 * \brief Remove redundant slashes '/' from a path
 * \param[in, out] str Path to modify
 */
void
RRD_wrapper::directory_path_sanitize(std::string &str)
{
	// Remove redundant slashes
	auto both_slashes = [](char lhs, char rhs) {
		return (lhs == rhs) && (lhs == '/');
	};

	std::string::iterator new_end = std::unique(str.begin(), str.end(),
		both_slashes);
	str.erase(new_end, str.end());
}

/**
 * \brief Check if a directory is inside a base directory
 * \param[in] base_dir Base directory path
 * \param[in] dir      Directory path
 * \return True or false
 */
bool
RRD_wrapper::directory_is_subdir(const std::string &base_dir,
	const std::string &dir)
{
	if (base_dir.empty()) {
		// Nothing to check...
		return true;
	}

	std::string dir_new = dir + '/';
	std::string base_new = base_dir + '/';
	directory_path_sanitize(dir_new);
	directory_path_sanitize(base_new);

	std::string::size_type pos = dir_new.find(base_new);
	return (pos == 0);
}

/**
 * \brief Check if a directory exists
 * \param[in] dir Directory path
 * \note If search permission is denied, the function also returns false.
 * \return True or false
 */
bool
RRD_wrapper::directory_exists(const std::string &dir)
{
	struct stat sb;
	if (stat(dir.c_str(), &sb) == 0 && S_ISDIR(sb.st_mode)) {
		// Exists
		return true;
	} else {
		// Doesn't exist or search permission is denied
		return false;
	}
}

/**
 * \brief Create (recursively) directory for a file
 * \param[in] file Path to the file
 * \throws runtime_error if mkdir function fails
 */
void
RRD_wrapper::directory_create_for_file(const std::string &file)
{
	// Create a directory hierarchy
	size_t size = file.length() + 1; // + 1 = '\0'
	std::unique_ptr<char[]> dir_cpy(new char[size]);
	file.copy(dir_cpy.get(), size);
	dir_cpy.get()[size - 1] = '\0';
	errno = 0;

	const char *dir = dirname(dir_cpy.get());
	if (utils_mkdir(dir)) {
		constexpr size_t buffer_size = 128;
		char buffer[buffer_size];
		buffer[0] = '\0';
		strerror_r(errno, buffer, buffer_size);
		throw std::runtime_error("Failed to create a directory '"
			+ std::string(dir) + "': " + buffer);
	}
}

const char *RRD_wrapper::_rrd_dst_str[] = {
		"GAUGE",
		"COUNTER",
		"DCOUNTER",
		"DERIVE",
		"DDERIVE",
		"ABSOLUTE",
		"COMPUTE"
};

const std::vector<RRD_wrapper::rrd_field> RRD_wrapper::_tmplt_fields = {
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