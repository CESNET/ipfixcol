/*
 * filter.h
 *
 *  Created on: 7.7.2014
 *      Author: michal
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
	VT_STRING
};

struct filter_source {
	uint32_t id;
	struct filter_source *next;
};


struct filter_config {
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
