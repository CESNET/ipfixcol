/**
 * \file profiles/filter.c
 * \author Michal Kozubik <kozubik@cesnet.cz>
 * \brief Intermediate plugin for IPFIX data filtering
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

#define _XOPEN_SOURCE

#include <arpa/inet.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpathInternals.h>

#include <ipfixcol.h>
#include <regex.h>
#include <libxml/xmlstring.h>

#include "filter.h"
#include "scanner.h"
#include "parser.h"

static const char *msg_module = "profiler";

/* Packet header fields - must correspond with header_field enum and end with NULL */
static const char *header_fields[] = {
	/* HF_ODID */		"ODID", 
	/* HF_SRCIP */		"SRCADDR", 
	/* HF_SRCPORT */	"SRCPORT",
	/* HF_DSTIP */		"DSTADDR",
	/* HF_DSTPORT */	"DSTPORT",
	NULL
};
	

/**
 * \brief Free tree structure
 */
void filter_free_tree(struct filter_treenode *node)
{
	if (!node) {
		return;
	}

	/* Free left and right subtrees */
	filter_free_tree(node->left);
	filter_free_tree(node->right);

	/* Free value */
	if (node->value) {
		if (node->value->value) {
			if (node->value->type == VT_REGEX) {
				regfree((regex_t *) node->value->value);
			}
			
			free(node->value->value);
		}
		free(node->value);
	}
	
	/* Free field */
	if (node->field) {
		free(node->field);
	}

	free(node);
}

/**
 * \brief Free profile structure
 *
 * \param[in] profile Filter profile
 */
void filter_free_profile(struct filter_profile *profile)
{
	/* Free filter tree */
	filter_free_tree(profile->root);
	free(profile);
}

/**
 * \brief Initialize ipfix-elements.xml
 * 
 * Opens xml file with elements specification and initializes file context
 *
 * \param[in] pdata Parser data structure
 */
int filter_init_elements(struct filter_parser_data *pdata)
{
	pdata->doc = xmlReadFile(ipfix_elements, NULL, 0);
	if (!pdata->doc) {
		MSG_ERROR(msg_module, "Unable to parse elements configuration file %s", ipfix_elements);
		return 1;
	}

	pdata->context = xmlXPathNewContext(pdata->doc);
	if (pdata->context == NULL) {
		MSG_ERROR(msg_module, "Error in xmlXPathNewContext");
		return 1;
	}
	
	return 0;
}

/**
 * \brief Check whether value in data record fits with node expression
 *
 * \param[in] node Filter tree node
 * \param[in] msg IPFIX message (filter may contain field from message header)
 * \param[in] record IPFIX data record
 * \return true if data record's field fits
 */
