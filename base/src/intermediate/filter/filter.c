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

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include <ipfixcol.h>

#include "filter.h"
#include "scanner.h"
#include "parser.h"

#include "../../intermediate_process.h"
#include "../../ipfix_message.h"

static const char *msg_module = "filter";

struct filter_source {
	uint32_t id;
	struct filter_source *next;
};


struct filter_config {
	void *ip_config;
	struct filter_profile *profiles;
	struct filter_profile *default_profile;
};

static char *ops[] = {"=", "<", "<=", ">", ">=", "!="};
char *filter_op(enum operators op)
{
	return ops[op];
}
void filter_print(struct filter_treenode *node)
{
	switch (node->type) {
	case NODE_AND:
		filter_print(node->left);
		MSG_DEBUG("", " && ");
		filter_print(node->right);
		return;
	case NODE_OR:
		filter_print(node->left);
		MSG_DEBUG("", " || ");
		filter_print(node->right);
		return;
	default:
		MSG_DEBUG("", "%d %s %d", node->field, filter_op(node->op), *((uint16_t *)node->value));
	}
}

/**
 * \brief Free tree structure
 */
void filter_free_tree(struct filter_treenode *node)
{
	if (!node) {
		return;
	}

	filter_tree_free(node->left);
	filter_tree_free(node->right);

	if (node->value) {
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
	free(profile->filter);
	free(profile);
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
	struct filter_profile *aux_profile;
	xmlDoc *doc = NULL;
	xmlNode *root = NULL, *profile = NULL, *node = NULL;
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
	xmlChar *aux_char;

	struct filter_source *aux_src = NULL;

	/* Iterate throught all profiles */
	for (profile = root->children; profile; profile = profile->next) {
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
				aux_profile->filter = (char *) xmlNodeListGetString(doc, node->children, 1);
			}
		}

		/* No filter string -> no profile */
		if (!aux_profile->filter) {
			free(aux_profile);
			continue;
		}

		/* Prepare scanner */
		yylex_init(&aux_profile->scanner);
		YY_BUFFER_STATE bp = yy_scan_string(aux_profile->filter, aux_profile->scanner);
		yy_switch_to_buffer(bp, aux_profile->scanner);

		/* Parse filter */
		ret = yyparse(aux_profile);

		/* Clear scanner */
		yy_flush_buffer(bp, aux_profile->scanner);
		yy_delete_buffer(bp, aux_profile->scanner);
		yylex_destroy(aux_profile->scanner);

		if (ret) {
			MSG_ERROR(msg_module, "Error while parsing filter - skipping profile");
			filter_free_profile(aux_profile);
			continue;
		}

//		filter_print(aux_profile->root);

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

	MSG_DEBUG(msg_module, "Initialized");
	return 0;

cleanup_err:
	if (!conf) {
		return -1;
	}

	if (doc) {
		xmlFreeDoc(doc);
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
 * \brief Check if field in data record matches value
 *
 * \param[in] rec Data record
 * \param[in] templ Data record's template
 * \param[in] id Field ID
 * \param[in] value Checked value
 * \return 0 if values is equal with value in data record
 */
int filter_data_record_match(uint8_t *rec, struct ipfix_template *templ, uint16_t id, uint8_t *value)
{
	uint8_t *data;
	int data_length;

	data = data_record_get_field(rec, templ, id, &data_length);

	if (!data) {
		return 1;
	}

	return memcmp(data, value, data_length);
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
 * \brief Check whether value in template record fits with node expression
 *
 * \param[in] node Filter tree node
 * \param[in] rec Data record
 * \param[in] templ Data record's template
 * \return true if data record's field fits
 */
bool filter_value_fits(struct filter_treenode *node, uint8_t *rec, struct ipfix_template *templ)
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
		return memcmp(recdata, node->value, datalen);
	case OP_NOT_EQUAL:
		return !memcmp(recdata, node->value, datalen);
	case OP_LESS_EQUAL:
		return filter_compare_values(recdata, node->value, datalen, true, false, true);
	case OP_LESS:
		return filter_compare_values(recdata, node->value, datalen, true, false, false);
	case OP_GREATER_EQUAL:
		return filter_compare_values(recdata, node->value, datalen, false, true, true);
	case OP_GREATER:
		return filter_compare_values(recdata, node->value, datalen, false, true, false);
	default:
		return false;
	}
}

/**
 * \brief Check whether node (and it's children) fits on data record
 *
 * \param[in] node Filter tree node
 * \param[in] rec Data record
 * \param[in] templ Data record's template
 * \return true if data record's field fits
 */
bool filter_node_fits(struct filter_treenode *node, uint8_t *rec, struct ipfix_template *templ)
{
	/**
	 * return result modified by negation flag
	 * it is the same as 'return (node->negate) ? !value : value'
	 */
	switch (node->type) {
	case NODE_AND:
		return (node->negate) ^ (filter_node_fits(node->left, rec, templ) && filter_node_fits(node->right, rec, templ));
	case NODE_OR:
		return (node->negate) ^ (filter_node_fits(node->left, rec, templ) || filter_node_fits(node->right, rec, templ));
	default:
		return (node->negate) ^ filter_value_fits(node, rec, templ);
	}
}

/**
 * \brief Apply profile filter on message and change ODID if it fits
 *
 * \param[in] msg IPFIX message
 * \param[in] profile Filter profile
 * \return 0 if no error occurs
 */
int filter_apply_profile(struct ipfix_message *msg, struct filter_profile *profile)
{
	(void) msg;
	(void) profile;
	return 0;
}

int process_message(void *config, void *message)
{
	struct ipfix_message *msg = (struct ipfix_message *) message;
	struct filter_config *conf = (struct filter_config *) config;
	struct filter_profile *aux_profile = NULL, *profile = NULL;
	struct filter_source *aux_src = NULL;
	int ret;
	uint32_t orig_odid = ntohl(msg->pkt_header->observation_domain_id);


	/* Go throught all profiles */
	for (aux_profile = conf->profiles; aux_profile && !profile; aux_profile = aux_profile->next) {
		/* Go throught all sources for this profile */
		for (aux_src = aux_profile->sources; aux_src; aux_src = aux_src->next) {
			if (aux_src->id == orig_odid) {
				profile = aux_profile;
				break;
			}
		}
	}

	if (!profile) {
		if (conf->default_profile) {
			/* Use default profile */
			profile = conf->default_profile;
		} else {
			/* No profile found for this ODID */
			pass_message(conf->ip_config, message);
			return 0;
		}
	}


	ret = filter_apply_profile(msg, profile);

	if (!ret) {
		return ret;
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
		free(aux_profile->filter);
		filter_free_profile(aux_profile);
		aux_profile = conf->profiles;
	}

	if (conf->default_profile) {
		free(conf->default_profile->filter);
		filter_free_profile(conf->default_profile);
	}

	free(conf);
	return 0;
}


/**
 * \brief Parse field name
 *
 * \param[in] field Field name
 * \return Information Element ID
 */
int filter_parse_field(const char *field)
{
	return 1;
}

/**
 * \brief Parse raw field name
 *
 * \param[in] rawfield Raw field name
 * \return Information Element ID
 */
int filter_parse_rawfield(const char *rawfield)
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
	if (!value) {
		MSG_ERROR(msg_module, "Not enought memory (%s:%d)", __FILE__, __LINE__);
		return NULL;
	}

	memcpy(value, data, length);
	return value;
}

