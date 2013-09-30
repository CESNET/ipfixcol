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
} ;

void format( const union plugin_arg * arg, int plain_numbers, char buffer[PLUGIN_BUFFER_SIZE] );


#endif
