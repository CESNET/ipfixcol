/*
 * filter.h
 *
 *  Created on: 7.7.2014
 *      Author: michal
 */

#ifndef FILTER_H_
#define FILTER_H_

#include <stdbool.h>
#include "parser.h"

//#define YY_DECL int yylex(yyscan_t scanner)

enum nodetype {
	NODE_LEAF,
	NODE_AND,
	NODE_OR
};


enum operators {
	OP_EQUAL,
	OP_LESS,
	OP_LESS_EQUAL,
	OP_GREATER,
	OP_GREATER_EQUAL,
	OP_NOT_EQUAL
};


struct filter_treenode {
	bool negate;
	bool fits;
	uint8_t *value;
	int field;
	enum nodetype type;
	enum operators op;
	struct filter_treenode *left, *right;
};

struct filter_profile {
	uint32_t new_odid;
	char *filter;
	struct filter_treenode *root;
	struct filter_source *sources;
	struct filter_profile *next;
	void *scanner;
};


/**
 * \brief Parse field name
 *
 * \param[in] field Field name
 * \return Information Element ID
 */
int filter_parse_field(const char *field);

/**
 * \brief Parse raw field name
 *
 * \param[in] rawfield Raw field name
 * \return Information Element ID
 */
int filter_parse_rawfield(const char *rawfield);

/**
 * \brief Parse number in format [0-9]+[kKmMgGtT]
 *
 * \param[in] number Number
 * \return Numeric value
 */
uint8_t *filter_parse_number(const char *number);

/**
 * \brief Parse hexadecimal number
 *
 * \param[in] hexnum Hexadecimal number
 * \return Numeric value
 */
uint8_t *filter_parse_hexnum(const char *hexnum);

/**
 * \brief Create new leaf treenode
 *
 * \param[in] field Field number
 * \param[in] operator Operator type
 * \param[in] value Pointer to value
 * \return Pointer to new leaf treenode
 */
struct filter_treenode *filter_new_leaf_node(int field, const char *op, uint8_t *value);

/**
 * \brief Decode node type
 *
 * \param[in] type Node type string
 * \return Node type value
 */
enum nodetype filter_decode_type(const char *type);

/**
 * \brief Create new parent node
 *
 * \param[in] left Left child
 * \param[in] type Node type
 * \param[in] right Right child
 * \return Pointer to new parent node
 */
struct filter_treenode *filter_new_parent_node(struct filter_treenode *left, const char *type, struct filter_treenode *right);

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
void filter_error(const char *msg);


#endif /* FILTER_H_ */