bool filter_fits_value(struct filter_treenode *node, struct ipfix_message *msg, struct ipfix_record *record)
{
	int cmpres;
	
	if (node->field->type == FT_HEADER) {
		/* Header field */
		struct input_info_network *info = (struct input_info_network*) msg->input_info;
		
		switch (node->field->id) {
		case HF_ODID:
			cmpres = !(msg->pkt_header->observation_domain_id == *((uint32_t *) node->value->value));
			break;
		case HF_SRCIP:
			cmpres = memcmp(&(info->src_addr), node->value->value, info->l3_proto == 4 ? sizeof(info->src_addr.ipv4) : sizeof(info->src_addr.ipv6));
			break;
		case HF_SRCPORT:
			cmpres = memcmp(&(info->src_port), node->value->value, sizeof(info->src_port));
			break;
		case HF_DSTIP:
			cmpres = memcmp(&(info->dst_addr), node->value->value, info->l3_proto == 4 ? sizeof(info->dst_addr.ipv4) : sizeof(info->dst_addr.ipv6));
			break;
		case HF_DSTPORT:
			cmpres = memcmp(&(info->dst_port), node->value->value, sizeof(info->dst_port));
			break;
		default:
			cmpres = 1;
			break;
		}
			
	} else {
		/* Data field */
		int datalen;
		
		/* Get data from record */
		uint8_t *recdata = data_record_get_field(record->record, record->templ, node->field->enterprise, node->field->id, &datalen);
		if (!recdata) {
			/* Field not found - if op is '!=' it is success */
			return node->op == OP_NOT_EQUAL;
		}

		if (datalen > node->value->length) {
			MSG_DEBUG(msg_module, "Cannot compare %d bytes with %d bytes", datalen, node->value->length);
			return node->op == OP_NOT_EQUAL;
		}

		/*
		 * Compare values
		 * values are in network byte order
		 * node value can be on more bytes than value in data
		 * e.g:
		 * value in data is 4 bytes long: 0 0 0 5
		 * value in node is 8 bytes long: 0 0 0 0 0 0 0 8
		 *										  ^
		 * => node value must be offsetted by length difference
		 * => &(nodeValue[nodeValueLength - dataValueLength]) => &(nodeValue[8 - 4])
		 */

		cmpres = memcmp(recdata, &(node->value->value[node->value->length - datalen]), datalen);
	}
	

	/* Compare values according to op */
	/* memcmp return 0 if operands are equal, so it must be negated for OP_EQUAL */
	switch (node->op) {
	case OP_EQUAL:			/* field == value */
		return !cmpres;
	case OP_NOT_EQUAL:		/* field != value */
		return cmpres;
	case OP_LESS_EQUAL:		/* field <= value */
		return cmpres <= 0;
	case OP_LESS:			/* field < value */
		return cmpres < 0;
	case OP_GREATER_EQUAL:	/* field >= value */
		return cmpres >= 0;
	case OP_GREATER:		/* field > value */
		return cmpres > 0;
	default:				/* suppress compiler warning */
		return false;
	}
}

/**
 * \brief Check whether string in data record fits with node
 *
 * \param[in] node Filter tree node
 * \param[in] record IPFIX data record
 * \return true if data record's field fits
 */
bool filter_fits_string(struct filter_treenode *node, struct ipfix_record *record)
{
	int datalen = 0, vallen = node->value->length;
	char *pos = NULL, *prevpos = NULL;
	bool result = false;

	/* Get data from record */
	uint8_t *recdata = data_record_get_field(record->record, record->templ, node->field->enterprise, node->field->id, &datalen);
	if (!recdata) {
		return node->op == OP_NOT_EQUAL;
	}

	/* recdata is string without terminating '\0' - append it */
	char *data = malloc(datalen + 1);
	memcpy(data, recdata, datalen);
	data[datalen] = '\0';

	/* Find substring in string */
	pos = strstr(data, (char *) node->value->value);

	switch (node->op) {
	case OP_NONE:
		/* Success == substring found */
		result = (bool) pos;
		break;
	case OP_EQUAL:
		/* Success == strings are equal */
		result = pos && datalen == vallen;
		break;
	case OP_NOT_EQUAL:
		/* Success == strings are different */
		result = !(pos && datalen == vallen);
		break;
	case OP_LESS:
		/* String must end with substring */
		while (pos) {
			prevpos = pos++;
			pos = strstr(pos, (char *) node->value->value);
		}
		pos = prevpos;
		result = pos == (char *) &(data[datalen - vallen]);
		break;
	case OP_GREATER:
		/* String must begin with substring */
		result = pos == (char *) data;
		break;
	default:
		/* Unsupported operation */
		result = false;
	}
	
	free(data);
	return result;
}

/**
 * \brief Compare prefix with given address
 * 
 * \param[in] prefix prefix
 * \param[in] addr address
 * \return true when prefix contains address
 */
bool filter_compare_prefix(struct filter_prefix *prefix, uint8_t *addr)
{
	/* Prefixes are compared in two stages:
	 * 
	 * 1) compare full bytes (memcmp etc.)
	 * 2) compare remaining bits
	 * 
	 * this is a lot faster then comparing full address only by bits
	 */
	
	/* Compare full bytes */
	if (prefix->fullBytes > 0) {
		if (memcmp(addr, prefix->data, prefix->fullBytes) != 0) {
			return false;
		}
	}
	
	/* Compare remaining bits */
	int i, bit;
	for (i = 0; i < prefix->bits; ++i) {
		/* Comparing from left-most bit */
		bit = 7 - i;
			
		if ((addr[prefix->fullBytes] & (1 << bit)) != (prefix->data[prefix->fullBytes] & (1 << bit))) {
			return false;
		}
	}
	
	return true;
}

