/**
 * \file parser.c
 * \author Lukas Hutak <xhutak01@stud.fit.vutbr.cz>
 * \brief Functions for parsing XML definitions of IPFIX elements
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

#include <ctype.h>
#include <stdint.h>
#include <string.h>

#include <ipfixcol.h>
#include "parser.h"

#define XML_ELEM_ENTERPRISE "enterprise"
#define XML_ELEM_ID         "id"
#define XML_ELEM_NAME       "name"
#define XML_ELEM_DATATYPE   "dataType"
#define XML_ELEM_SEMANTIC   "semantic"

/* Global name of this compoment for processing IPFIX elements */
static const char *msg_module = "elements_parser";

// TODO: move to utils
/**
 * \brief Trim a string
 *
 * Removes all leading and tailing whitespaces of the string and result stores
 * into output buffer. Size of the buffer should be at least same size (+ 1)
 * as lenght of the string for trimming.
 * \param[in] src String for trimming
 * \param[out] buffer Output buffer for trimmed string
 * \param[in] buffer_size Size of output buffer
 * \return On success returns a pointer to the buffer and the buffer is filled
 * with trimmed string. When trimmed string cannot be stored into the buffer
 * (buffer is too small), returns NULL.
 */
char *trim_string(const char *src, char *buffer, size_t buffer_size)
{
	const char *pos_start = src;
	const char *pos_end = NULL;
	
	// Is there any buffer?
	if (!buffer || !buffer_size) {
		return NULL;
	}
	
	// Find first non-white character
	while(*pos_start != '\0' && isspace(*pos_start)) {
		++pos_start;
	}
	
	// Empty string?
	if (*pos_start == '\0') {
		*buffer = '\0';
		return buffer;
	}
	
	// Find last non-white character
	pos_end = src + strlen(src) - 1;
	while(pos_end > pos_start && isspace(*pos_end)) {
		--pos_end;
	}
	
	// Can we store the substring to the buffer?
	size_t copy_size = pos_end + 1 - pos_start;
	if (copy_size > buffer_size - 1) { // Trailing '\0'
		// The buffer is small
		return NULL;
	}
	
	memcpy(buffer, pos_start, copy_size);
	buffer[copy_size] = '\0';
	return buffer;
}



/**
 * \brief Get the text content of the node's chidren node with given name.
 *
 * The given node is searched for children specified by name. Children MUST BE
 * text node. This function returns only the first match and it is case 
 * sensitive.
 * \warning If the children node is empty (text is missing), returns NULL. You
 * can use xml_children_is_empty() to check it.
 * \param[in] node Parent node
 * \param[in] name Name of children node
 * \return On success return pointer to the context. Otherwise returns NULL.
 */
const xmlChar *xml_get_text_content(const xmlNodePtr node, const xmlChar *name)
{
	if (!node || !name) {
		return NULL;
	}
	
	// Find the child
	for (xmlNodePtr cur = node->children; cur; cur = cur->next) {
		if (xmlStrcmp(cur->name, name))  {
			continue;
		}
		
		if (!cur->children || cur->children->type != XML_TEXT_NODE) {		
			continue;
		}
		
		// Success
		return cur->children->content;
	}
	
	// Not found
	return NULL;
}

/**
 * \brief Check if node's chidren node with given name has no children.
 * 
 * The given node is searched for children specified by name. This function
 * returns only the first match and it is case sensitive.
 * \param[in] node Parent node
 * \param[in] name Name of children node
 * \return If the node with given name was not found returns -1. If the node
 * was found and empty returns 0. If the node was found and not empty returns 1.
 */
int xml_children_is_empty(const xmlNodePtr node, const xmlChar *name)
{
	if (!node || !name) {
		return -1;
	}
	
	// Find the child
	for (xmlNodePtr cur = node->children; cur; cur = cur->next) {
		if (xmlStrcmp(cur->name, name)) {
			continue;
		}
		
		// Element found
		return cur->children ? 1 : 0;
	}
	
	// Child not found
	return -1;
}

