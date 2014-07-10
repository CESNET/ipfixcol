/**
 * \file filter.h
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


#ifndef FILTER_H_
#define FILTER_H_

#include <stdbool.h>

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpathInternals.h>

#include "parser.h"

//#define YY_DECL int yylex(yyscan_t scanner)

enum nodetype {
	NODE_LEAF,
	NODE_AND,
	NODE_OR,
	NODE_EXISTS
};


enum operators {
	OP_EQUAL,
	OP_LESS,
	OP_LESS_EQUAL,
	OP_GREATER,
	OP_GREATER_EQUAL,
	OP_NOT_EQUAL,
	OP_NONE,  /* string values only */
};

enum valtype {
	VT_NUMBER,
	VT_STRING,
	VT_REGEX
};

struct filter_source {
	uint32_t id;
	struct filter_source *next;
};


struct filter_config {
	bool remove_original;
	void *ip_config;
	struct filter_profile *profiles;
	struct filter_profile *default_profile;
};

struct filter_value {
	enum valtype type;
	uint8_t *value;
};

struct filter_treenode {
	bool negate;
	bool fits;
	int field;
	enum nodetype type;
	enum operators op;
	struct filter_value *value;
	struct filter_treenode *left, *right;
};

struct filter_profile {
	uint32_t new_odid;
	struct filter_treenode *root;
	struct filter_source *sources;
	struct filter_profile *next;
};

struct filter_parser_data {
	struct filter_profile *profile;
	xmlDoc *doc;
	xmlXPathContextPtr context;
	void *scanner;
	char *filter;
};


/**
 * \brief Parse field name
 *
 * \param[in] field Field name
 * \param[in] doc Elements document
 * \param[in] context Elements XML context
 * \return Information Element ID
 */
int filter_parse_field(char *field, xmlDoc *doc, xmlXPathContextPtr context);

/**
 * \brief Parse raw field name
 *
 * \param[in] rawfield Raw field name
 * \return Information Element ID
 */
int filter_parse_rawfield(char *rawfield);

/**
 * \brief Parse number in format [0-9]+[kKmMgGtT]
 *
 * \param[in] number Number
 * \return Numeric value
 */
struct filter_value *filter_parse_number(char *number);

/**
 * \brief Parse hexadecimal number
 *
 * \param[in] hexnum Hexadecimal number
 * \return Numeric value
 */
struct filter_value *filter_parse_hexnum(char *hexnum);

/**
 * \brief Parse string value
 *
 * \param[in] string String
 * \return pointer to parsed string
 */
struct filter_value *filter_parse_string(char *string);

/**
 * \brief Parse regular expression
 *
 * \param[in] regexstr Regular expressing
 * \return pointer to parsed regex
 */
struct filter_value *filter_parse_regex(char *regexstr);

/**
 * \brief Parse IPv4 address
 *
 * \param[in] string IPv4 address
 * \return pointer to parsed value
 */
struct filter_value *filter_parse_ipv4(char *addr);

/**
 * \brief Parse IPv6 address
 *
 * \param[in] string IPv6 address
 * \return pointer to parsed value
 */
struct filter_value *filter_parse_ipv6(char *addr);

/**
 * \brief Parse timestamp
 *
 * \param[in] tstamp Timestamp
 * \return pointer to parsed value
 */
struct filter_value *filter_parse_timestamp(char *tstamp);

/**
 * \brief Create new leaf treenode
 *
 * \param[in] field Field number
 * \param[in] operator Operator type
 * \param[in] value Pointer to value
 * \return Pointer to new leaf treenode
 */
struct filter_treenode *filter_new_leaf_node(int field, char *op, struct filter_value *value);

/**
 * \brief Create new leaf treenode without specified operator
 *
 * \param[in] field Field number
 * \param[in] value Pointer to value
 * \return Pointer to new leaf treenode
 */
struct filter_treenode *filter_new_leaf_node_opless(int field, struct filter_value *value);

/**
 * \brief Decode node type
 *
 * \param[in] type Node type string
 * \return Node type value
 */
enum nodetype filter_decode_type(char *type);

/**
 * \brief Create new parent node
 *
 * \param[in] left Left child
 * \param[in] type Node type
 * \param[in] right Right child
 * \return Pointer to new parent node
 */
struct filter_treenode *filter_new_parent_node(struct filter_treenode *left, char *type, struct filter_treenode *right);

/**
 * \brief Create new EXISTS node
 *
 * \param[in] field Field ID
 * \return Pointer to new node
 */
struct filter_treenode *filter_new_exists_node(int field);

/**
 * \brief Set node negated
 *
 * \param[in] node Tree node
 */
void filter_node_set_negated(struct filter_treenode *node);

/**
 * \brief Set profile root node
 *
 * \param[in] profile Filter profile
 * \param[in] node Profile root
 */
void filter_set_root(struct filter_profile *profile, struct filter_treenode *node);

/**
 * \brief Free tree structure
 *
 * \param[in] node Tree node
 */
void filter_free_tree(struct filter_treenode *node);

/**
 * \brief Print error message from filter parser
 *
 * \param[in] msg Error message
 */
void filter_error(const char *msg, YYLTYPE *loc);


#endif /* FILTER_H_ */