/**
 * \brief Match prefix
 * 
 * \param[in] node Tree node
 * \param[in] msg ipfix message
 * \param[in] record data record
 * \return true when node matches
 */
bool filter_fits_prefix(struct filter_treenode *node, struct ipfix_message *msg, struct ipfix_record *record)
{
	bool match;
	struct filter_prefix *prefix = (struct filter_prefix *) node->value->value;
	
	/* Check header field */
	if (node->field->type == FT_HEADER) {
		struct input_info_network *info = (struct input_info_network*) msg->input_info;
		
		if (node->field->id == HF_SRCIP) {
			return filter_compare_prefix(prefix, (void *) &(info->src_addr));
		} else {
			/* Invalid comparison */
			return false;
		}
		
	} else {
		/* Check header field */

		int datalen = 0;
		uint8_t *addr = data_record_get_field((uint8_t *)record->record, record->templ, node->field->enterprise, node->field->id, &datalen);

		if (!addr) {
			return node->op == OP_EQUAL;
		}

		match = filter_compare_prefix(prefix, addr);
	}
	
	return (node->op == OP_NOT_EQUAL) ^ match;
}

/**
 * \brief Check whether string in data record fits with node's regex
 *
 * \param[in] node Filter tree node
 * \param[in] record IPFIX data record
 * \return true if data record's field fits
 */
bool filter_fits_regex(struct filter_treenode *node, struct ipfix_record *record)
{
	bool result = false;
	regex_t *regex = (regex_t *) node->value->value;
	int datalen = 0;

	/* Get data from record */
	uint8_t *recdata = data_record_get_field(record->record, record->templ, node->field->enterprise, node->field->id, &datalen);
	if (!recdata) {
		return node->op == OP_NOT_EQUAL;
	}

	/* recdata is string without terminating '\0' - append it */
	char *data = malloc(datalen + 1);
	memcpy(data, recdata, datalen);
	data[datalen] = '\0';

	/* Execute regex */
	result = !regexec(regex, data, 0, NULL, 0);

	free(data);
	return (node->op == OP_NOT_EQUAL) ^ result;
}

/**
 * \brief Check whether data record contains given field
 *
 * \param[in] node Filter tree node
 * \param[in] data IPFIX data record
 * \return true if data record's field fits
 */
bool filter_fits_exists(struct filter_treenode *node, struct ipfix_record *data)
{
	/* Header field always exists */
	if (node->field->type == FT_HEADER) {
		return true;
	}
	
	return data_record_get_field(data->record, data->templ, node->field->enterprise, node->field->id, NULL);
}

/**
 * \brief Check whether node (and it's children) fits on data record
 *
 * \param[in] node Filter tree node
 * \param[in] msg IPFIX message (filter may contain field from message header)
 * \param[in] data IPFIX data record
 * \return true if data record's field fits
 */
bool filter_fits_node(struct filter_treenode *node, struct ipfix_message *msg, struct ipfix_record *data)
{
	/**
	 * return result modified by negation flag
	 * it is the same as 'return (node->negate) ? !value : value'
	 */
	switch (node->type) {
	case NODE_AND:
		return (node->negate) ^ (filter_fits_node(node->left, msg, data) && filter_fits_node(node->right, msg, data));
	case NODE_OR:
		return (node->negate) ^ (filter_fits_node(node->left, msg, data) || filter_fits_node(node->right, msg, data));
	case NODE_EXISTS:
		return (node->negate) ^ (filter_fits_exists(node, data));
	default: /* LEAF node */
		switch (node->value->type) {
		case VT_STRING:
			return (node->negate) ^ filter_fits_string(node, data);
		case VT_REGEX:
			return (node->negate) ^ filter_fits_regex(node, data);
		case VT_PREFIX:
			return (node->negate) ^ filter_fits_prefix(node, msg, data);
		default:
			return (node->negate) ^ filter_fits_value(node, msg, data);
		}
	}
}

/**
 * \brief Parse field name
 */