/**
 * \brief Get the text content of the node's children node with given name and
 * convert it to 64bit unsigned integer.
 *
 * The given node is searched for children specified by name. Children MUST BE
 * text node. Value is converted to 64bit unsigned integer (uint64_t).
 * \param[in] node Parent node
 * \param[in] name Name of children node
 * \param[out] res Result of conversion.
 * \return 0 on success. Otherwise returns non-zero value.
 */
int xml_get_unsigned(const xmlNodePtr node, const xmlChar *name, uint64_t *res)
{
	const size_t buffer_size = 32;
	char buffer[buffer_size];
	
	// Get a text value
	const xmlChar *aux_str = xml_get_text_content(node, name);
	if (!aux_str) {
		MSG_ERROR(msg_module, "Cannot find '%s' in IPFIX element (line: %ld)",
			(const char *) name, xmlGetLineNo(node));
		return 1;
	}
	
	// Trim
	if (!trim_string((const char *) aux_str, buffer, buffer_size)) {
		MSG_ERROR(msg_module, "Cannot parse '%s' value in IPFIX element "
			"(line: %ld)", (const char *) name, xmlGetLineNo(node));
		return 1;
	}
	
	uint64_t result;
	char *end_ptr = NULL;
	
	// Convert
	result = strtoul(buffer, &end_ptr, 10);
	if (end_ptr == NULL || *end_ptr != '\0') {
		// Conversion failed
		MSG_ERROR(msg_module, "'%s' has not valid value in IPFIX element "
			"(line %ld)", (const char *) name, xmlGetLineNo(node));
		return 1;
	}
	
	*res = result;
	return 0;
}

/**
 * \brief Get the type of IPFIX element from XML node with IPFIX element 
 * description
 * 
 * Find 'dataType' element and parse it's value. If the value is not defined or
 * the type is unknown, returns non-zero value. Conversion of element is case
 * insensitive.
 * \param[in] node Parent node
 * \param[out] res Result
 * \return 0 on success. Otherwise returns non-zero value.
 */
