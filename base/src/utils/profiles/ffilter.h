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
#define FF_MULTINODE_MAX 4

typedef struct ff_ip_s { uint32_t data[4]; } ff_ip_t; /*!< IPv4/IPv6 address */

/*! \brief Supported data types */
typedef enum {
	FF_TYPE_UNSUPPORTED = 0x0,  // for unsupported data types
	FF_TYPE_UNSIGNED,
	FF_TYPE_UNSIGNED_BIG,
	FF_TYPE_SIGNED,
	FF_TYPE_SIGNED_BIG,
	FF_TYPE_UINT8,				/* 1Byte unsigned - fixed size */
	FF_TYPE_UINT16,
	FF_TYPE_UINT32,
	FF_TYPE_UINT64,
	FF_TYPE_INT8,				/* 1Byte unsigned - fixed size */
	FF_TYPE_INT16,
	FF_TYPE_INT32,
	FF_TYPE_INT64,
	FF_TYPE_DOUBLE,        // TODO: muzeme si byt jisti, ze se bude pouzivat format IEEE 754? Podla standardu C vraj ano
	FF_TYPE_ADDR,
	FF_TYPE_MAC,
	FF_TYPE_STRING,
	FF_TYPE_MPLS,
	FF_TYPE_TIMESTAMP     // TODO: jaky format??
} ff_type_t;

//TODO: Nebol by lepsi typedef ? Alebo mienene pouzite bolo len pre sizeof()
#define FF_TYPE_UNSUPPORTED_T void
#define FF_TYPE_UNSIGNED_T char*
#define FF_TYPE_UNSIGNED_BIG_T char*
#define FF_TYPE_SIGNED_T char*
#define FF_TYPE_SIGNED_BIG_T char*
#define FF_TYPE_UINT8_T uint8_t
#define FF_TYPE_UINT16_T uint8_t
#define FF_TYPE_UINT32_T uint32_t
#define FF_TYPE_UINT64_T uint64_t
#define FF_TYPE_INT8_T int8_t
#define FF_TYPE_INT16_T int16_t
#define FF_TYPE_INT32_T int32_t
#define FF_TYPE_INT64_T int64_t
#define FF_TYPE_DOUBLE_T double
#define FF_TYPE_FLOAT_T double
#define FF_TYPE_ADDR_T ff_ip_t
#define FF_TYPE_MAC_T char[8]
#define FF_TYPE_STRING_T char*
#define FF_TYPE_MPLS_T unit32_t[10]
#define FF_TYPE_TIMESTAMP_T unit64_t

typedef void ff_unsupported_t;
typedef char* ff_uint_t;
typedef char* ff_int_t;
typedef uint8_t ff_uint8_t;
typedef uint16_t ff_uint16_t;
typedef uint32_t ff_uint32_t;
typedef uint64_t ff_uint64_t;
typedef int8_t ff_int8_t;
typedef int16_t ff_int16_t;
typedef int32_t ff_int32_t;
typedef int64_t ff_int64_t;
typedef double ff_double_t;
typedef double ff_float_t;
typedef ff_ip_t ff_addr_t;
typedef char ff_mac_t[8];
typedef char* ff_string_t;
typedef uint32_t ff_mpls_t[10];
typedef uint64_t ff_timestamp_t;



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

/** \brief External identification of value */
typedef union {
	uint64_t index;       /**< Index mapping      */
	const void *ptr;      /**< Direct mapping     */
} ff_extern_id_t;

/** \brief Identification of left value */
typedef struct ff_lvalue_s {
	/** Type of left value */
	ff_type_t type;
	/** External identification */
	ff_extern_id_t id[FF_MULTINODE_MAX];
	int options;

	// POZN: velikost datoveho typu nemuze byt garantovana IPFIXcolem a muze
	//       se lisit v zavislosti na velikostech dat posilanych exporterem
	//       -> velikost dat si bude muset zjistit komparacni funkce a podle
	//       toho se bude muset zachovat
} ff_lvalue_t;


//typedef struct ff_s ff_t;
struct ff_s;

/** \typedef Prototype of function pointer on element lookup function
 *
 * \param[in] ff_s Filter pointer
 * \param[in] string Name of element to identify
 * \return lvalue identification
 */
typedef ff_error_t (*ff_lookup_func_t) (struct ff_s *, const char *, ff_lvalue_t *);
typedef ff_error_t (*ff_data_func_t) (struct ff_s*, void *, ff_extern_id_t, char*, size_t *);
typedef ff_error_t (*ff_rval_map_func_t) (struct ff_s *, const char *, ff_type_t, ff_extern_id_t, char*, size_t* );

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