struct filter_field *filter_parse_field(char *name, xmlDoc *doc, xmlXPathContextPtr context)
{
	xmlChar xpath[100];
	xmlXPathObjectPtr result;
	xmlChar *tmp = NULL;
	
	/* Allocate memory */
	struct filter_field *field = calloc(1, sizeof(struct filter_field));
	if (!field) {
		MSG_ERROR(msg_module, "Not enough memory (%s:%d)", __FILE__, __LINE__);
		return NULL;
	}

	/* Check whether it is header field */
	int i;
	for (i = 0; header_fields[i]; ++i) {
		if (!strcasecmp(name, header_fields[i])) {
			field->type = FT_HEADER;
			field->id = i;
			return field;
		}
	}
	
	/* Prepare XPath */
	sprintf((char *) xpath, "/ipfix-elements/element[name='%s']", name);
	result = xmlXPathEvalExpression(xpath, context);

	if (result == NULL) {
		MSG_ERROR(msg_module, "Error in xmlXPathEvalExpression\n");
		free(field);
		return NULL;
	}

	if (xmlXPathNodeSetIsEmpty(result->nodesetval)) {
		xmlXPathFreeObject(result);
		MSG_ERROR(msg_module, "Unknown field '%s'!", name);
		free(field);
		return NULL;
	} else {
		xmlNode *info = result->nodesetval->nodeTab[0]->xmlChildrenNode;
		
		/* Go through element informations */
		while (info) {
			/* Get enterprise number */
			if (!xmlStrcmp(info->name, (const xmlChar *) "enterprise")) {
				tmp = xmlNodeListGetString(doc, info->xmlChildrenNode, 1);
				field->enterprise = strtoul((char *) tmp, NULL, 10);
				xmlFree(tmp);
			/* Get field ID */
			} else if (!xmlStrcmp(info->name, (const xmlChar *) "id")) {
				tmp = xmlNodeListGetString(doc, info->xmlChildrenNode, 1);
				field->id = strtoul((char *) tmp, NULL, 10);
				xmlFree(tmp);
			}
			
			info = info->next;
		}
	}

	xmlXPathFreeObject(result);
	return field;
}

/**
 * \brief Parse raw field name
 */
struct filter_field *filter_parse_rawfield(char *rawfield)
{
	char *idptr = NULL;
	
	/* Allocate memory */
	struct filter_field *field = calloc(1, sizeof(struct filter_field));
	if (!field) {
		MSG_ERROR(msg_module, "Not enough memory (%s:%d)", __FILE__, __LINE__);
		return NULL;
	}
	
	/* Get enterprise number and element ID  (eXidYY) */
	field->enterprise = strtoul(rawfield + 1, &idptr, 10);
	field->id = strtoul(idptr + 2, NULL, 10);
	
	return field;
}

/**
 * \brief Create a pointer with given value
 *
 * \param[in] data Source value
 * \param[in] length Data length (pointer size)
 * \return New pointer
 */
uint8_t *filter_num_to_ptr(uint8_t *data, int length)
{
	uint8_t *value = malloc(length);
	if (!value) {
		MSG_ERROR(msg_module, "Not enough memory (%s:%d)", __FILE__, __LINE__);
		return NULL;
	}

	/* Convert value to network byte order */
	uint16_t i;
	for (i = 0; i < length; ++i) {
		value[length - i - 1] = data[i];
	}

	return value;
}

/**
 * \brief Parse number in format [0-9]+[kKmMgGtT]
 *
 * \param[in] number Number
 * \return Numeric value
 */
struct filter_value *filter_parse_number(char *number)
{
	struct filter_value *val = malloc(sizeof(struct filter_value));
	if (!val) {
		MSG_ERROR(msg_module, "Not enough memory (%s:%d)", __FILE__, __LINE__);
		return NULL;
	}

	/* Check suffix */
	uint64_t tmp = strlen(number);
	long mult = 1;
	switch (number[tmp - 1]) {
	case 'k':
	case 'K':
		mult = 1000;
		break;
	case 'm':
	case 'M':
		mult = 1000000;
		break;
	case 'g':
	case 'G':
		mult = 1000000000;
		break;
	case 't':
	case 'T':
		mult = 1000000000000;
		break;
	}

