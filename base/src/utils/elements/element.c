/**
 * \file element.c
 * \author Lukas Hutak <xhutak01@stud.fit.vutbr.cz>
 * \brief Functions for parsing definitions of IPFIX elements
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

#include <stdlib.h> // qsort, bsearch
#include <stdbool.h>
#include <string.h>

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>

#include <ipfixcol.h>
#include "element.h"
#include "parser.h"


/** Default number of preallocated elements in auxiliary structures */
#define ELEM_DEF_COUNT (32)

/** Default XPath expression for elements in XML file (ipfix-elements.xml) */
#define ELEM_XML_XPATH "/ipfix-elements/element"

/* Global name of this compoment for processing IPFIX elements */
static const char *msg_module = "elements_collection";

/**
 * \brief Structure for iteration over XML elements
 */
struct elem_xml_iter {
	xmlDocPtr doc;                    /**< A XML document                 */
	xmlXPathContextPtr xpath_ctx;     /**< XPath context of the document  */
	xmlXPathObjectPtr xpath_obj;      /**< XPath objects of the context   */
	int index;                        /**< Index of next item             */
};

/**
 * \brief Comparsion function for groups of elements
 *
 * This function can be used only to compare two structures #elem_en_group by
 * their Enterprise ID.
 * \param[in] g1 Pointer on pointer of first structure
 * \param[in] g2 Pointer on pointer of second structure
 * \return An integer less than, equal to, or greater than zero if the first 
 * argument is considered to be respectively less than, equal to, or greater
 * than the second.
 */
int cmp_groups(const void *g1, const void *g2)
{
	const struct elem_en_group *group1 = *(const struct elem_en_group **) g1;
	const struct elem_en_group *group2 = *(const struct elem_en_group **) g2;
	
	if (group1->en_id == group2->en_id) {
		return 0;
	}
	
	return (group1->en_id < group2->en_id) ? (-1) : 1;
}

/**
 * \brief Comparsion function for elements with same Enterprise ID
 *
 * This function can be used only to compare two structures ipfix_element_t by
 * their Element ID.
 * \warning Elements are not compared by their Enterprise ID
 * \param[in] e1 Pointer on pointer of first structure
 * \param[in] e2 Pointer on pointer of second structure
 * \return An integer less than, equal to, or greater than zero if the first 
 * argument is considered to be respectively less than, equal to, or greater
 * than the second.
 */
int cmp_elem_by_id(const void *e1, const void *e2)
{
	const ipfix_element_t *element1 = *(const ipfix_element_t **) e1;
	const ipfix_element_t *element2 = *(const ipfix_element_t **) e2;
	
	if (element1->id == element2->id) {
		return 0;
	}
	
	return (element1->id < element2->id) ? (-1) : 1;
}

/**
 * \brief Comparsion function for elements' names (case senstive)
 *
 * This function can be used only to compare two structures ipfix_element_t by
 * their name. Function is case sensitive.
 * \param[in] e1 Pointer on pointer of first structure
 * \param[in] e2 Pointer on pointer of second structure
 * \return Return value is same as in standard library strcasecmp and strcmp
 * functions.
 */
int cmp_elem_by_name_sens(const void *e1, const void *e2)
{
	const ipfix_element_t *element1 = *(const ipfix_element_t **) e1;
	const ipfix_element_t *element2 = *(const ipfix_element_t **) e2;
	
	/*
	 * First case insensitive comparation
	 * We want to be able to compare elements also case insensitive, so elements
	 * that differ by letter case must be closer.
	 */
	int case_cmp = strcasecmp(element1->name, element2->name);
	if (case_cmp != 0) {
		return case_cmp;
	}
	
	// Probably same names -> case sensitive comparation
	return strcmp(element1->name, element2->name);
}

/**
 * \brief Comparsion function for elements' names (case insenstive)
 *
 * This function can be used only to compare two structures ipfix_element_t by
 * their name. Function is case insensitive.
 * \param[in] e1 Pointer on pointer of first structure
 * \param[in] e2 Pointer on pointer of second structure
 * \return Return value is same as in standard library strcasecmp function.
 */
int cmp_elem_by_name_ins(const void *e1, const void *e2)
{
	const ipfix_element_t *element1 = *(const ipfix_element_t **) e1;
	const ipfix_element_t *element2 = *(const ipfix_element_t **) e2;
	
	return strcasecmp(element1->name, element2->name);
}


/**
 * \brief Add new IPFIX element to a proper group
 *
 * Find a group of elements with same Enteprise ID and insert new element inside
 * it. If Enterprise group does not exists, it will create new one.
 * \warning Dupliticity of elements is not check. Groups must be sorted before
 * next usage for searching for elements.
 * \param[in,out] groups Structure with groups of IPFIX elements.
 * \param[in] elem New IPFIX element
 * \return O on success. Otherwise returns non-zero value.
 */
