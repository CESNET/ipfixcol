#ifndef PLUGIN_HEADER_H
#define PLUGIN_HEADER_H
#include "../protocols.h"
#include <inttypes.h>

#define PLUGIN_BUFFER_SIZE 50

union plugin_arg_val
{
	char int8;
	unsigned char uint8;
	int16_t int16;
	uint16_t uint16;
	int32_t int32;
	uint32_t uint32;
	int64_t int64;
	uint64_t uint64;
	float flt;
	double dbl;
	struct {
		uint64_t length;
		const char *ptr;
	} blob;
};


typedef struct {
	int type;
	const union plugin_arg_val *val;
	const char *text;
} plugin_arg_t;


enum val_type {
	UNKNOWN = 0,
	/// A special eight-byte ID type for internal use.
	OID,
	INT8,	///!< One-byte signed integers, internally char.
	UINT8,	///!< One-byte unsigned integers, internally unsigned char.
	INT16,	///!< Two-byte signed integers, internally int16_t.
	UINT16, ///!< Two-byte unsigned integers, internally uint16_t.
	INT32,	///!< Four-byte signed integers, internally int32_t.
	UINT32,	///!< Four-byte unsigned integers, internally uint32_t.
	INT64,	///!< Eight-byte signed integers, internally int64_t.
	UINT64,	///!< Eight-byte unsigned integers, internally uint64_t.
	FLOAT,	///!< Four-byte IEEE floating-point numbers, internally float.
	DOUBLE, ///!< Eight-byte IEEE floating-point numbers, internally double.
	/// Low cardinality null-terminated strings.  Strings are
	/// internally stored with the null terminators.  Each string value
	/// is intended to be treated as a single atomic item.
	CATEGORY,
	/// Arbitrary null-terminated strings.  Strings are internally
	/// stored with the null terminators.  Each string could be further
	/// broken into tokens for a full-text index known as keyword
	/// index.  Could search for presence of some keywords through
	/// expression "contains" such as "contains(textcolumn, 'Berkeley',
	/// 'California')".
	TEXT,
	/// Byte array.  Also known as Binary Large Objects (blob) or
	/// opaque objects.  A column of this type requires special
	/// handling for input and output.  It can not be used as a part of
	/// any searching criteria.
	BLOB,
	/// User-defined type.  FastBit does not know much about it.
	UDT
};

/**
 * \brief Plugin initialization
 * \return 0 on success
 */
int init(const char *params, void **conf);

/**
 * \brief Close plugin
 */
void close(void **conf);

/**
 * \brief Format data into human readable string
 * \param[in] arg Data arguments
 * \param[in] plain_numbers
 * \param[out] buffer Output string
 */
void format(const plugin_arg_t *arg, int plain_numbers, char buffer[PLUGIN_BUFFER_SIZE], void *conf );

/**
 * \brief Format data into inner representation
 * \param[in] input Input data
 * \param[out] out Result
 */
void parse( char *input, char out[PLUGIN_BUFFER_SIZE], void *conf);

/**
 * \brief Plugin destription
 * \return NULL terminated string with plugin description
 */
char *info();

#endif