	/* Apply suffix */
	tmp = strtol(number, NULL, 10) * mult;

	/* Create value */
	val->type = VT_NUMBER;
	val->length = sizeof(uint64_t);
	val->value = filter_num_to_ptr((uint8_t *) &tmp, val->length);
	return val;
}

/**
 * \brief Parse hexadecimal number
 */
struct filter_value *filter_parse_hexnum(char *hexnum)
{
	struct filter_value *val = malloc(sizeof(struct filter_value));
	if (!val) {
		MSG_ERROR(msg_module, "Not enough memory (%s:%d)", __FILE__, __LINE__);
		return NULL;
	}

	val->type = VT_NUMBER;

	/* Convert hexa number */
	uint64_t tmp = strtol(hexnum, NULL, 16);

	val->length = sizeof(uint64_t);
	val->value = filter_num_to_ptr((uint8_t *) &tmp, val->length);
	return val;
}

/**
 * \brief Parse string
 */
struct filter_value *filter_parse_string(char *string)
{
	struct filter_value *val = malloc(sizeof(struct filter_value));
	if (!val) {
		MSG_ERROR(msg_module, "Not enough memory (%s:%d)", __FILE__, __LINE__);
		return NULL;
	}

	int len = strlen(string) + 1;

	val->type = VT_STRING;
	val->value = malloc(len);
	if (!val->value) {
		MSG_ERROR(msg_module, "Not enough memory (%s:%d)", __FILE__, __LINE__);
		free(val);
		return NULL;
	}

	/* Add terminating '\0' */
	memcpy(val->value, string, len - 1);
	val->value[len - 1] = '\0';
	val->length = strlen((char *) val->value);
	return val;
}

/**
 * \brief Parse regular expression
 */
struct filter_value *filter_parse_regex(char *regexstr)
{
	int reglen;
	
	/* Allocate space for regex */
	regex_t *regex = calloc(1, sizeof(regex_t));
	if (!regex) {
		MSG_ERROR(msg_module, "Not enough memory (%s:%d)", __FILE__, __LINE__);
		return NULL;
	}

	reglen = strlen(regexstr) + 1;
	char *reg = malloc(reglen);
	if (!reg) {
		MSG_ERROR(msg_module, "Not enough memory (%s:%d)", __FILE__, __LINE__);
		free(regex);
		return NULL;
	}

	memcpy(reg, regexstr, reglen - 1);
	reg[reglen - 1] = '\0';

	/* REG_NOSUB == we don't need positions of matches */
	if (regcomp(regex, reg, REG_NOSUB)) {
		MSG_ERROR(msg_module, "Can't compile regular expression '%s'", reg);
		free(regex);
		free(reg);
		return NULL;
	}

	/* Create value */
	struct filter_value *val = malloc(sizeof(struct filter_value));
	if (!val) {
		MSG_ERROR(msg_module, "Not enough memory (%s:%d)", __FILE__, __LINE__);
		free(regex);
		free(reg);
		return NULL;
	}

	val->type = VT_REGEX;
	val->value = (uint8_t *) regex;

	free(reg);
	return val;
}

/**
 * \brief Parse IPv4 address
 */
struct filter_value *filter_parse_ipv4(char *addr)
{
	struct filter_value *val = malloc(sizeof(struct filter_value));
	if (!val) {
		MSG_ERROR(msg_module, "Not enough memory (%s:%d)", __FILE__, __LINE__);
		return NULL;
	}

	val->type = VT_NUMBER;

	struct in_addr tmp;

	/* Convert address */
	if (inet_pton(AF_INET, addr, &tmp) != 1) {
		MSG_ERROR(msg_module, "Cannot parse IP address %s", addr);
		free(val);
		return NULL;
	}

	/* Create value */
	val->length = sizeof(struct in_addr);
	val->value = filter_num_to_ptr((uint8_t *) &tmp, val->length);

	return val;
}

/**
 * \brief Parse IPv6 address
 */
