/*

 Copyright (c) 2015, Tomas Podermanski, Lukas Hutak

 This file is part of libnf.net project.

 Libnf is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 Libnf is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with libnf.  If not, see <http://www.gnu.org/licenses/>.

*/

/*! \file ff_filter.h
    \brief netflow fiter implementation - C interface
*/
#ifndef _FLOW_FILTER_H_
#define _FLOW_FILTER_H_//

#include <stdint.h>
#include <stddef.h>

#define FF_MAX_STRING  1024
#define FF_SCALING_FACTOR  1000LL


typedef struct ff_ip_s { uint32_t data[4]; } ff_ip_t; /*!< IPv4/IPv6 address */
typedef struct ff_net_s { ff_ip_t ip; ff_ip_t mask; } ff_net_t;

/*! \brief Supported data types */
typedef enum {
	FF_TYPE_UNSUPPORTED = 0x0,  // for unsupported data types
#define FF_TYPE_UNSUPPORTED_T void
//	FF_TYPE_SIGNED,
#define FF_TYPE_UNSIGNED_T char*
	FF_TYPE_UNSIGNED,
#define FF_TYPE_UNSIGNED_BIG_T char*
	FF_TYPE_UNSIGNED_BIG,
#define FF_TYPE_SIGNED_T char*
	FF_TYPE_SIGNED,
#define FF_TYPE_SIGNED_BIG_T char*
	FF_TYPE_SIGNED_BIG,
#define FF_TYPE_UINT8_T uint8_t
	FF_TYPE_UINT8,				/* 1Byte unsigned - fixed size */
#define FF_TYPE_UINT16_T uint8_t
	FF_TYPE_UINT16,
#define FF_TYPE_UINT32_T uint32_t
	FF_TYPE_UINT32,
#define FF_TYPE_UINT64_T uint64_t
	FF_TYPE_UINT64,
#define FF_TYPE_INT8_T int8_t
	FF_TYPE_INT8,				/* 1Byte unsigned - fixed size */
#define FF_TYPE_INT16_T int8_t
	FF_TYPE_INT16,
#define FF_TYPE_INT32_T int32_t
	FF_TYPE_INT32,
#define FF_TYPE_INT64_T int64_t
	FF_TYPE_INT64,
#define FF_TYPE_DOUBLE_T double
	FF_TYPE_DOUBLE,        // TODO: muzeme si byt jisti, ze se bude pouzivat format IEEE 754?
#define FF_TYPE_FLOAT_T double
	FF_TYPE_ADDR,
#define FF_TYPE_ADDR_T ff_ip_t
	FF_TYPE_MAC,
#define FF_TYPE_MAC_T char[8]
	FF_TYPE_STRING,
#define FF_TYPE_STRING_T char*
	FF_TYPE_MPLS,
#define FF_TYPE_MPLS_T unit32_t[10]
	FF_TYPE_TIMESTAMP     // jaky format??
#define FF_TYPE_TIMESTAMP_T unit64_t
} ff_type_t;

typedef enum {
	FF_OK = 0x1,
	FF_ERR_NOMEM = -0x1,
	FF_ERR_UNKN  = -0x2,
	FF_ERR_UNSUP  = -0x3,
	FF_ERR_OTHER  = -0xE,
	FF_ERR_OTHER_MSG  = -0xF,
} ff_error_t;

typedef enum {
	FFOPTS_MULTINODE = 0x01,
	FFOPTS_FLAGS = 0x02,
} ff_opts_t;

/*! \brief External identification of value */
typedef union {
	uint64_t index;       /**< Index mapping      */
	const void *ptr;      /**< Direct mapping     */
} ff_extern_id_t;



/** \brief Identification of left value */
typedef struct ff_lvalue_s {
	/** Type of left value */
	ff_type_t type;
	/** External identification */
	ff_extern_id_t id;
	ff_extern_id_t id2;	/* for pair/extra fields  */
	ff_extern_id_t *more;
	int num;
	int options;

	// POZN: velikost datoveho typu nemuze byt garantovana IPFIXcolem a muze
	//       se lisit v zavislosti na velikostech dat posilanych exporterem
	//       -> velikost dat si bude muset zjistit komparacni funkce a podle
	//       toho se bude muset zachovat
} ff_lvalue_t;


//typedef struct ff_s ff_t;
struct ff_s;

/** \brief Function pointer on element lookup function
 *
 * - first: Name of the element
 * - returns: lvalue identification
 */
typedef ff_error_t (*ff_lookup_func_t) (struct ff_s *, const char *, ff_lvalue_t *);
typedef ff_error_t (*ff_data_func_t) (struct ff_s*, void *, ff_extern_id_t, char*, size_t *);

typedef ff_error_t (*ff_rval_map_func_t) (struct ff_s *, const char *, ff_extern_id_t, uint64_t *);

//typedef ff_error_t (*ff__func_t) (struct ff_s*, void *, ff_extern_id_t, char*, size_t *);


/** \brief Options  */
typedef struct ff_options_s {

	/** Element lookup function */
	ff_lookup_func_t ff_lookup_func;
	/** Value comparation function */
	ff_data_func_t ff_data_func;
	/** Literal constants translation function eg. TCP->6 */
	ff_rval_map_func_t ff_rval_map_func;
} ff_options_t;


/** \brief Filter instance */
typedef struct ff_s {

	ff_options_t	options;

	void *root;
	char error_str[FF_MAX_STRING];

} ff_t;


ff_error_t ff_options_init(ff_options_t **ff_options);
ff_error_t ff_options_free(ff_options_t *ff_options);

ff_error_t ff_init(ff_t **ff_filter, const char *expr, ff_options_t *ff_options);
int ff_eval(ff_t *filter, void *rec);
ff_error_t ff_free(ff_t *filter);

void ff_set_error(ff_t *filter, char *format, ...);
const char* ff_error(ff_t *filter, const char *buf, int buflen);


#endif /* _LNF_FILTER_H */
