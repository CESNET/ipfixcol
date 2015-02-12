/**
 * \file Rule.cpp
 * \author Michal Kozubik <kozubik@cesnet.cz>
 * \brief Class for profiler intermediate plugin
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
#include <arpa/inet.h>
#include <string.h>	
#include <errno.h>
}
#include "profiler.h"
#include "Rule.h"

static const char *msg_module = "profiler";

/**
 * Constructor
 */
Rule::Rule(uint32_t id): m_id{id} 
{
}

/**
 * Destructor
 */
Rule::~Rule()
{
	/* Free data filter */
	if (m_filter) {
		filter_free_profile(m_filter);
	}
}


/**
 * Set data filter
 */
void Rule::setDataFilter(filter_profile* filter)
{
	m_filter = filter;
}

/**
 * Match rule with IPFIX data record
 */
bool Rule::matchRecord(ipfix_message* msg, ipfix_record* data) const
{
	/* Match ODID */
	if (m_hasOdid) {
		if (msg->pkt_header->observation_domain_id != m_odid) {
			return false;
		}
	}
	
	/* Match source address */
	if (m_hasSource) {
		/* We want only data from network */
		struct input_info_network *info = reinterpret_cast<struct input_info_network *>(msg->input_info);
		if (info->type == SOURCE_TYPE_IPFIX_FILE) {
			return false;
		}
		
		/* Compare IP versions */
		if (   (m_sourceIPv == AF_INET  && info->l3_proto != 4)
			|| (m_sourceIPv == AF_INET6 && info->l3_proto != 6)) {
			return false;
		}
		
		/* Compare addresses */
		if (m_sourceIPv == AF_INET) {
			if (memcmp(m_source, (void*) &(info->src_addr.ipv4), IPV4_LEN) != 0) {
				return false;
			}
		} else {
			if (memcmp(m_source, (void*) &(info->src_addr.ipv6), IPV6_LEN) != 0) {
				return false;
			}
		}
	}
	
	/* Match dataFilter */
	if (m_filter) {
		if (!filter_fits_node(m_filter->root, msg, data)) {
			return false;
		}
	}
	
	return true;
}

/**
 * Set ODID
 */
void Rule::setOdid(char *odid)
{
	m_odid = htonl(atoi(odid));
	m_hasOdid = true;
}

/**
 * Set source address
 */
void Rule::setSource(char* ip)
{
	std::string source = ip;
	m_sourceIPv = ipVersion(source);
	
	/* Convert address to binary form */
	if (!inet_pton(m_sourceIPv, source.c_str(), m_source)) {
		throw std::invalid_argument("Cannot parse address " + source + " (" + sys_errlist[errno] + ")");
	}
	
	m_hasSource = true;
}

/**
 * Find out IP version
 */
int Rule::ipVersion(std::string &ip)
{
	if (ip.find(':') != std::string::npos) {
		return AF_INET6;
	}
	
	return AF_INET;
}

/**
 * Check rule validity
 */
bool Rule::isValid() const
{
	if (!m_hasSource) {
		MSG_ERROR(msg_module, "Rule %d: missing source address", m_id);
		return false;
	}
	
	return true;
}