struct filter_value *filter_parse_ipv6(char *addr)
{
	struct filter_value *val = malloc(sizeof(struct filter_value));
	if (!val) {
		MSG_ERROR(msg_module, "Not enough memory (%s:%d)", __FILE__, __LINE__);
		return NULL;
	}

	val->type = VT_NUMBER;

	struct in6_addr tmp;

	/* Convert address */
	if (inet_pton(AF_INET6, addr, &tmp) != 1) {
		MSG_ERROR(msg_module, "Cannot parse IP address %s", addr);
		free(val);
		return NULL;
	}

	/* Create value */
	val->length = sizeof(struct in6_addr);
	val->value = filter_num_to_ptr((uint8_t *) &tmp, val->length);

	return val;
}

/**
 * \brief Parse prefix
 * 
 * \param family IP version (AF_INET / AF_INET6)
 * \param addr IP address
 * \return parsed value
 */
struct filter_value *filter_parse_prefix(int family, char *addr)
{
	/* Allocate space for value */
	struct filter_value *val = malloc(sizeof(struct filter_value));
	if (!val) {
		MSG_ERROR(msg_module, "Not enough memory (%s:%d)", __FILE__, __LINE__);
		return NULL;
	}
	
	/* Allocate space for prefix data */
	struct filter_prefix *prefix = malloc(sizeof(struct filter_prefix));
	if (!prefix) {
		MSG_ERROR(msg_module, "Not enough memory (%s:%d)", __FILE__, __LINE__);
		free(val);
		return NULL;
	}
	
	/* Find prefix length */
	char *slash = strchr(addr, '/');
	uint16_t prefixLen = atoi(slash + 1);
	
	prefix->fullBytes = prefixLen / 8;
	prefix->bits = prefixLen % 8;
	
	/* Process address */
	char onlyAddr[50];
	memcpy(onlyAddr, addr, slash - addr);
	onlyAddr[slash - addr] = '\0';
	
	if (inet_pton(family, onlyAddr, prefix->data) != 1) {
		MSG_ERROR(msg_module, "Cannot parse IP prefix %s", addr);
		free(val);
		free(prefix);
		return NULL;
	}
	
	/* Create value */
	val->type = VT_PREFIX;
	val->length = prefixLen;
	val->value = (uint8_t*) prefix;
	
	return val;
}

/**
 * \brief Parse IPv4 prefix
 */
struct filter_value *filter_parse_prefix4(char* addr)
{
	return filter_parse_prefix(AF_INET, addr);
}

/**
 * \brief Parse IPv4 prefix
 */
struct filter_value *filter_parse_prefix6(char* addr)
{
	return filter_parse_prefix(AF_INET6, addr);
}

/**
 * \brief Parse timestamp
 */
struct filter_value *filter_parse_timestamp(char *tstamp)
{
	struct tm ctime;

	if (strptime(tstamp, "%Y/%m/%d.%H:%M:%S", &ctime) == NULL) {
		MSG_ERROR(msg_module, "Cannot parse timestamp %s", tstamp);
		return NULL;
	}

	ctime.tm_isdst = 0;

	/* Get time in seconds */
	uint64_t tmp = mktime(&ctime);

	/* Check suffix */
	switch (tstamp[strlen(tstamp) - 1]) {
	case 's':
		break;
	case 'm':
		tmp *= 1000;
		break;
	case 'u':
		tmp *= 1000000;
		break;
	case 'n':
		tmp *= 1000000000;
		break;
	}

	/* Create value */
	struct filter_value *val = malloc(sizeof(struct filter_value));
	if (!val) {
		MSG_ERROR(msg_module, "Not enough memory (%s:%d)", __FILE__, __LINE__);
		return NULL;
	}

	val->type = VT_NUMBER;
	val->length = sizeof(uint64_t);
	val->value = filter_num_to_ptr((uint8_t *) &tmp, val->length);

	return val;
}

/**
 * \brief Decode operator
 *
 * \param[in] op Operator
 * \return Numeric value of operator
 */
