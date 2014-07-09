/**
 * \file filter.c
 * \author Michal Kozubik <kozubik@cesnet.cz>
 * \brief Intermediate plugin for IPFIX data filtering
 *
 * Copyright (C) 2014 CESNET, z.s.p.o.
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
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpathInternals.h>

#include <ipfixcol.h>

#include "filter.h"
#include "scanner.h"
#include "parser.h"

#include "../../intermediate_process.h"
#include "../../ipfix_message.h"

static const char *msg_module = "filter";

#define DEFAULT_ELEMENTS_FILE "/etc/ipfixcol/ipfix-elements.xml"

#define CHECK_ALLOC(check_alloc_ptr) \
	if (!(check_alloc_ptr)) { \
		MSG_ERROR(msg_module, "Not enought memory (%s:%d)", __FILE__, __LINE__); \
		return NULL; \
	}

struct filter_process {
	uint8_t *ptr;
	int *offset;
	struct filter_profile *profile;
};

/**
 * \brief Free tree structure
 */
void filter_free_tree(struct filter_treenode *node)
{
	if (!node) {
		return;
	}

	filter_free_tree(node->left);
	filter_free_tree(node->right);

	if (node->value) {
		if (node->value->value) {
			free(node->value->value);
		}
		free(node->value);
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
	struct filter_source *aux_src = profile->sources;

	while (aux_src) {
		profile->sources = profile->sources->next;
		free(aux_src);
		aux_src = profile->sources;
	}

	filter_free_tree(profile->root);
	free(profile);
}

/**
 * \brief Initialize ipfix-elements.xml
 *
 * \param[in] pdata Parser data structure
 */
void filter_init_elements(struct filter_parser_data *pdata)
{
	pdata->doc = xmlReadFile(DEFAULT_ELEMENTS_FILE, NULL, 0);
	if (!pdata->doc) {
		MSG_ERROR(msg_module, "Unable to parse elements configuration file %s", DEFAULT_ELEMENTS_FILE);
		return;
	}

	pdata->context = xmlXPathNewContext(pdata->doc);
	if (pdata->context == NULL) {
		MSG_ERROR(msg_module, "Error in xmlXPathNewContext");
		return;
	}
}

/**
 * \brief Initialize filter plugin
 *
 * \param[in] params Plugin parameters
 * \param[in] ip_config Internal process configuration
 * \param[in] ip_id Source ID into Template Manager
 * \param[in] template_mgr Template Manager
 * \param[out] config Plugin configuration
 * \return 0 if everything OK
 */
int intermediate_plugin_init(char *params, void *ip_config, uint32_t ip_id, struct ipfix_template_mgr *template_mgr, void **config)
{
	(void) ip_id; (void) template_mgr;
	struct filter_config *conf = NULL;
	struct filter_profile *aux_profile = NULL;
	struct filter_source *aux_src = NULL;
	struct filter_parser_data parser_data;

	xmlDoc *doc = NULL;
	xmlNode *root = NULL, *profile = NULL, *node = NULL;
	xmlChar *aux_char;

	int ret;

	conf = (struct filter_config *) calloc(1, sizeof(struct filter_config));
	if (!conf) {
		MSG_ERROR(msg_module, "Unable to allocate memory (%s:%d)", __FILE__, __LINE__);
		goto cleanup_err;
	}

	if (!params) {
		MSG_ERROR(msg_module, "Missing plugin configuration!");
		goto cleanup_err;
	}

	doc = xmlParseDoc(BAD_CAST params);
	if (!doc) {
		MSG_ERROR(msg_module, "Cannot parse config xml!");
		goto cleanup_err;
	}

	root = xmlDocGetRootElement(doc);
	if (!root) {
		MSG_ERROR(msg_module, "Cannot get document root element!");
		goto cleanup_err;
	}

	filter_init_elements(&parser_data);

	/* Iterate throught all profiles */
	for (profile = root->children; profile; profile = profile->next) {
		parser_data.filter = NULL;

		/* Allocate space for profile */
		aux_profile = calloc(1, sizeof(struct filter_profile));
		if (!aux_profile) {
			MSG_ERROR(msg_module, "Unable to allocate memory (%s:%d)", __FILE__, __LINE__);
			goto cleanup_err;
		}
		/* Set new ODID */
		aux_char = xmlGetProp(profile, (const xmlChar *) "to");
		aux_profile->new_odid = atoi((char *) aux_char);
		xmlFree(aux_char);

		/* Get filter string and all sources */
		for (node = profile->children; node; node = node->next) {
			if (!xmlStrcmp(node->name, (const xmlChar *) "from")) {
				/* New source */
				aux_src = calloc(1, sizeof(struct filter_source));
				if (!aux_src) {
					MSG_ERROR(msg_module, "Unable to allocate memory (%s:%d)", __FILE__, __LINE__);
					free(aux_profile);
					goto cleanup_err;
				}
				aux_char = xmlNodeListGetString(doc, node->children, 1);
				aux_src->id = atoi((char *) aux_char);
				xmlFree(aux_char);

				/* Insert new source into list */
				if (!aux_profile->sources) {
					aux_profile->sources = aux_src;
				} else {
					aux_src->next = aux_profile->sources;
					aux_profile->sources = aux_src;
				}
			} else if (!xmlStrcmp(node->name, (const xmlChar *) "filterString")) {
				/* Filter string found */
				parser_data.filter = (char *) xmlNodeListGetString(doc, node->children, 1);
			}
		}

		/* No filter string -> no profile */
		if (!parser_data.filter) {
			free(aux_profile);
			continue;
		}

		parser_data.profile = aux_profile;

		/* Prepare scanner */
		yylex_init(&parser_data.scanner);
		YY_BUFFER_STATE bp = yy_scan_string(parser_data.filter, parser_data.scanner);
		yy_switch_to_buffer(bp, parser_data.scanner);

		/* Parse filter */
		ret = yyparse(&parser_data);

		/* Clear scanner */
		yy_flush_buffer(bp, parser_data.scanner);
		yy_delete_buffer(bp, parser_data.scanner);
		yylex_destroy(parser_data.scanner);
		free(parser_data.filter);

		if (ret) {
			MSG_ERROR(msg_module, "Error while parsing filter - skipping profile");
			filter_free_profile(aux_profile);
			continue;
		}

		/* This is default profile */
		if (!xmlStrcasecmp(profile->name, (const xmlChar *) "default")) {
			if (conf->default_profile) {
				MSG_WARNING(msg_module, "Multiple default profiles, using the first one!");
				free(aux_profile);
			} else {
				conf->default_profile = aux_profile;
			}

			continue;
		}


		/* Insert new profile into list */
		if (!conf->profiles) {
			conf->profiles = aux_profile;
		} else {
			aux_profile->next = conf->profiles;
			conf->profiles = aux_profile;
		}
	}

	conf->ip_config = ip_config;

	*config = conf;
	xmlFreeDoc(doc);
	xmlXPathFreeContext(parser_data.context);
	xmlFreeDoc(parser_data.doc);

	MSG_NOTICE(msg_module, "Initialized");
	return 0;

cleanup_err:
	if (!conf) {
		return -1;
	}

	if (doc) {
		xmlFreeDoc(doc);
	}

	if (parser_data.doc) {
		xmlXPathFreeContext(parser_data.context);
		xmlFreeDoc(parser_data.doc);
	}

	aux_profile = conf->profiles;

	while (aux_profile) {
		conf->profiles = conf->profiles->next;
		filter_free_profile(aux_profile);
		aux_profile = conf->profiles;
	}

	if (conf->default_profile) {
		filter_free_profile(conf->default_profile);
	}

	free(conf);
	return -1;
}

/**
 * \brief Compare values byte by byte and return according bool value
 *
 * \param[in] first First value
 * \param[in] second Second value
 * \param[in] datalen length of values
 * \param[in] less    Return value when first value is less than second
 * \param[in] greater Return value when first value is greater than second
 * \param[in] equal   Return value when both values are equal
 * \return less, greater or equal value
 */
bool filter_compare_values(uint8_t *first, uint8_t *second, int datalen, bool less, bool greater, bool equal)
{
	int i;
	/**
	 * Start with highest bytes of values
	 * If byte of first and second value equals, compare lower two
	 * Continue until i == datalen (values are equal) or some bytes differs
	 */
	for (i = 0; i < datalen; ++i) {
		if (first[i] < second[i]) {
			return less;
		} else if (first[i] > second[i]) {
			return greater;
		}
	}

	return equal;
}

/**
 * \brief Check whether value in data record fits with node expression
 *
 * \param[in] node Filter tree node
 * \param[in] rec Data record
 * \param[in] templ Data record's template
 * \return true if data record's field fits
 */
bool filter_fits_value(struct filter_treenode *node, uint8_t *rec, struct ipfix_template *templ)
{
	int datalen;
	uint8_t *recdata = data_record_get_field(rec, templ, node->field, &datalen);
	if (!recdata) {
		/* Field not found - if op is '!=' it is success */
		return node->op == OP_NOT_EQUAL;
	}

	/* Compare values according to op */
	switch (node->op) {
	case OP_EQUAL:
		return !memcmp(recdata, node->value->value, datalen);
	case OP_NOT_EQUAL:
		return memcmp(recdata, node->value->value, datalen);
	case OP_LESS_EQUAL:
		return filter_compare_values(recdata, node->value->value, datalen, true, false, true);
	case OP_LESS:
		return filter_compare_values(recdata, node->value->value, datalen, true, false, false);
	case OP_GREATER_EQUAL:
		return filter_compare_values(recdata, node->value->value, datalen, false, true, true);
	case OP_GREATER:
		return filter_compare_values(recdata, node->value->value, datalen, false, true, false);
	default:
		return false;
	}
}

/**
 * \brief Check whether string in data record fits with node
 *
 * \param[in] node Filter tree node
 * \param[in] rec Data record
 * \param[in] templ Data record's template
 * \return true if data record's field fits
 */
bool filter_fits_string(struct filter_treenode *node, uint8_t *rec, struct ipfix_template *templ)
{
	(void) node; (void) rec; (void) templ;
	return false;
}

/**
 * \brief Check whether data record contains given field
 *
 * \param[in] node Filter tree node
 * \param[in] rec Data record
 * \param[in] templ Data record's template
 * \return true if data record's field fits
 */
bool filter_fits_exists(struct filter_treenode *node, uint8_t *rec, struct ipfix_template *templ)
{
	return data_record_get_field(rec, templ, node->field, NULL);
}

/**
 * \brief Check whether node (and it's children) fits on data record
 *
 * \param[in] node Filter tree node
 * \param[in] rec Data record
 * \param[in] templ Data record's template
 * \return true if data record's field fits
 */
bool filter_fits_node(struct filter_treenode *node, uint8_t *rec, struct ipfix_template *templ)
{
	/**
	 * return result modified by negation flag
	 * it is the same as 'return (node->negate) ? !value : value'
	 */
	switch (node->type) {
	case NODE_AND:
		return (node->negate) ^ (filter_fits_node(node->left, rec, templ) && filter_fits_node(node->right, rec, templ));
	case NODE_OR:
		return (node->negate) ^ (filter_fits_node(node->left, rec, templ) || filter_fits_node(node->right, rec, templ));
	case NODE_EXISTS:
		return (node->negate) ^ (filter_fits_exists(node, rec, templ));
	default:
		if (node->value->type == VT_NUMBER) {
			return (node->negate) ^ filter_fits_value(node, rec, templ);
		} else {
			return (node->negate) ^ filter_fits_string(node, rec, templ);
		}
	}
}

/**
 * \brief Copy (options) template sets from original message
 *
 * \param[in] msg Original message
 * \param[in] ptr Destination
 * \param[in] offset Offset in new message
 */
void filter_add_template_sets(struct ipfix_message *msg, uint8_t *ptr, int *offset)
{
	int length, i;

	/* Copy template sets */
	for (i = 0; i < 1024 && msg->templ_set[i]; ++i) {
		length = ntohs(msg->templ_set[i]->header.length);
		memcpy(ptr + *offset, msg->templ_set[i], length);
		*offset += length;
	}

	/* Copy options template sets */
	for (i = 0; i < 1024 && msg->opt_templ_set[i]; ++i) {
		length = ntohs(msg->opt_templ_set[i]->header.length);
		memcpy(ptr + *offset, msg->opt_templ_set[i], length);
		*offset += length;
	}
}

/**
 * \brief Process one data record
 *
 * \param[in] rec Data record
 * \param[in] rec_len Data record's length
 * \param[in] templ Data record's template
 * \param[in] data Processing data
 */
void filter_process_data_record(uint8_t *rec, int rec_len, struct ipfix_template *templ, void *data)
{
	struct filter_process *conf = (struct filter_process *) data;

	if (filter_fits_node(conf->profile->root, rec, templ)) {
//		MSG_DEBUG(msg_module, "data record fits");
		memcpy(conf->ptr + *(conf->offset), rec, rec_len);
		*(conf->offset) += rec_len;

		tm_template_reference_inc(templ);
	} else {
//		MSG_DEBUG(msg_module, "data record DOES NOT fit");
	}
}

/**
 * \brief Apply profile filter on message and change ODID if it fits
 *
 * \param[in] msg IPFIX message
 * \param[in] profile Filter profile
 * \return pointer to new ipfix message
 */
struct ipfix_message *filter_apply_profile(struct ipfix_message *msg, struct filter_profile *profile)
{
	struct ipfix_message *new_msg = NULL;
	struct ipfix_header *header = NULL;
	struct filter_process conf;
	int i, j, offset = 0, oldoffset;
	uint8_t *ptr = NULL;

	/* Allocate space */
	ptr = calloc(1, ntohs(msg->pkt_header->length));
	CHECK_ALLOC(ptr);

	conf.offset = &offset;
	conf.ptr = ptr;
	conf.profile = profile;

	/* Copy header */
	memcpy(ptr, msg->pkt_header, IPFIX_HEADER_LENGTH);
	offset += IPFIX_HEADER_LENGTH;

	/* Copy (options) template sets */
	filter_add_template_sets(msg, ptr, &offset);

	for (i = 0; i < 1024 && msg->data_couple[i].data_set; ++i) {
		oldoffset = offset;
		/* Copy set header */
		memcpy(ptr + offset, &(msg->data_couple[i].data_set->header), sizeof(struct ipfix_set_header));
		offset += sizeof(struct ipfix_set_header);

		/* Process data records */
		data_set_process_records(msg->data_couple[i].data_set, msg->data_couple[i].data_template, &filter_process_data_record, (void *) &conf);

		if (offset == oldoffset + 4) {
			/* No data records were copied, rollback */
			offset = oldoffset;
			continue;
		}

		/* Update data set length */
		((struct ipfix_set_header *) ((uint8_t *) ptr + oldoffset))->length = htons(offset - oldoffset);
	}

	if (offset == IPFIX_HEADER_LENGTH) {
		/* empty message */
		free(ptr);
		return NULL;
	}

	/* Modify header and create IPFIX message */
	header = (struct ipfix_header *) ptr;
	header->length = ntohs(offset);
	header->observation_domain_id = htonl(profile->new_odid);

	/* Create new IPFIX message */
	new_msg = message_create_from_mem(ptr, offset, msg->input_info, msg->source_status);

	/* Match data couples */
	for (i = 0; i < 1024 && new_msg->data_couple[i].data_set; ++i) {
		for (j = 0; j < 1024 && msg->data_couple[j].data_set; ++j) {
			if (new_msg->data_couple[i].data_set->header.flowset_id == msg->data_couple[j].data_set->header.flowset_id) {
				new_msg->data_couple[i].data_template = msg->data_couple[j].data_template;
				break;
			}
		}
	}

	return new_msg;
}

int process_message(void *config, void *message)
{
	struct ipfix_message *msg = (struct ipfix_message *) message, *new_msg;
	struct filter_config *conf = (struct filter_config *) config;
	struct filter_profile *aux_profile = NULL;
	struct filter_source *aux_src = NULL;
	uint32_t orig_odid = ntohl(msg->pkt_header->observation_domain_id);
	int profiles = 0;

	/* Go throught all profiles */
	for (aux_profile = conf->profiles; aux_profile; aux_profile = aux_profile->next) {
		/* Go throught all sources for this profile */
		for (aux_src = aux_profile->sources; aux_src; aux_src = aux_src->next) {
			if (aux_src->id == orig_odid) {
				break;
			}
		}

		if (!aux_src) {
			/* Profile is not for this source */
			continue;
		}

		profiles++;

		new_msg = filter_apply_profile(msg, aux_profile);
		if (new_msg) {
			pass_message(conf->ip_config, (void *) new_msg);
		}
	}

	/* No profile for this source */
	if (!profiles) {
		if (conf->default_profile) {
			/* Use default profile */
			new_msg = filter_apply_profile(msg, conf->default_profile);
			if (new_msg) {
				pass_message(conf->ip_config, (void *) new_msg);
			}
		} else {
			/* No profile found for this ODID */
			pass_message(conf->ip_config, message);
			return 0;
		}
	}

	pass_message(conf->ip_config, message);
	return 0;
}

int intermediate_plugin_close(void *config)
{
	struct filter_config *conf = (struct filter_config *) config;
	struct filter_profile *aux_profile = conf->profiles;

	while (aux_profile) {
		conf->profiles = conf->profiles->next;
		filter_free_profile(aux_profile);
		aux_profile = conf->profiles;
	}

	if (conf->default_profile) {
		filter_free_profile(conf->default_profile);
	}

	free(conf);
	return 0;
}


/**
 * \brief Parse field name
 *
 * \param[in] field Field name
 * \param[in] doc XML document
 * \param[in] context XML context
 * \return Information Element ID or -1 on error
 */
int filter_parse_field(char *field, xmlDoc *doc, xmlXPathContextPtr context)
{
	xmlChar xpath[100];
	xmlChar *tmp = NULL;
	xmlXPathObjectPtr result;
	int res;

	/* Prepare XPath */
	sprintf((char *) xpath, "/ipfix-elements/element[name='%s']/id", field);
	result = xmlXPathEvalExpression(xpath, context);

	if (result == NULL) {
		MSG_ERROR(msg_module, "Error in xmlXPathEvalExpression\n");
		return -1;
	}

	if (xmlXPathNodeSetIsEmpty(result->nodesetval)) {
		xmlXPathFreeObject(result);
		MSG_ERROR(msg_module, "Unknown field '%s'!", field);
		return -1;
	} else {
		/* Get ID */
		tmp = xmlNodeListGetString(doc, result->nodesetval->nodeTab[0]->xmlChildrenNode, 1);
		res = atoi((char *) tmp);
	}

	xmlXPathFreeObject(result);
	xmlFree(tmp);
	return res;
}

/**
 * \brief Parse raw field name
 */
int filter_parse_rawfield(char *rawfield)
{
	return atoi(&(rawfield[2]));
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
	CHECK_ALLOC(value);

	memcpy(value, data, length);
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
	CHECK_ALLOC(val);

	long long tmp = strlen(number);
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

	tmp = strtol(number, NULL, 10) * mult;

	val->type = VT_NUMBER;
	val->value = filter_num_to_ptr((uint8_t *) &tmp, sizeof(long long));
	return val;
}

/**
 * \brief Parse hexadecimal number
 */
struct filter_value *filter_parse_hexnum(char *hexnum)
{
	struct filter_value *val = malloc(sizeof(struct filter_value));
	CHECK_ALLOC(val);

	val->type = VT_NUMBER;

	long tmp = strtol(hexnum, NULL, 16);

	val->value = filter_num_to_ptr((uint8_t *) &tmp, sizeof(long));
	return val;
}

/**
 * \brief Parse string
 */
struct filter_value *filter_parse_string(char *string)
{
	struct filter_value *val = malloc(sizeof(struct filter_value));
	CHECK_ALLOC(val);

	val->type = VT_STRING;
	val->value = malloc(strlen(string) - 1);
	CHECK_ALLOC(val->value);

	memcpy(val->value, string + 1, strlen(string) - 1);
	val->value[strlen(string) - 2] = 0;

	return val;
}

/**
 * \brief Parse IPv4 address
 */
struct filter_value *filter_parse_ipv4(char *addr)
{
	struct filter_value *val = malloc(sizeof(struct filter_value));
	CHECK_ALLOC(val);

	val->type = VT_NUMBER;

	struct in_addr tmp;

	if (inet_pton(AF_INET, addr, &tmp) != 1) {
		MSG_ERROR(msg_module, "Cannot parse IP address %s", addr);
		free(val);
		return NULL;
	}

	val->value= filter_num_to_ptr((uint8_t *) &tmp, sizeof(struct in_addr));

	return val;
}

/**
 * \brief Parse IPv6 address
 */
struct filter_value *filter_parse_ipv6(char *addr)
{
	struct filter_value *val = malloc(sizeof(struct filter_value));
	CHECK_ALLOC(val);

	val->type = VT_NUMBER;

	struct in6_addr tmp;

	if (inet_pton(AF_INET6, addr, &tmp) != 1) {
		MSG_ERROR(msg_module, "Cannot parse IP address %s", addr);
		free(val);
		return NULL;
	}

	val->value = filter_num_to_ptr((uint8_t *) &tmp, sizeof(struct in6_addr));

	return val;
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

	uint64_t tmp = mktime(&ctime);

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

	struct filter_value *val = malloc(sizeof(struct filter_value));
	CHECK_ALLOC(val);

	val->type = VT_NUMBER;
	val->value = filter_num_to_ptr((uint8_t *) &tmp, sizeof(uint64_t));

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
	/*
	 * there must be strNcmp because there is rest of filter string behind operator
	 * for example filter = "ie20 > 60" =>  op = "> 60"
	 */
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

	return OP_EQUAL;
}

/**
 * \brief Create new leaf treenode
 */
struct filter_treenode *filter_new_leaf_node(int field, char *op, struct filter_value *value)
{
	struct filter_treenode *node = calloc(1, sizeof(struct filter_treenode));
	CHECK_ALLOC(node);

	node->value = value;
	node->field = field;
	node->type = NODE_LEAF;
	node->op = filter_decode_operator(op);

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
	CHECK_ALLOC(node);

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

struct filter_treenode *filter_new_exists_node(int field)
{
	struct filter_treenode *node = calloc(1, sizeof(struct filter_treenode));
	CHECK_ALLOC(node);

	node->type = NODE_EXISTS;
	node->field = field;
	return node;
}

