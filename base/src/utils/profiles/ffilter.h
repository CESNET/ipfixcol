/*

 Copyright (c) 2015, Tomas Podermanski, Lukas Hutak, Imrich Stoffa

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

/*! \file ffilter.h
	\brief netflow fiter implementation - C interface
*/
#ifndef _FLOW_FILTER_H_
#define _FLOW_FILTER_H_//

#include <stdint.h>
#include <stddef.h>

#define FF_MAX_STRING  1024
#define FF_SCALING_FACTOR  1000LL
#define FF_MULTINODE_MAX 4

#ifndef HAVE_HTONLL
#ifdef WORDS_BIGENDIAN
#   define ntohll(n)    (n)
#   define htonll(n)    (n)
#else
#   define ntohll(n)    ((((uint64_t)ntohl(n)) << 32) | ntohl(((uint64_t)(n)) >> 32))
#   define htonll(n)    ((((uint64_t)htonl(n)) << 32) | htonl(((uint64_t)(n)) >> 32))
#endif
#define HAVE_HTONLL 1
#endif

typedef struct ff_ip_s { uint32_t data[4]; } ff_ip_t; /*!< IPv4/IPv6 address */
typedef union {
	//TODO: Test on big-endian machine
	struct {
		unsigned eos : 1;
		unsigned exp : 3;
		unsigned label : 20;
		unsigned none : 8;
	};
	uint32_t data;
} ff_mpls_label_t;

/*! \brief Supported data types */
typedef enum {
	FF_TYPE_UNSUPPORTED = 0x0,	// for unsupported data types
	FF_TYPE_UNSIGNED,
	FF_TYPE_UNSIGNED_BIG,
	FF_TYPE_SIGNED,
	FF_TYPE_SIGNED_BIG,
	FF_TYPE_UINT8,			/* 1Byte unsigned - fixed size */
	FF_TYPE_UINT16,
	FF_TYPE_UINT32,
	FF_TYPE_UINT64,
	FF_TYPE_INT8,			/* 1Byte unsigned - fixed size */
	FF_TYPE_INT16,
	FF_TYPE_INT32,
	FF_TYPE_INT64,
	FF_TYPE_DOUBLE,			/* muzeme si byt jisti, ze se bude pouzivat format IEEE 754. */
	FF_TYPE_ADDR,
	FF_TYPE_MAC,
	FF_TYPE_STRING,
	//TODO: Implement
	FF_TYPE_MPLS,
	FF_TYPE_TIMESTAMP,      /* uint64_t bit timestamp eval as unsigned, milliseconds from 1-1-1970 00:00:00 */
	FF_TYPE_TIMESTAMP_BIG,  /* uint64_t bit timestamp eval as unsigned, to host byte order conversion required */
} ff_type_t;

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
#define FF_TYPE_MPLS_T uint32_t[10]
#define FF_TYPE_TIMESTAMP_T unit64_t

//Some of the types here are useless - why define another fixed size types ?
typedef void ff_unsup_t;
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
//typedef double ff_float_t;
typedef ff_ip_t ff_addr_t;
typedef char ff_mac_t[8];
typedef char* ff_string_t;
typedef ff_mpls_label_t ff_mpls_stack_t[10];		//WTF ? why 10 ? is it because of the way libnf treats mpls
typedef uint64_t ff_timestamp_t;



/**
 * \typedef ffilter interface return codes
 */
typedef enum {
	FF_OK = 0x1,				/**< No error occuried */
	FF_ERR_NOMEM = -0x1,
	FF_ERR_UNKN  = -0x2,
	FF_ERR_UNSUP  = -0x3,
	FF_ERR_OTHER  = -0xE,
	FF_ERR_OTHER_MSG  = -0xF, 	/**< Closer description of fault can be received from ff_error */
} ff_error_t;

/**
 * \typedef ffilter lvalue options
 *
 */
typedef enum {
	FF_OPTS_NONE = 0,
	FF_OPTS_MULTINODE = 0x01,	/**< Lvalue identificates more data filelds */
	FF_OPTS_FLAGS = 0x02,		/**< Item is of flag type, this change behaviour when no operator is set to bit compare */
	FF_OPTS_MPLS_LABEL,
	FF_OPTS_MPLS_EOS,
	FF_OPTS_MPLS_EXP,
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
	/** External identification of data field */
	ff_extern_id_t id[FF_MULTINODE_MAX];
	/** Extra options that modiflies evaluation of data */
	//TODO: Clarify purpose, maybe create getters and setters
	int options;
	/** 0 for not set */
	int n;
} ff_lvalue_t;