static int elem_add_element(struct elem_groups* groups, ipfix_element_t *elem)
{
	struct elem_en_group *key_ptr, *en_group = NULL;
	struct elem_en_group **aux_ptr;
	
	struct elem_en_group key;
	key.en_id = elem->en;
	key_ptr = &key;
	
	// Find the group with same Enterprise ID
	aux_ptr = bsearch(&key_ptr, groups->groups, groups->elem_used,
		sizeof(struct elem_en_group *), cmp_groups);
	if (aux_ptr) {
		en_group = *aux_ptr;
	}
	
	if (!en_group) {
		// Group not found -> create new one
		if (groups->elem_used == groups->elem_max) {
			// Array is full -> realloc
			struct elem_en_group **new_ptr;
			new_ptr = (struct elem_en_group **) realloc(groups->groups,
				2 * groups->elem_max * sizeof(struct elem_en_group *));
			if (!new_ptr) {
				MSG_ERROR(msg_module, "REALLOC FAILED! (%s:%d)", __FILE__,
					__LINE__);
				return 1;
			}
			
			groups->elem_max *= 2;
			groups->groups = new_ptr;
		}
		
		en_group = (struct elem_en_group *) calloc(1, sizeof(struct elem_en_group));
		if (!en_group) {
			MSG_ERROR(msg_module, "CALLOC FAILED! (%s:%d)", __FILE__, __LINE__);
			return 1;
		}
		
		en_group->en_id = elem->en;
		groups->groups[groups->elem_used++] = en_group;
		
		// We use binary search for searching for groups -> sort
		qsort(groups->groups, groups->elem_used, sizeof(struct elem_en_group *),
			cmp_groups);
	}

	// Add element to the group
	if (en_group->elem_used == en_group->elem_max) {
		// Array is empty or full -> realloc
		size_t new_size = (en_group->elem_max == 0)
			? ELEM_DEF_COUNT : (2 * en_group->elem_max);
		
		ipfix_element_t **new_ptr;
		new_ptr = (ipfix_element_t **) realloc(en_group->elements,
			new_size * sizeof(ipfix_element_t *));
		if (!new_ptr) {
			MSG_ERROR(msg_module, "CALLOC FAILED! (%s:%d)", __FILE__, __LINE__);
			return 1;
		}
		
		en_group->elements = new_ptr;
		en_group->elem_max = new_size;
	}
	
	// Success
	en_group->elements[en_group->elem_used++] = elem;
	return 0;
}

/**
 * \brief Sort all elements in internal structures
 *
 * For each group of elements with same Enterprise ID, the elements are sorted
 * by their Element ID.
 * \param[in,out] groups Structure with groups of IPFIX elements
 */
static void elem_sort(struct elem_groups *groups)
{
	for (unsigned int i = 0; i < groups->elem_used; ++i) {
		struct elem_en_group *aux_grp = groups->groups[i];
		
		qsort(aux_grp->elements, aux_grp->elem_used, sizeof(ipfix_element_t *),
			cmp_elem_by_id);
	}
}

/**
 * \brief Duplication check
 *
 * If there are multiple definitions of elements with same Element ID and 
 * Enterprise ID, duplication will be found. When name indexes are generated
 * for each group of elements with same Enterprise ID, this function will also
 * check if there are elements with similar names in each group.
 * \warning Groups of elements must be sorted by elem_sort() first.
 * \param[in] groups Structure with groups of IPFIX elements.
 * \return If the duplication is found, it will return true. Otherwise returns
 * false.
 */
static bool elem_duplication_check(const struct elem_groups *groups)
{
	bool duplicity = false;

	// Check duplicity of IDs -> error
	for (unsigned int i = 0; i < groups->elem_used; ++i) {
		// For each group
		struct elem_en_group *aux_grp = groups->groups[i];
		for (unsigned int j = 0; j < aux_grp->elem_used - 1; ++j) {
			// Check duplicity
			if (aux_grp->elements[j]->id != aux_grp->elements[j + 1]->id) {
				continue;
			}
			
			const ipfix_element_t *elem = aux_grp->elements[j];
			MSG_ERROR(msg_module, "Multiple definions of same IPFIX element "
				"(EN: %u, ID: %u)", elem->en, elem->id);
			duplicity = true;
		}
	}
	
	// Check duplicity of names -> just warning
	for (unsigned int i = 0; i < groups->elem_used; ++i) {
		// For each group
		struct elem_en_group *aux_grp = groups->groups[i];
		if (!aux_grp || !aux_grp->name_index) {
			// Name index not filled
			continue;
		}
		
		for (unsigned int j = 0; j < aux_grp->elem_used - 1; ++j) {
			// Check duplicity
			if (cmp_elem_by_name_ins(&aux_grp->name_index[j],
				&aux_grp->name_index[j + 1]) != 0) {
				continue;
			}
			
			MSG_WARNING(msg_module, "Multiple definions of IPFIX elements "
				"with similar name '%s' in the group of elements with "
				"Enterprise ID %u.", aux_grp->name_index[j]->name,
				aux_grp->en_id);
		}
	}
	
	return duplicity;
}