int xml_elem_get_type(const xmlNodePtr node, enum ELEMENT_TYPE *res)
{
	const size_t buffer_size = 64;
	char buffer[buffer_size];

	// Get a text value
	const xmlChar *aux_str;
	aux_str = xml_get_text_content(node, BAD_CAST XML_ELEM_DATATYPE);
	if (!aux_str) {
		MSG_ERROR(msg_module, "Cannot find '%s' in IPFIX element (line: %ld)",
			XML_ELEM_DATATYPE, xmlGetLineNo(node));
		return 1;
	}
	
	// Trim
	if (!trim_string((const char *) aux_str, buffer, buffer_size)) {
		MSG_ERROR(msg_module, "Cannot parse '%s' value in IPFIX element "
			"(line: %ld)", XML_ELEM_DATATYPE, xmlGetLineNo(node));
		return 1;
	}
	
	enum ELEMENT_TYPE result = ET_UNASSIGNED;
	#define TYPE_RESULT(value) { \
		result = value; \
		goto xml_elem_get_type_end; \
	}
	
	// Try to match with known types
	if (!strcasecmp(buffer, "octetarray"))
		TYPE_RESULT(ET_OCTET_ARRAY);
	if (!strcasecmp(buffer, "unsigned8"))
		TYPE_RESULT(ET_UNSIGNED_8);
	if (!strcasecmp(buffer, "unsigned16"))
		TYPE_RESULT(ET_UNSIGNED_16);
	if (!strcasecmp(buffer, "unsigned32"))
		TYPE_RESULT(ET_UNSIGNED_32);
	if (!strcasecmp(buffer, "unsigned64"))
		TYPE_RESULT(ET_UNSIGNED_64);
	if (!strcasecmp(buffer, "signed8"))
		TYPE_RESULT(ET_SIGNED_8);
	if (!strcasecmp(buffer, "signed16"))
		TYPE_RESULT(ET_SIGNED_16);
	if (!strcasecmp(buffer, "signed32"))
		TYPE_RESULT(ET_SIGNED_32);
	if (!strcasecmp(buffer, "signed64"))
		TYPE_RESULT(ET_SIGNED_64);
	if (!strcasecmp(buffer, "float32"))
		TYPE_RESULT(ET_FLOAT_32);
	if (!strcasecmp(buffer, "float64"))
		TYPE_RESULT(ET_FLOAT_64);
	if (!strcasecmp(buffer, "boolean"))
		TYPE_RESULT(ET_BOOLEAN);
	if (!strcasecmp(buffer, "macaddress"))
		TYPE_RESULT(ET_MAC_ADDRESS);
	if (!strcasecmp(buffer, "string"))
		TYPE_RESULT(ET_STRING);
	if (!strcasecmp(buffer, "datetimeseconds")) 
		TYPE_RESULT(ET_DATE_TIME_SECONDS);
	if (!strcasecmp(buffer, "datetimemilliseconds"))
		TYPE_RESULT(ET_DATE_TIME_MILLISECONDS);
	if (!strcasecmp(buffer, "datetimemicroseconds"))
		TYPE_RESULT(ET_DATE_TIME_MICROSECONDS);
	if (!strcasecmp(buffer, "datetimenanoseconds"))
		TYPE_RESULT(ET_DATE_TIME_NANOSECONDS);
	if (!strcasecmp(buffer, "ipv4address"))
		TYPE_RESULT(ET_IPV4_ADDRESS);
	if (!strcasecmp(buffer, "ipv6address"))
		TYPE_RESULT(ET_IPV6_ADDRESS);
	if (!strcasecmp(buffer, "basiclist"))
		TYPE_RESULT(ET_BASIC_LIST);
	if (!strcasecmp(buffer, "subtemplatelist")) 
		TYPE_RESULT(ET_SUB_TEMPLATE_LIST);
	if (!strcasecmp(buffer, "subtemplatemultilist"))
		TYPE_RESULT(ET_SUB_TEMPLATE_MULTILIST);
	#undef TYPE_RESULT
	
	// Not found
	MSG_ERROR(msg_module, "Element '%s' of IPFIX element (line: %ld) has "
		"unknown type '%s'.", XML_ELEM_DATATYPE, xmlGetLineNo(node), buffer);
	return 1;
	
xml_elem_get_type_end:
	// Success
	*res = result;
	return 0;
}

/**
 * \brief Get the semantic of IPFIX element from XML node with IPFIX element
 * description
 * 
 * Find 'semantic' element and parse it's value. If the value is not defined or
 * the type is unknown, will return non-zero value. Conversion of the element is
 * case insensitive.
 * \param[in] node Parent node
 * \param[out] res Result
 * \return 0 on success. Otherwise returns non-zero value.
 */
int xml_elem_get_semantic(const xmlNodePtr node, enum ELEMENT_SEMANTIC *res)
{
	const size_t buffer_size = 64;
	char buffer[buffer_size];

	// Get a text value
	const xmlChar *aux_str;
	aux_str = xml_get_text_content(node, BAD_CAST XML_ELEM_SEMANTIC);
	if (!aux_str) {
		// The element not found or it is empty (text node is missing)
		if (xml_children_is_empty(node, BAD_CAST XML_ELEM_SEMANTIC) == 0) {
			// The element exists but text node is empty
			*res = ES_DEFAULT;
			return 0;
		}
	
		MSG_ERROR(msg_module, "Cannot find '%s' in IPFIX element (line: %ld)",
			XML_ELEM_SEMANTIC, xmlGetLineNo(node));
		return 1;
	}
	
	// Trim
	if (!trim_string((const char *) aux_str, buffer, buffer_size)) {
		MSG_ERROR(msg_module, "Cannot parse '%s' value in IPFIX element "
		 "(line: %ld)", XML_ELEM_SEMANTIC, xmlGetLineNo(node));
		return 1;
	}
	
	enum ELEMENT_SEMANTIC result = ES_UNASSIGNED;
	#define SEMANTIC_RESULT(value) { \
		result = value; \
		goto xml_elem_get_semantic_end; \
	}

	if (buffer[0] == '\0') {
		// Not defined -> default
		SEMANTIC_RESULT(ES_DEFAULT);
	}

	// Try to match with known types - case insensitive
	if (!strcasecmp(buffer, "quantity"))
		SEMANTIC_RESULT(ES_QUANTITY);
	if (!strcasecmp(buffer, "totalcounter"))
		SEMANTIC_RESULT(ES_TOTAL_COUNTER);
	if (!strcasecmp(buffer, "deltacounter"))
		SEMANTIC_RESULT(ES_DELTA_COUNTER);
	if (!strcasecmp(buffer, "identifier"))
		SEMANTIC_RESULT(ES_IDENTIFIER);
	if (!strcasecmp(buffer, "flags"))
		SEMANTIC_RESULT(ES_FLAGS);
	if (!strcasecmp(buffer, "list"))
		SEMANTIC_RESULT(ES_LIST);
	#undef SEMANTIC_RESULT
	
	// Not found
	MSG_ERROR(msg_module, "Element '%s' of IPFIX element (line: %ld) has "
		"unknown type '%s'.", XML_ELEM_SEMANTIC, xmlGetLineNo(node), buffer);
	return 1;

