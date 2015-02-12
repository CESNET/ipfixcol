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

#include "Organization.h"

extern "C" {
#include "scanner.h"
#include "parser.h"
}

#include <algorithm>

static const char *msg_module = "profiler";

/*************************************************
 *	DEBUG FUNCTIONS
 *************************************************/
#include <iostream>
#include <sstream>

#define OOUT(_id_) std::cout << "[" << _id_ << "] "
#define ROUT(_id_) std::cout << "\t[" << _id_ << "] "
#define POUT(_id_) std::cout << "\t[" << _id_ << "] "

std::string field_name(struct filter_field *field)
{
	std::stringstream ss;
	if (field->type == FT_DATA) {
		ss << "e" << field->enterprise << "id" << field->id;
	} else {
		switch (field->id) {
		case HF_DSTIP:
			ss << "dstip ";
			break;
		case HF_SRCIP:
			ss << "srcip ";
			break;
		case HF_DSTPORT:
			ss << "dstport ";
			break;
		case HF_SRCPORT:
			ss << "srcport ";
			break;
		case HF_ODID:
			ss << "odid ";
			break;
		}
	}
	
	return ss.str();
}

std::string print_tree(struct filter_treenode *node)
{
	if (!node) {
		return "";
	}

	std::stringstream ss;
	ss << (node->negate ? "!(" : "");
	switch (node->type) {
	case NODE_AND: 
		ss << print_tree(node->left) << " AND " << print_tree(node->right);
		break;
	case NODE_OR:
		ss << print_tree(node->left) << " OR " << print_tree(node->right);
		break;
	case NODE_EXISTS: 
		ss << "EXISTS " << field_name(node->field);
		break;
	case NODE_LEAF:
		ss << field_name(node->field);
		switch (node->op) {
		case OP_EQUAL: ss << " = "; break;
		case OP_GREATER: ss << " > "; break;
		case OP_GREATER_EQUAL: ss << " >= "; break;
		case OP_LESS: ss << " < "; break;
		case OP_LESS_EQUAL: ss << " <= "; break;
		case OP_NOT_EQUAL: ss << " != "; break;
		default: break;
		}
		
		switch (node->value->type) {
		case VT_STRING: ss << "[string]"; break;
		case VT_REGEX: ss << "[regex]"; break;
		case VT_NUMBER: ss << "[number]"; break;
		default: break;
		}
		break;
	default:
		break;
	}
	
	if (node->negate) {
		ss << ")";
	}
	
	return ss.str();
}

void Organization::print()
{
	OOUT(id()) << "rules:\n";
	for (auto rule: m_rules) {
		rule->print();
	}
	
	OOUT(id()) << "profiles:\n";
	for (auto profile: m_profiles) {
		POUT(profile->id) << print_tree(profile->root) << "\n";
	}
}

void Rule::print()
{
	if (m_hasOdid) {
		ROUT(id()) << "ODID " << ntohl(m_odid) << "\n";;
	}
	
	if (m_hasSource) {
		char buff[50] = {0};
		inet_ntop(m_sourceIPv, m_source, buff, 50);
		ROUT(id()) << "SOURCE " << buff << "\n";
	}
	
	if (m_filter) {
		ROUT(id()) << "FILTER " << print_tree(m_filter->root) << "\n";
	}
}

/*************************************************
 *	ORGANIZATION IMPLEMENTATION
 *************************************************/

/**
 * Constructor
 */
Organization::Organization(uint32_t id): m_id{id} 
{
}

/**
 * Destructor
 */
Organization::~Organization()
{
	for (auto profile: m_profiles) {
		filter_free_profile(profile);
	}
	
	for (auto rule: m_rules) {
		delete rule;
	}
	
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
Rule* Organization::matchingRule(struct ipfix_message* msg, struct ipfix_record* data) const
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
profileVec Organization::matchingProfiles(ipfix_message* msg, ipfix_record* data) const
{
	(void) msg;
	profileVec vec{};
	
	for (auto profile: m_profiles) {
		if (filter_fits_node(profile->root, msg, data)) {
			vec.push_back(profile);
		}
	}
	
	return vec;
}

/**
 * Add new rule
 */
void Organization::addRule(filter_parser_data* pdata, xmlNode* root)
{
	/* Get rule ID */
	m_auxChar = xmlGetProp(root, (const xmlChar *) "id");
	if (!m_auxChar) {
		MSG_WARNING(msg_module, "Missing rule ID, skipping");
		return;
	}
	
	/* Get rule ID and check if already used */
	uint32_t id = atoi((char *) m_auxChar);
	freeAuxChar();
	
	for (auto r: m_rules) {
		if (r->id() == id) {
			MSG_WARNING(msg_module, "Org %d: rule with existing ID (%d), skipping", m_id, id);
			return;
		}
	}
	
	/* Create new rule */
	Rule *rule = new Rule(id);
	
	pdata->filter = NULL;
	pdata->profile = NULL;
	
	std::string name;
	try {
		for (xmlNode *node = root->children; node; node = node->next) {

			/* Get node name and value */
			name = (char *) node->name;
			m_auxChar = xmlNodeListGetString(pdata->doc, node->children, 1);

			/* Set rule "sub-rules" */
			if		(name == "odid")   rule->setOdid((char *) m_auxChar);
			else if (name == "source") rule->setSource((char *) m_auxChar);
			else if (name == "dataFilter") {
				pdata->filter  = reinterpret_cast<char*>(xmlNodeListGetString(pdata->doc, node->children, 1));
				pdata->profile = reinterpret_cast<filter_profile *>(calloc(1, sizeof(filter_profile)));
				
				/* Parse filter */
				if (parseFilter(pdata) != 0) {
					free(pdata->filter);
					throw ("Error while parsing data filter");
				}
				
				free(pdata->filter);
				rule->setDataFilter(pdata->profile);
			} else {
				MSG_WARNING(msg_module, "Org %d: Rule %d: unknown element %s", m_id, id, name.c_str());
			}

			freeAuxChar();
		}
	} catch (std::exception &e) {
		if (pdata->profile) {
			filter_free_profile(pdata->profile);
		}
		MSG_ERROR(msg_module, "Org %d: Rule %d: %s", m_id, id, e.what());
		delete rule;
		return;
	}
	
	/* Check rule validity */
	if (!rule->isValid()) {
		MSG_ERROR(msg_module, "Org %d: invalid rule %d", m_id, id);
		delete rule;
		return;
	}
	
	/* Store rule */
	m_rules.push_back(rule);
}

/**
 * Parse flex/bison profile
 */
int Organization::parseFilter(filter_parser_data* pdata) const
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
	
	/* Get rule ID and chech if already used */
	uint32_t id = atoi((char *) m_auxChar);
	freeAuxChar();
	
	for (auto p: m_profiles) {
		if (p->id == id) {
			MSG_WARNING(msg_module, "Org %d: profile with existing ID (%d), skipping", m_id, id);
			return;
		}
	}
	
	/* Allocate space for profile */
	filter_profile *profile = reinterpret_cast<filter_profile *>(calloc(1, sizeof(filter_profile)));
	
	/* Set profile id */
	profile->id = id;
	
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
