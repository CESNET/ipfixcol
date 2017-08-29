/**
 * \file RRD.h
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief RRD file wrapper (header file)
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

#ifndef PROFILESTATS_RRD_H
#define PROFILESTATS_RRD_H

#include <string>
#include <vector>

extern "C" {
#include <ipfixcol.h>
}

class RRD_wrapper {
private:
	/** Statistics groups                  */
	enum st_group {
		FLOWS = 0,   /**< This MUST be the first!              */
		PACKETS,
		BYTES,
		ST_GROUP_CNT /**< This must be always the last element */
	};

	/** Statistics protocols               */
	enum st_protocol {
		ST_TOTAL = 0,   /**< This MUST be the first!              */
		ST_TCP,
		ST_UDP,
		ST_ICMP,
		ST_OTHER,
		ST_PROTOCOL_CNT /**< This must be always the last element */
	};

	/** IPFIX protocol identifiers         */
	enum ipfix_protocol {
		IP_ICMP = 1,
		IP_TCP = 6,
		IP_UDP = 17,
		IP_ICMPv6 = 58,
	};

	/** Type of RRD data source            */
	enum rrd_data_source_type {
		RRD_DST_GAUGE = 0,
		RRD_DST_COUNTER,
		RRD_DST_DCOUNTER,
		RRD_DST_DERIVE,
		RRD_DST_DDERIVE,
		RRD_DST_ABSOLUTE,
		RRD_DST_COMPUTE
	};

	/** Definition of a RRD source         */
	struct rrd_field {
		const char           *name; /**< Name of the source      */
		rrd_data_source_type  type; /**< Type of the source      */
	};

	/** String alternatives of the enum ::rrd_data_source_type   */
	static const char *_rrd_dst_str[];
	/** Names and types of RRD Data sources */
	static const std::vector<rrd_field> _tmplt_fields;

	/** Statistic fields                   */
	struct stats_field {
		/** Summary fields                 */
		uint64_t sum[ST_PROTOCOL_CNT];
		/** Max value                      */
		uint64_t max;
		/** Average value                  */
		uint64_t avg;
	};

	/** Update interval                    */
	uint64_t _interval;
	/** Update template for RRD files      */
	std::string _rrd_tmplt;
	/** Base directory                     */
	std::string _base_dir;
	/** Path to the RRD file               */
	std::string _path;
	/** Stats data (local counters)        */
	struct stats_field _fields[ST_GROUP_CNT];

	void
	stats_reset();
	std::string
	stats_to_string(uint64_t timestamp);
	void
	stats_get_create_args(uint64_t ts_start, uint64_t ts_step,
		std::vector<std::string> &args);

    void
	directory_check_config(const std::string &base, const std::string &path);
	void
	directory_path_sanitize(std::string &str);
	bool
	directory_is_subdir(const std::string &base_dir, const std::string &dir);
	bool
	directory_exists(const std::string &dir);
	void
	directory_create_for_file(const std::string &file);



public:
	/**
	 * \brief Create a wrapper of a RRD file
	 * \note If the file doesn't exists, you must create a new one using
	 *   file_create() function.
	 * \param[in] base_dir Base directory (can be empty string)
	 * \param[in] path     Full path to the database
	 * \param[in] interval RRD update interval
	 * \warning If the \p base_dir is not empty, the RRD file will not be create
	 *   until the directory already exists in the system. The \p base_dir also
	 *   MUST be prefix of the full \p path.
	 */
	RRD_wrapper(const std::string &base_dir, const std::string &path,
		uint64_t interval);
	/**
	 * \brief Destroy a wrapper
	 */
	~RRD_wrapper() = default;

	/**
	 * \brief Create a new RRD file
	 * \param[in] since     Specifies the time in seconds since 1970-01-01 UTC
	 *   when the first value should be added to the RRD.
	 * \param[in] overwrite Overwrite original file if exists
	 * \throws runtime_error in case of failure
	 */
	void
	file_create(uint64_t since, bool overwrite = false);
	/**
	 * \brief Flush local counters to the RRD file and reset the counters
	 * \note If the RRD file doesn't exists, the function will try to create
	 *   a new one.
	 * \param[in] timestamp Update timestamp
	 */
	void
	file_update(uint64_t timestamp);

	/**
	 * \brief Add a new flow to local statistics
	 * \param[in] stat Flow statistics
	 */
	void
	flow_add(const struct flow_stat &stat);
};


#endif // PROFILESTATS_RRD_H
