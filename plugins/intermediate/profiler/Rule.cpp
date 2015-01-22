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
}
#include "profiler.h"
#include "Rule.h"

#define IPV4_SRC_FIELD 8
#define IPV4_DST_FIELD 12

#define IPV6_SRC_FIELD 27
#define IPV6_DST_FIELD 28

static const char *msg_module = "profiler";

/**
 * Constructor
 */
Rule::Rule(uint32_t id): m_id{id} 
{
}

bool Rule::matchPrefix(struct ipfix_record *data, int field) const
{
	/* Get field value */
	uint8_t *addr = data_record_get_field((uint8_t *)data->record, data->templ, 0, field, NULL);
	
	/* Field not found */
	if (!addr) {
		return false;
	}
	
	/* Prefixes are compared in two stages:
	 * 
	 * 1) compare full bytes (memcmp etc.)
	 * 2) compare remaining bits
	 * 
	 * this is a lot faster then comparing full address only by bits
	 */
	
	/* Compare full bytes */
	if (m_prefixBytes > 0) {
		if (memcmp(addr, m_prefix, m_prefixBytes) != 0) {
			return false;
		}
	}
	
	/* Compare remaining bits */
	int bit;
	for (int i = 0; i < m_prefixBits; ++i) {
		/* Comparing from left-most bit */
		bit = 7 - i;
		
		if ((addr[m_prefixBytes] & (1 << bit)) != (m_prefix[m_prefixBytes] & (1 << bit))) {
			return false;
		}
	}

	/* Prefix matched */
	return true;
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
		if (m_sourceIPv == AF_INET && info->l3_proto != 4) {
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
	
	/* Match IP prefix */
	if (m_hasPrefix) {
		if (m_prefixIPv == AF_INET) {
			/* Compare IPv4 prefix */
			if (!matchPrefix(data, IPV4_SRC_FIELD) && !matchPrefix(data, IPV4_DST_FIELD)) {
				return false;
			}
		} else {
			/* Compare IPv6 prefix */
			if (!matchPrefix(data, IPV6_SRC_FIELD) && !matchPrefix(data, IPV6_DST_FIELD)) {
				return false;
			}
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
		MSG_ERROR(msg_module, "cannot parse address %s (%s)", source.c_str(), sys_errlist[errno]);
		/* TODO: throw */
		return;
	}
	
	m_hasSource = true;
}

/**
 * Set prefix
 */
void Rule::setPrefix(char* prefix)
{
	std::string source = prefix;
	uint16_t prefixLen;
	m_prefixIPv = ipVersion(source);
	
	size_t subnetPos = source.find('/');
	if (subnetPos == std::string::npos) {
		/* no subnet mask - exact match */
		prefixLen = (m_prefixIPv == AF_INET) ? IPV4_LEN : IPV6_LEN;
	} else {
		/* get prefix length and subnet address */
		prefixLen = std::atoi(source.substr(subnetPos + 1).c_str());
		source = source.substr(0, subnetPos);
	}
	
	/* Convert address to binary form */
	if (!inet_pton(m_prefixIPv, source.c_str(), m_prefix)) {
		MSG_ERROR(msg_module, "cannot parse address %s (%s)", source.c_str(), sys_errlist[errno]);
		/* TODO: throw */
		return;
	}
	
	/* 
	 * Get number of full bytes and bits
	 * e.g. for prefix /17 is number of full bytes 2 and number of bits 1
	 * prefixes are then compared in two stages:
	 * 
	 * 1) compare full bytes (memcmp etc.)
	 * 2) compare remaining bits
	 * 
	 * this is a lot faster then comparing full address only by bits
	 */
	m_prefixBytes = prefixLen / 8;
	m_prefixBits  = prefixLen % 8;
	
	m_hasPrefix = true;
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


