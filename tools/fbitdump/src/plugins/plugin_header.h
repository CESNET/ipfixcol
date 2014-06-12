#ifndef PLUGIN_HEADER_H
#define PLUGIN_HEADER_H
#include "../protocols.h"
#include <inttypes.h>

#define PLUGIN_BUFFER_SIZE 50

union plugin_arg
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
} ;

/**
 * \brief Plugin initialization
 * \return 0 on success
 */
int init();

/**
 * \brief Close plugin
 */
void close();

/**
 * \brief Format data into human readable string
 * \param[in] arg Data arguments
 * \param[in] plain_numbers
 * \param[out] buffer Output string
 */
void format( const union plugin_arg * arg, int plain_numbers, char buffer[PLUGIN_BUFFER_SIZE] );

/**
 * \brief Format data into inner representation
 * \param[in] input Input data
 * \param[out] out Result
 */
void parse( char *input, char out[PLUGIN_BUFFER_SIZE]);


#endif
