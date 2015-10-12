/**
 * \file ipfix_element.c
 * \author Lukas Hutak <xhutak01@stud.fit.vutbr.cz>
 * \brief Public functions for searching for definitions of IPFIX elements
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

#include <stdlib.h> // bsearch
#include <string.h> // strchr

#include <ipfixcol.h>
#include "element.h"
#include "collection.h"


/**
 * \brief Get a group of elements with same Enterprise ID
 *
 * Try to find the group with given Enterprise ID. If the group does not exists,
 * returns NULL.
 * \param[in] en Enterprise ID
 * \return Pointer to the group or NULL.
 */
static const struct elem_en_group *get_en_group_by_id(uint32_t en)
{
	// Get a pointer to the main structure
	const struct elem_groups *groups = elem_coll_get();
	if (!groups) {
		// Not initialized
		return NULL;
	}
	
	// Find the group with same Enterprise ID
	struct elem_en_group grp_key;
	struct elem_en_group *grp_key_p;
	struct elem_en_group **grp_pp;
	
	grp_key.en_id = en;
	grp_key_p = &grp_key;
	
	grp_pp = bsearch(&grp_key_p, groups->groups, groups->elem_used,
		sizeof(struct elem_en_group *), cmp_groups);
	if (!grp_pp) {
		// Group not found
		return NULL;
	}
	
	return *grp_pp;
}

/**
 * \brief Get a description of the IPFIX element with given Elemenent ID and
 * Enterprise ID
 *
 * \param[in] id Element ID
 * \param[in] en Enterprise ID
 * \return On success returns pointer to the element. If the element is unknown,
 * it will return NULL.
 */
const ipfix_element_t *get_element_by_id(uint16_t id, uint32_t en)
{
	// Find the group
	const struct elem_en_group *group = get_en_group_by_id(en);
	if (!group) {
		// Group not found
		return NULL;
	}
	
	// Find the element
	ipfix_element_t el_key;
	const ipfix_element_t *el_key_p;
	const ipfix_element_t **elem_pp;
	
	el_key.id = id;
	el_key_p = &el_key;
	
	elem_pp = bsearch(&el_key_p, group->elements, group->elem_used,
		sizeof(ipfix_element_t *), cmp_elem_by_id);
	if (!elem_pp) {
		// Element not found in the group
		return NULL;
	}
	
	// Suceess
	return *elem_pp;
}

/**
 * \brief Get a description of the IPFIX element with given name
 * 
 * This function allows to search for the element between all known elements or
 * in a group of elements with same Enterprise ID. Search between all elements
 * is enabled by default. If the Enterprise ID is specified at the beginning
 * of the name and is devided from real name with a colon, this function will
 * search only in the group of elements with specified Enterprise ID.
 * Example inputs: "tcpControlBits", "8057:sipvia"
 * \param[in] name Name of the element
 * \param[in] case_sens Enable case sensitivity search
 * \return Structure with a number of suitable results. Only when exactly one
 * result is found, a pointer to the element will be filled. Otherwise the
 * pointer is always NULL.
 */
ipfix_element_result_t get_element_by_name(const char *name, bool case_sens)
{
	if (!name) {
		return (ipfix_element_result_t) {0, NULL};
	}

	// Get a pointer to the main structure
	const struct elem_groups *groups = elem_coll_get();
	if (!groups) {
		// Not initialized
		return (ipfix_element_result_t) {0, NULL};
	}
	
	ipfix_element_t **base = groups->name_index;
	size_t nmemb = groups->name_count;
	int (*cmp_func)(const void *, const void *) = (case_sens) ?
		cmp_elem_by_name_sens : cmp_elem_by_name_ins;
	
	// Is there any enterprise prefix?
	char *colon_pos = NULL;
	if (name[0] >= '0' && name[0] <= '9' && (colon_pos = strchr(name, ':'))) {
		// Convert Enterprise ID
		char *num_end;
		unsigned long int en_id = strtoul(name, &num_end, 10);
		
		if (num_end == colon_pos) {
			// Find the group with Enterprise ID
			const struct elem_en_group *group = get_en_group_by_id(en_id);
			if (!group) {
				// Group not found
				return (ipfix_element_result_t) {0, NULL};
			}
		
			// Change base
			base = group->name_index;
			nmemb = group->elem_used;
			name = colon_pos + 1;
		}		
	}
	
	// Create a key
	ipfix_element_t el_key;
	const ipfix_element_t *el_key_p;
	ipfix_element_t **elem_pp;
	
	el_key.name = (char *) name; // Ugly, but effective
	el_key_p = &el_key;
	
	// Search for the key
	elem_pp = bsearch(&el_key_p, base, nmemb, sizeof(ipfix_element_t *),
		cmp_func);
	if (!elem_pp) {
		// Element not found
		return (ipfix_element_result_t) {0, NULL};
	}
	
	// Check if there are another suitable results
	unsigned int matches = 1;
	
	ipfix_element_t **aux_elem = elem_pp - 1;
	while (aux_elem >= base && cmp_func(aux_elem, elem_pp) == 0) {
		matches++;
		aux_elem--;
	}
	
	aux_elem = elem_pp + 1;
	while (aux_elem < base + nmemb && cmp_func(aux_elem, elem_pp) == 0) {
		matches++;
		aux_elem++;
	}
	
	if (matches > 1) {
		return (ipfix_element_result_t) {matches, NULL};
	}
	
	return (ipfix_element_result_t) {1, *elem_pp};
}