//typedef struct ff_s ff_t;
struct ff_s;

/**{@ \section ff_options_t
 *	Clarify purpose of options object in filter
 */

/**
 * \typedef Lookup callback signature.
 * \brief Lookup the field name found in filter expresson and identify its type one of and associated data elements
 * Callback fills in information about field into ff_lvalue_t sturcture. Required information are external
 * identification of field as understood by data function, data type of filed as one of ff_type_t enum
 * \param filter Filter object
 * \param[in] fieldstr Name of element to identify
 * \param[out] lvalue identification representing field
 * \return FF_OK on success
 */
typedef ff_error_t (*ff_lookup_func_t) (struct ff_s *filter, const char *fieldstr, ff_lvalue_t *lvalue);

/**
 * \typedef Data Callback signature
 * \brief Select requested data from record.
 * Callback copies data associated with external identification, as set by lookup callback, from evaluated record
 * to buffer and marks length of these data. Structure of record must be known to data function.
 * \param ff_s Filter object
 * \param[in] record General data pointer to record
 * \param[in] id Indentfication of data field in recrod
 * \param[out] buf Buffer to store retrieved data
 * \param[out] vsize Length of retrieved data
 */
typedef ff_error_t (*ff_data_func_t) (struct ff_s*, void *, ff_extern_id_t, char*, size_t *);

/**
 * \typedef Rval_map Callback signature
 * \brief Translate constant values unresolved by filter convertors.
 * Callback is used to transform literal value to given ff_type_t when internal conversion function fails.
 * \param ff_s Filter object
 * \param[in] valstr String representation of value
 * \param[in] type Required ffilter internal type
 * \param[in] id External identification of field (form transforming exceptions like flags)
 * \param[out] buf Buffer to copy data
 * \param[out] size Length of valid data in buffer
 */
typedef ff_error_t (*ff_rval_map_func_t) (struct ff_s *, const char *, ff_type_t, ff_extern_id_t, char*, size_t* );

/** \typedef Filter options callbacks  */
typedef struct ff_options_s {
	/** Element lookup function */
	ff_lookup_func_t ff_lookup_func;
	/** Value comparation function */
	ff_data_func_t ff_data_func;
	/** Literal constants translation function eg. TCP->6 */
	ff_rval_map_func_t ff_rval_map_func;
} ff_options_t;

/**@}*/

/** \brief Filter object instance */
typedef struct ff_s {

	ff_options_t    options;	/**< Callback functions */
	void            *root;		/**< Internal representation of filter expression */
	char            error_str[FF_MAX_STRING];	/**< Last error set */

} ff_t;

/**
 * \brief Options constructor
 * allocates options structure
 * \param ff_options
 * \return FF_OK on success
 */
ff_error_t ff_options_init(ff_options_t **ff_options);

/**
 * \brief Options destructor
 * frees options structure
 * \param ff_options Address of pointer to options
 * \return FF_OK on success
 */
ff_error_t ff_options_free(ff_options_t *ff_options);

/**
 * \brief Create filter structure and compile filter expression using callbacks in options
 * First filter object is created then expr is compiled to internal representation.
 * Options callbacks provides following:
 * Lookup identifies the valid lvalue field names and associated filed data types.
 * Data callback sellects associated data for each identificator during evaluation
 * Rval_map callback provides translations to literal constants in value fileds eg.: "SSH"->22 etc.
 * \param ff_filter Address of pointer to filter object
 * \param expr Filter expression
 * \param ff_options Associated options containig callbacks
 * \return FF_OK on success
 */
ff_error_t ff_init(ff_t **ff_filter, const char *expr, ff_options_t *ff_options);

/**
 * \brief Evaluate filter on data
 * \param filter Compiled filter object
 * \param rec Data record in form readable to data callback
 * \return Nonzero on match
 */
int ff_eval(ff_t *filter, void *rec);

/**
 * \brief Release memory allocated for filter object and destroy it
 * \parqm filter Compiled filter object
 * \return FF_OK on success
 */
ff_error_t ff_free(ff_t *filter);

/**
 * \brief Set error string to filter object
 * \param filter Compiled filter object
 * \param format Format string as used in printf
 */
void ff_set_error(ff_t *filter, char *format, ...);

//TODO: Pass only constant pointer to error message
/**
 * \brief Retrive last error set form filter object
 * \param filter Compiled filter object
 * \param buf Place where to copy err string
 * \param buflen Length of data available
 * \return Pointer to copied error string
 */
const char* ff_error(ff_t *filter, const char *buf, int buflen);


#endif /* _LNF_FILTER_H */
