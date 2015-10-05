/**
 * \file element.h
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

#ifndef _ELEMENT_H_
#define _ELEMENT_H_

#include <stdint.h>
#include <stdbool.h>

#include <ipfixcol.h>

/**
 * \brief Structure for IPFIX elements with same Enterprise ID
 */
struct elem_en_group {
	uint32_t en_id;                /**< Enterprise ID of elements family      */
	ipfix_element_t **elements;    /**< Array of elements (sorted by ID)      */
	unsigned int elem_used;        /**< Number of used elements               */
	unsigned int elem_max;         /**< Max. number of elements               */
	
	ipfix_element_t **name_index;  /**< Same as "elements" but sorted by name */
};

/**
 * \brief Structure for handling groups of IPFIX elements
 */
struct elem_groups {
	struct elem_en_group **groups; /**< Array of IPFIX groups                 */
	unsigned int elem_used;        /**< Number of existing groups             */
	unsigned int elem_max;         /**< Max. number of groups                 */
	
	// Do not free content of the array below!
	ipfix_element_t **name_index;  /**< Array of all elements sorted by name  */
	unsigned int name_count;       /**< Size of the array sorted by name      */
};

// Create structures for elements
struct elem_groups *elements_init();

// Load data from file and parse structure
int elements_load(int file_descriptor, struct elem_groups *ipfix_groups);

// Delete loaded data and structures
void elements_destroy(struct elem_groups *ipfix_groups);


// Comparsion function for groups of elements
int cmp_groups(const void *g1, const void *g2);

// Comparsion function for elements with same Enterprise ID
int cmp_elem_by_id(const void *e1, const void *e2);

// Comparsion function for elements' names (case senstive)
int cmp_elem_by_name_sens(const void *e1, const void *e2);

// Comparsion function for elements' names (case insenstive)
int cmp_elem_by_name_ins(const void *e1, const void *e2);

#endif /* _ELEMENT_H_ */

