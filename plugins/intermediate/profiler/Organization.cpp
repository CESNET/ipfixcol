/**
 * \file Organization.cpp
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

#define IPV4_SRC_FIELD 8
#define IPV4_DST_FIELD 12

#define IPV6_SRC_FIELD 27
#define IPV6_DST_FIELD 28

#include "Organization.h"
#include "filter.h"
#include "scanner.h"
#include "parser.h"

#include <iostream>

static const char *msg_module = "profiler";

#define OOUT(_id_) std::cout << "[O " << _id_ << "] "
#define ROUT(_id_) std::cout << "[R " << _id_ << "] "
#define POUT(_id_) std::cout << "[P " << _id_ << "] "

void Organization::print()
{
	OOUT(id()) << "rules:\n";
	for (auto rule: m_rules) {
		rule->print();
	}
	
	OOUT(id()) << "profiles:\n";
	for (auto profile: m_profiles) {
		POUT(profile->id) << "\n";
	}
}

void Rule::print()
{
	if (m_hasOdid) {
		ROUT(id()) << "ODID " << m_odid << "\n";;
	}
	
	if (m_hasPrefix) {
		char buff[50] = {0};
		inet_ntop(m_prefixIPv, m_prefix, buff, 50);
		ROUT(id()) << "PREFIX " << buff << "/" << m_prefixLen << "\n";;
	}
	
	if (m_hasSource) {
		char buff[50] = {0};
		inet_ntop(m_sourceIPv, m_source, buff, 50);
		ROUT(id()) << "SOURCE " << buff << "\n";
	}
}


/**
 * Constructor
 */
Organization::Organization(uint32_t id = 0): m_id{id} 
{
}

/**
 * Destructor - frees xmlChar buffer
 */
Organization::~Organization()
{
	freeAuxChar();
}

/**
 * Free xmlChar buffer
 */
void Organization::freeAuxChar()
{
	if (m_auxChar) {
		xmlFree(m_auxChar);
		m_auxChar = NULL;
	}
}

/**
 * Find matching rule
 */
Rule* Organization::matchingRule(struct ipfix_message* msg, struct ipfix_record* data)
{	
	for (auto rule: m_rules) {
		if (rule->matchRecord(msg, data)) {
			return rule;
		}
	}
	
	/* No matching rule was found */
	return NULL;
}

/**
 * Find matching profiles
 */
profileVec Organization::matchingProfiles(ipfix_message* msg, ipfix_record* data)
{
	profileVec vec{};
	
	for (auto profile: m_profiles) {
		if (filter_fits_node(profile->root, data)) {
			vec.push_back(profile);
		}
	}
	
	return vec;
}


/**
 * Add new rule
 */
void Organization::addRule(xmlDoc *doc, xmlNode* root)
{
	/* Get rule ID */
	m_auxChar = xmlGetProp(root, (const xmlChar *) "id");
	if (!m_auxChar) {
		MSG_WARNING(msg_module, "Missing rule ID, skipping");
		return;
	}
	
	/* Create new rule */
	Rule *rule = new Rule(atoi((char *) m_auxChar));
	freeAuxChar();
	
	std::string name;
	for (xmlNode *node = root->children; node; node = node->next) {
		
		/* Get node name and value */
		name = (char *) node->name;
		m_auxChar = xmlNodeListGetString(doc, node->children, 1);
		
		/* Set rule "sub-rules" */
		if		(name == "odid")   rule->setOdid((char *) m_auxChar);
		else if (name == "source") rule->setSource((char *) m_auxChar);
		else if (name == "prefix") rule->setPrefix((char *) m_auxChar);
		
		freeAuxChar();
	}
	
	/* Store rule */
	m_rules.push_back(rule);
}

/**
 * Parse flex/bison profile
 */