/**
 * \brief Make indexes of elements' names
 * 
 * Each index allows to search for an element by name in an enterprise group
 * or global.
 * \param[in,out] groups Structure with groups of IPFIX elements.
 * \return 0 on success. Otherwiser returns non-zero value.
 */
static int elem_make_name_indexes(struct elem_groups *groups)
{
	// Get a count of all elements
	unsigned int count = 0;
	for (unsigned int i = 0; i < groups->elem_used; ++i) {
		count += groups->groups[i]->elem_used;
	}
	
	if (count == 0) {
		return 0;
	}

	// Make an index of all elements by name
	groups->name_index = calloc(count, sizeof(ipfix_element_t *));
	if (!groups->name_index) {
		MSG_ERROR(msg_module, "CALLOC FAILED! (%s:%d)", __FILE__, __LINE__);
		return 1;
	}
	groups->name_count = count;
	
	for (unsigned int index = 0, i = 0; i < groups->elem_used; ++i) {
		struct elem_en_group *aux_grp = groups->groups[i];
		
		// Create also index of names for each Enterprise group
		aux_grp->name_index = calloc(aux_grp->elem_used,
			sizeof(ipfix_element_t *));
		if (!aux_grp->name_index) {
			MSG_ERROR(msg_module, "CALLOC FAILED! (%s:%d)", __FILE__, __LINE__);
			return 1;
		}
		
		for (unsigned int y = 0; y < aux_grp->elem_used; ++y) {
			groups->name_index[index++] = aux_grp->elements[y];
			aux_grp->name_index[y] = aux_grp->elements[y];
		}
		
		// Sort name index of Enterprise group
		qsort(aux_grp->name_index, aux_grp->elem_used,
			sizeof(ipfix_element_t *), cmp_elem_by_name_sens);
	}
	
	// Sort name index of all elements
	qsort(groups->name_index, groups->name_count, sizeof(ipfix_element_t *),
		cmp_elem_by_name_sens);

	return 0;
}


/**
 * \brief Initialize the iterator of IPFIX elements over a XML document
 *
 * After success intilialization call #elem_iter_next to get first element.
 * \param[in] fd An opened file descriptor of XML document
 * \return On success returns pointer to the initialized iterator. Otherwise
 * returns NULL.
 */
static struct elem_xml_iter *elem_iter_init(int fd)
{
	struct elem_xml_iter *iter;
	iter = (struct elem_xml_iter *) calloc(1, sizeof(struct elem_xml_iter));
	if (!iter) {
		MSG_ERROR(msg_module, "CALLOC FAILED! (%s:%d)", __FILE__, __LINE__);
		return NULL;
	}

	// Load XML document	
	iter->doc = xmlReadFd(fd, NULL, NULL, 
		XML_PARSE_NOERROR | XML_PARSE_NOWARNING | XML_PARSE_NOBLANKS | 
		XML_PARSE_BIG_LINES);
	if (!iter->doc) {
		MSG_ERROR(msg_module, "Unable to parse XML document with IPFIX "
			" elements.");
		free(iter);
		return NULL;
	}
	
	// Get XPath context and evaluate expression
	iter->xpath_ctx = xmlXPathNewContext(iter->doc);
	if (!iter->xpath_ctx) {
		MSG_ERROR(msg_module, "Unable to get XPath context of XML document "
			"with IPFIX elements.");
		xmlFreeDoc(iter->doc);
		free(iter);
		return NULL;
	}
	
	iter->xpath_obj = xmlXPathEvalExpression(BAD_CAST ELEM_XML_XPATH,
		iter->xpath_ctx);
	if (!iter->xpath_obj) {
		MSG_ERROR(msg_module, "Unable to evaluate XPath expression.");
		xmlXPathFreeContext(iter->xpath_ctx);
		xmlFreeDoc(iter->doc);
		free(iter);
		return NULL;
	}
	