enum operators filter_decode_operator(char *op)
{
	if (!strcmp(op, "=") || !strcmp(op, "==")) {
		return OP_EQUAL;
	} else if (!strcmp(op, "!=")) {
		return OP_NOT_EQUAL;
	} else if (!strcmp(op, "<")) {
		return OP_LESS;
	} else if (!strcmp(op, "<=") || !strcmp(op, "=<")) {
		return OP_LESS_EQUAL;
	} else if (!strcmp(op, ">")) {
		return OP_GREATER;
	} else if (!strcmp(op, ">=") || !strcmp(op, "=>")) {
		return OP_GREATER_EQUAL;
	}

	/* Implicit operator */
	return OP_EQUAL;
}

/**
 * \brief Create new leaf treenode
 */
struct filter_treenode *filter_new_leaf_node(struct filter_field *field, char *op, struct filter_value *value)
{
	struct filter_treenode *node = calloc(1, sizeof(struct filter_treenode));
	if (!node) {
		MSG_ERROR(msg_module, "Not enough memory (%s:%d)", __FILE__, __LINE__);
		return NULL;
	}

	node->value = value;
	node->field = field;
	node->type = NODE_LEAF;
	node->op = filter_decode_operator(op);

	/* Convert ODID value to network byte order */
	if (node->field->type == FT_HEADER && node->field->id == HF_ODID) {
		*((uint32_t *) node->value->value) = ntohl(*((uint32_t *) node->value->value));
	}
	
	/* Check comparison compatibility */
	if (node->field->type == FT_HEADER) {
		switch (node->field->id) {
		case HF_DSTIP:
		case HF_SRCIP:
			if (node->value->type == VT_STRING) {
				MSG_ERROR(msg_module, "Cannot compare address with string");
				free(node);
				return NULL;
			}
			
			break;
			
		case HF_DSTPORT:
		case HF_SRCPORT:
		case HF_ODID:
			if (node->value->type != VT_NUMBER) {
				MSG_ERROR(msg_module, "Ports and ODID can only be compared with number");
				free(node);
				return NULL;
			}
		default:
			break;
		}
	}
	
	return node;
}

/**
 * \brief Create new leaf treenode without specified operator
 */
struct filter_treenode *filter_new_leaf_node_opless(struct filter_field *field, struct filter_value *value)
{
	/*
	 * For string values - no operator means "find this substring".
	 * For numeric values it is the same as "="
	 */
	struct filter_treenode *node = filter_new_leaf_node(field, "=", value);
	if (node && value->type == VT_STRING) {
		node->op = OP_NONE;
	}

	return node;
}

/**
 * \brief Decode node type
 */
enum nodetype filter_decode_type(char *type)
{
	if (!strcmp(type, "and") || !strcmp(type, "AND") || !strcmp(type, "&&")) {
		return NODE_AND;
	}
	
	return NODE_OR;
}

/**
 * \brief Create new parent node
 */
struct filter_treenode *filter_new_parent_node(struct filter_treenode *left, char *type, struct filter_treenode *right)
{
	struct filter_treenode *node = calloc(1, sizeof(struct filter_treenode));
	if (!node) {
		MSG_ERROR(msg_module, "Not enough memory (%s:%d)", __FILE__, __LINE__);
		return NULL;
	}

	node->left = left;
	node->right = right;
	node->type = filter_decode_type(type);

	return node;
}

/**
 * \brief Set node negated
 */
void filter_node_set_negated(struct filter_treenode *node)
{
	if (node) {
		node->negate = true;
	}
}

/**
 * \brief Set profile root node
 */
void filter_set_root(struct filter_profile *profile, struct filter_treenode *node)
{
	if (profile && node) {
		profile->root = node;
	}
}

/**
 * \brief Print error message from filter parser
 */
void filter_error(const char *msg, YYLTYPE *loc)
{
	MSG_ERROR(msg_module, "%d: %s", loc->last_column, msg);
}

/**
 * \brief Create new EXISTS node
 */
struct filter_treenode *filter_new_exists_node(struct filter_field *field)
{
	struct filter_treenode *node = calloc(1, sizeof(struct filter_treenode));
	if (!node) {
		MSG_ERROR(msg_module, "Not enough memory (%s:%d)", __FILE__, __LINE__);
		return NULL;
	}

	node->type = NODE_EXISTS;
	node->field = field;
	return node;
}
