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

#include <ipfixcol.h>
#include <stdbool.h>
#include <stdint.h>

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpathInternals.h>

#include "parser.h"

//#define YY_DECL int yylex(yyscan_t scanner)

/**
 * Type of tree nodes
 */
enum nodetype {
    NODE_LEAF,  /**< leaf node */
    NODE_AND,   /**< subtree && subtree */
    NODE_OR,    /**< subtree || subtree */
    NODE_EXISTS /**< leaf node for testing presence of some field in record */
};

/**
 * Comparison operators
 */
enum operators {
    OP_EQUAL,           /**< == */
    OP_LESS,            /**< <  */
    OP_LESS_EQUAL,      /**< <= */
    OP_GREATER,         /**< >  */
    OP_GREATER_EQUAL,   /**< >= */
    OP_NOT_EQUAL,       /**< != */
    OP_NONE,            /**< string values only */
};

/**
 * Value types
 */
enum valtype {
    VT_NUMBER,  /**< numeric value */
    VT_STRING,  /**< string value  */
    VT_REGEX,   /**< regular expression */
    VT_PREFIX	/**< IP prefix */
};

/**
 * \brief IP prefix value
 */
struct filter_prefix {
	uint16_t fullBytes;	/**< Number of full bytes */
	uint16_t bits;		/**< Number of remaining bits after full bytes */
	uint8_t data[16];	/**< Prefix address */
};

/**
 * \brief Tree node value structure
 */
struct filter_value {
    enum valtype type;  /**< value type */
    uint8_t *value;     /**< data */
    int length;         /**< data length */
};

/**
 * Field types
 */
enum field_type {
    FT_DATA,    /**< Data field from ipfix-elements */
    FT_HEADER   /**< Packet header field */
};

/**
 * Header fields
 */
enum header_field {
    HF_ODID,    /**< Observation domain ID */
    HF_SRCIP,   /**< Source IP address */
    HF_SRCPORT, /**< Source port */
    HF_DSTIP,   /**< Destination IP address */
    HF_DSTPORT, /**< Destination port number */
};

/**
 * \brief Field identifier
 */
struct filter_field {
    enum field_type type;   /**< Field type */
    uint32_t enterprise;    /**< enterprise number */
    uint16_t id;            /**< element ID */
};

/**
 * \brief Tree node structure
 * 
 * Each treenode keeps some part of filter expression
 * Leaf nodes: field op value
 * Exists nodes: EXISTS field
 * And nodes: left && right
 * Or nodes:  left || right
 */
struct filter_treenode {
    bool negate;        /**< negation flag */
    enum nodetype type; /**< type of node (leaf | exists | or | and) */
    enum operators op;  /**< comparison operator */
    struct filter_field *field; /**< IPFIX field identifier */
    struct filter_value *value; /**< value compared with the same field in data records */
    struct filter_treenode *left, *right; /**< subtrees */
};

/**
 * \brief Profile structure
 * 
 * Each filter string is representing one filter profile
 */
struct filter_profile {
    uint16_t id;                    /**< profile ID */
    struct filter_treenode *root;   /**< filter tree */
};

/**
 * \brief Data for parsing filter
 * 
 * This structure is given into BISON parser and may be passed into parsing functions
 */
struct filter_parser_data {
    struct filter_profile *profile; /**< actual profile */
    xmlDoc *doc;                    /**< ipfix-elements.xml */
    xmlXPathContextPtr context;     /**< xml context */
    void *scanner;                  /**< FLEX scanner */
    char *filter;                   /**< parsed filter */
};

/**
 * \brief Open ipfix-elements.xml file
 * 
 * \param[in] pdata Parser's data with document's context
 * \return 0 on success
 */
int filter_init_elements(struct filter_parser_data *pdata);

/**
 * \brief Parse field name
 *
 * Searches field name in ipfix-elements.xml and converts it into field ID
 * 
 * \param[in] name Field name
 * \param[in] doc Elements document
 * \param[in] context Elements XML context
 * \return filter field structure
 */
struct filter_field *filter_parse_field(char *name, xmlDoc *doc, xmlXPathContextPtr context);

/**
 * \brief Parse raw field name
 * 
 * Converts raw field name (eXidYY) into ID
 *
 * \param[in] rawfield Raw field name
 * \return filter field structure
 */
struct filter_field *filter_parse_rawfield(char *rawfield);

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
 * \param[in] addr IPv4 address
 * \return pointer to parsed value
 */
struct filter_value *filter_parse_ipv4(char *addr);

/**
 * \brief Parse IPv6 address
 *
 * \param[in] addr IPv6 address
 * \return pointer to parsed value
 */
struct filter_value *filter_parse_ipv6(char *addr);

/**
 * \brief Parse IPv4 prefix
 * 
 * \param[in] addr IPv4 prefix
 * \return pointer to parsed value
 */
struct filter_value *filter_parse_prefix4(char *addr);

/**
 * \brief Parse IPv6 prefix
 * 
 * \param[in] addr IPv6 prefix
 * \return pointer to parsed value
 */
struct filter_value *filter_parse_prefix6(char *addr);

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
 * \param[in] field Field ID
 * \param[in] op Operator type
 * \param[in] value Pointer to value
 * \return Pointer to new leaf treenode
 */
struct filter_treenode *filter_new_leaf_node(struct filter_field *field, char *op, struct filter_value *value);

/**
 * \brief Create new leaf treenode without specified operator
 *
 * \param[in] field Field ID
 * \param[in] value Pointer to value
 * \return Pointer to new leaf treenode
 */
struct filter_treenode *filter_new_leaf_node_opless(struct filter_field *field, struct filter_value *value);

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
struct filter_treenode *filter_new_exists_node(struct filter_field *field);

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
 * \param[in] loc Error location
 */
void filter_error(const char *msg, YYLTYPE *loc);

/**
 * \brief Match filter with IPFIX record
 * 
 * \param[in] node filter node
 * \param[in] msg IPFIX message (filter may contain field from message header)
 * \param[in] data IPFIX data record
 * \return true when node fits
 */
bool filter_fits_node(struct filter_treenode* node, struct ipfix_message *msg, struct ipfix_record *data);

/**
 * \brief Free profile's data
 * 
 * \param[in] profile Profile
 */
void filter_free_profile(struct filter_profile *profile);


#endif /* FILTER_H_ */