int Organization::parseFilter(filter_parser_data* pdata)
{
	int ret = 0;
	
	/* Prepare scanner */
	yylex_init(&(pdata->scanner));
	YY_BUFFER_STATE bp = yy_scan_string(pdata->filter, pdata->scanner);
	yy_switch_to_buffer(bp, pdata->scanner);
	
	/* Parse filter */
	ret = yyparse(pdata);
	
	/* Clear scanner */
	yy_flush_buffer(bp, pdata->scanner);
	yy_delete_buffer(bp, pdata->scanner);
	yylex_destroy(pdata->scanner);
	
	return ret;
}

/**
 * Add new profile
 */
void Organization::addProfile(struct filter_parser_data *pdata, xmlNode* root)
{
	/* Get profile ID */
	m_auxChar = xmlGetProp(root, (const xmlChar *) "id");
	if (!m_auxChar) {
		MSG_WARNING(msg_module, "Org %d: Missing profile ID, skipping", m_id);
		return;
	}
	
	/* Allocate space for profile */
	filter_profile *profile = reinterpret_cast<filter_profile *>(calloc(1, sizeof(filter_profile)));
	if (!profile) {
		MSG_ERROR(msg_module, "Cannot allocate space for profile (%s:%d)!", __FILE__, __LINE__);
		freeAuxChar();
		return;
	}
	
	/* Set profile id */
	profile->id = std::atoi((char *) m_auxChar);
	freeAuxChar();
	
	pdata->profile = NULL;
	pdata->filter = NULL;
	
	/* Get filter string */
	std::string name;
	for (xmlNode *node = root->children; node; node = node->next) {
		name = (char *) node->name;
		
		/* Filter found */
		if (name == "filter") {
			pdata->filter = reinterpret_cast<char*>(xmlNodeListGetString(pdata->doc, node->children, 1));
		}
	}
	
	if (!pdata->filter) {
		/* No filter was found */
		MSG_WARNING(msg_module, "Org %d: profile %d without filter string, skipping!", m_id, profile->id);
		filter_free_profile(profile);
		return;
	}
	
	pdata->profile = profile;
	
	/* Parse filter */
	if (parseFilter(pdata) != 0) {
		MSG_ERROR(msg_module, "Org %d: Error while parsing filter - skipping profile %d", m_id, profile->id);
		filter_free_profile(profile);
		free(pdata->filter);
		return;
	}
	
	/* Filter is not needed now */
	free(pdata->filter);
	
	/* Save profile */
	m_profiles.push_back(profile);
}

/*************************************************
 *	RULE IMPLEMENTATION
 *************************************************/

/**
 * Constructor
 */
Rule::Rule(uint32_t id = 0): m_id{id} 
{
}

bool Rule::matchPrefix(struct ipfix_record *data, int field)
{
	/* Compare source IPv4 address */
	uint8_t *addr = data_record_get_field((uint8_t *)data->record, data->templ, 0, field, NULL);
	
	/* Compare prefix with address (if any) */
	if (!addr || memcmp(addr, m_prefix, m_prefixLen) != 0) {
		return false;
	}
	
	return true;
}

/**
 * Match rule with IPFIX data record
 */
bool Rule::matchRecord(ipfix_message* msg, ipfix_record* data)
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
	m_odid = atoi(odid);
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
	inet_pton(m_sourceIPv, source.c_str(), m_source);
	m_hasSource = true;
}

/**
 * Set prefix
 */
void Rule::setPrefix(char* prefix)
{
	std::string source = prefix;
	m_prefixIPv = ipVersion(source);
	
	size_t subnetPos = source.find('/');
	if (subnetPos == std::string::npos) {
		/* no subnet mask - exact match */
		m_prefixLen = (m_prefixIPv == AF_INET) ? IPV4_LEN : IPV6_LEN;
	} else {
		/* get prefix length and subnet address */
		m_prefixLen = std::atoi(source.substr(subnetPos + 1).c_str());
		source = source.substr(0, subnetPos);
	}
	
	/* Convert address to binary form */
	inet_pton(m_prefixIPv, source.c_str(), m_prefix);
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
