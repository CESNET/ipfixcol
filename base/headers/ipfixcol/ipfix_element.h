/**
 * \file ipfix_element.h
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
 
#ifndef _IPFIX_ELEMENTS_
#define _IPFIX_ELEMENTS_

#include <stdint.h>
#include <stdbool.h>

#include "api.h"

/**
 * \defgroup ipfixElemAPI Auxiliary functions for getting a description of IPFIX
 * elements
 * \ingroup publicAPIs
 *
 * @{
 */

/** 
 * \enum ELEMENT_TYPE
 * \brief IPFIX element type
 * 
 * The type distinguish several general types based on RFC 5610
 */
enum ELEMENT_TYPE {
	ET_OCTET_ARRAY = 0,
	ET_UNSIGNED_8,
	ET_UNSIGNED_16,
	ET_UNSIGNED_32,
	ET_UNSIGNED_64,
	ET_SIGNED_8,
	ET_SIGNED_16,
	ET_SIGNED_32,
	ET_SIGNED_64,
	ET_FLOAT_32,
	ET_FLOAT_64,
	ET_BOOLEAN,
	ET_MAC_ADDRESS,
	ET_STRING,
	ET_DATE_TIME_SECONDS,
	ET_DATE_TIME_MILLISECONDS,
	ET_DATE_TIME_MICROSECONDS,
	ET_DATE_TIME_NANOSECONDS,
	ET_IPV4_ADDRESS,
	ET_IPV6_ADDRESS,
	ET_BASIC_LIST,
	ET_SUB_TEMPLATE_LIST,
	ET_SUB_TEMPLATE_MULTILIST,
	ET_UNASSIGNED = 255
};

/**
 * \enum ELEMENT_SEMANTIC
 * \breif IPFIX element semantics
 *
 * The type distinguish several general semantics based on RFC 5610
 */
enum ELEMENT_SEMANTIC {
	ES_DEFAULT = 0,
	ES_QUANTITY,
	ES_TOTAL_COUNTER,
	ES_DELTA_COUNTER,
	ES_IDENTIFIER,
	ES_FLAGS,
	ES_LIST,
	ES_UNASSIGNED = 255
};


/**
 * \brief IPFIX element definition 
 */
typedef struct {
	/** Element ID */
	uint16_t id;
	/** Enterprise ID */
	uint32_t en;
	/** Name of the element */
	char *name;
	/** Data type */
	enum ELEMENT_TYPE type;
	/** Data semantic */
	enum ELEMENT_SEMANTIC semantic;
} ipfix_element_t;

/**
 * \brief Result of searching for an element by name (get_element_by_name())
 */
typedef struct {
	unsigned int count;            /**< Number of suitable matches for query  */
	const ipfix_element_t *result; /**< Result - \b only when count == 1      */
} ipfix_element_result_t;


/**
 * \brief Get a description of the IPFIX element with given Elemenent ID and
 * Enterprise ID
 *
 * \param[in] id Element ID
 * \param[in] en Enterprise ID
 * \return On success returns pointer to the element. If the element is unknown,
 * it will return NULL.
 */
API const ipfix_element_t *get_element_by_id(uint16_t id, uint32_t en);

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
API ipfix_element_result_t get_element_by_name(const char *name, 
	bool case_sens);

#endif /* _IPFIX_ELEMENTS_ */

/**@}*/