/**
 * \brief Parse number in format [0-9]+[kKmMgGtT]
 *
 * \param[in] number Number
 * \return Numeric value
 */
uint8_t *filter_parse_number(const char *number)
{
	long tmp = strlen(number);
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

	return filter_num_to_ptr((uint8_t *) &tmp, sizeof(long));
}

/**
 * \brief Parse hexadecimal number
 */
uint8_t *filter_parse_hexnum(const char *hexnum)
{
	long tmp = strtol(hexnum, NULL, 16);
	return filter_num_to_ptr((uint8_t *) &tmp, sizeof(long));
}

enum operators filter_decode_operator(const char *op)
{
	/*
	 * there must be strNcmp because there is rest of filter string behind operator
	 * for example filter = "ie20 > 60" =>  op = "> 60"
	 */
	if (!strcmp(op, "=") || !strcmp(op, "==")) {
		return OP_EQUAL;
	} else if (!strncmp(op, "!=", 2)) {
		return OP_NOT_EQUAL;
	} else if (!strncmp(op, "<", 1)) {
		return OP_LESS;
	} else if (!strncmp(op, "<=", 2) || !strncmp(op, "=<", 2)) {
		return OP_LESS_EQUAL;
	} else if (!strncmp(op, ">", 1)) {
		return OP_GREATER;
	} else if (!strncmp(op, ">=", 2) || !strncmp(op, "=>", 2)) {
		return OP_GREATER_EQUAL;
	}

	return OP_EQUAL;
}

/**
 * \brief Create new leaf treenode
 */
struct filter_treenode *filter_new_leaf_node(int field, const char *op, uint8_t *value)
{
	struct filter_treenode *node = calloc(1, sizeof(struct filter_treenode));
	if (!node) {
		MSG_ERROR(msg_module, "Not enought memory (%s:%d)", __FILE__, __LINE__);
		return NULL;
	}

	node->value = value;
	node->field = field;
	node->type = NODE_LEAF;
	node->op = filter_decode_operator(op);

	return node;
}

/**
 * \brief Decode node type
 */
enum nodetype filter_decode_type(const char *type)
{
	if (!strncasecmp(type, "and", 3) || !strncmp(type, "&&", 2)) {
		return NODE_AND;
	}

	return NODE_OR;
}

/**
 * \brief Create new parent node
 */
struct filter_treenode *filter_new_parent_node(struct filter_treenode *left, const char *type, struct filter_treenode *right)
{
	struct filter_treenode *node = calloc(1, sizeof(struct filter_treenode));
	if (!node) {
		MSG_ERROR(msg_module, "Not enought memory (%s:%d)", __FILE__, __LINE__);
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
void filter_error(const char *msg)
{
	MSG_ERROR(msg_module, "%s", msg);
}

struct filter_treenode *filter_new_exists_node(int field)
{
	struct filter_treenode *node = calloc(1, sizeof(struct filter_treenode));

	node->field = field;
	return node;
}