xml_elem_get_semantic_end:
	// Success
	*res = result;
	return 0;
}


/**
 * \brief Parse an IPFIX element
 * 
 * Convert the IPFIX element XML specification to #ipfix_element_t structure.
 * \param[in] Pointer to the XML node of IPFIX element
 * \return On success returns pointer to new element. Otherwise returns NULL.
 */
ipfix_element_t *parse_element(const xmlNodePtr node)
{
	ipfix_element_t *element;
	uint64_t aux_uint;

	element = (ipfix_element_t *) calloc(1, sizeof(ipfix_element_t));
	if (!element) {
		MSG_ERROR(msg_module, "CALLOC FAILED! (%s:%d)", __FILE__, __LINE__);
		return NULL;
	}
	
	// Parse Element ID
	if (xml_get_unsigned(node, BAD_CAST XML_ELEM_ID, &aux_uint)) {
		free(element);
		return NULL;
	}
	
	// 16th bit is flag of usage enterprise el. -> range of value is only 15bit
	if (aux_uint > (UINT16_MAX / 2)) {
		MSG_ERROR(msg_module, "Element '%s' of IPFIX element (line: %ld) is "
			"out of range.", XML_ELEM_ID, xmlGetLineNo(node));
		free(element);
		return NULL;
	}
	element->id = aux_uint;
	
	// Parse Enterprise ID
	if (xml_get_unsigned(node, BAD_CAST XML_ELEM_ENTERPRISE, &aux_uint)) {
		free(element);
		return NULL;
	}
	
	if (aux_uint > UINT32_MAX) {
		MSG_ERROR(msg_module, "Element '%s' of IPFIX element (line: %ld) is "
			"out of range.", XML_ELEM_ENTERPRISE, xmlGetLineNo(node));
		free(element);
		return NULL;
	}
	element->en = aux_uint;
	
	// Parse element's type
	if (xml_elem_get_type(node, &element->type)) {
		free(element);
		return NULL;
	}
	
	// Parse element's semantic
	if (xml_elem_get_semantic(node, &element->semantic)) {
		free(element);
		return NULL;
	}
	
	// Copy name
	const xmlChar *xml_name = xml_get_text_content(node,
		BAD_CAST XML_ELEM_NAME);
	if (!xml_name) {
		MSG_ERROR(msg_module, "Element '%s' of IPFIX element (line: %ld) is "
			"missing or is empty.", XML_ELEM_NAME, xmlGetLineNo(node));
		free(element);
		return NULL;
	}
	
	element->name = strdup((const char *) xml_name);
	if (!element->name) {
		MSG_ERROR(msg_module, "STRDUP FAILED! (%s:%d)", __FILE__, __LINE__);
		free(element);
		return NULL;
	}
	
	// Success
	return element;
}