	if (xmlXPathNodeSetIsEmpty(iter->xpath_obj->nodesetval)) {
		MSG_ERROR(msg_module, "No IPFIX elements in XML document.");
		xmlXPathFreeObject(iter->xpath_obj);
		xmlXPathFreeContext(iter->xpath_ctx);
		xmlFreeDoc(iter->doc);
		free(iter);
		return NULL;
	}
	
	return iter;
}

/**
 * \brief Get next XML element with IPFIX element from the iterator
 *
 * \param[in,out] iter Iterator
 * \return On success returns a pointer to next element. Otherwise returns NULL.
 */
static xmlNodePtr elem_iter_next(struct elem_xml_iter *iter)
{
	if (iter->index >= iter->xpath_obj->nodesetval->nodeNr) {
		return NULL;
	}
	
	return iter->xpath_obj->nodesetval->nodeTab[iter->index++];
}

/**
 * \brief Destroy the iterator of IPFIX elements
 *
 * \param[in,out] iter Iterator
 */
static void elem_iter_destroy(struct elem_xml_iter *iter)
{
	xmlXPathFreeObject(iter->xpath_obj);
	xmlXPathFreeContext(iter->xpath_ctx);
	xmlFreeDoc(iter->doc);
	
	free(iter);
}


/**
 * \brief Init internal stuctures for elements
 * \return On success returns pointer to initialized structures. Otherwise
 * returns NULL.
 */
struct elem_groups *elements_init()
{
	struct elem_groups *groups;
	
	// Initialize structures
	groups = (struct elem_groups *) calloc(1, sizeof(struct elem_groups));
	if (!groups) {
		MSG_ERROR(msg_module, "CALLOC FAILED! (%s:%d)", __FILE__, __LINE__);
		return NULL;
	}
	
	groups->groups = (struct elem_en_group **) calloc(ELEM_DEF_COUNT,
		sizeof(struct elem_en_group *));
	if (!groups->groups) {
		MSG_ERROR(msg_module, "CALLOC FAILED! (%s:%d)", __FILE__, __LINE__);
		free(groups);
		return NULL;
	}
	
	groups->elem_max = ELEM_DEF_COUNT;
	return groups;
}


/**
 * \brief Load IPFIX elements
 *
 * \warning When function fails to load all elements, content of ipfix_group 
 * will not be defined and should be destroyed using elements_destroy().
 * \param[in] file_descriptor Opened file with XML specification of IPFIX elems.
 * \param[out] ipfix_groups Structure with elements description
 * \return 0 on success. Otherwise returns non-zero value.
 */
int elements_load(int file_descriptor, struct elem_groups *ipfix_groups)
{
	// Create an iterator over IPFIX elements in XML document
	struct elem_xml_iter *iter = elem_iter_init(file_descriptor);
	if (!iter) {
		// Failed to init iterator
		return 1;
	}
	
	unsigned int count = 0;
	bool failed = false;
	xmlNodePtr node;
	
	// Iterate over all elements and fill structures
	while((node = elem_iter_next(iter)) != NULL) {
		ipfix_element_t *new_item = parse_element(node);
		if (!new_item) {
			// Failed to create new element description
			failed = true;
			break;
		}
		
		if (elem_add_element(ipfix_groups, new_item) != 0) {
			failed = true;
			break;
		}
		
		++count;
	}
	
	elem_iter_destroy(iter);
	if (failed) {
		return 1;
	}


	elem_sort(ipfix_groups);
	if (elem_make_name_indexes(ipfix_groups)) {
		return 1;
	}
	
	if (elem_duplication_check(ipfix_groups)) {
		// Duplication found
		return 1;
	}
	
	// All elements successfully loaded
	MSG_INFO(msg_module, "Description of %u IPFIX elements loaded.", count);
	return 0;
}

/**
 * \brief Delete loaded data and structures
 * \param[in] ipfix_groups Main IPFIX group structure
 */ 
void elements_destroy(struct elem_groups *ipfix_groups)
{
	if (ipfix_groups == NULL) {
		return;
	}

	for (unsigned int i = 0; i < ipfix_groups->elem_used; ++i) {
		// Delete group
		struct elem_en_group *aux_grp = ipfix_groups->groups[i];
		for (unsigned int y = 0; y < aux_grp->elem_used; ++y)  {
			// Delete IPFIX element
			ipfix_element_t *aux_elem = aux_grp->elements[y];
			free(aux_elem->name);
			free(aux_elem);
		}
		
		free(aux_grp->elements);
		free(aux_grp->name_index);
		free(aux_grp);
	}
	
	free(ipfix_groups->groups);
	free(ipfix_groups->name_index);
	free(ipfix_groups);
}

