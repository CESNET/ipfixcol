#define _GNU_SOURCE
#include <stdio.h>
#include "plugin_header.h"

void format( const union plugin_arg * arg, int plain_numbers, char * buff ) {
	char out[PLUGIN_BUFFER_SIZE]={0};
	char *methods[]={"GET", "POST", "HTTP", "HEAD", "PUT", "OPTIONS", "DELETE", "TRACE", "CONNECT", "PATCH"};

	if (arg[0].uint32 > 10 || arg[0].uint32 == 0) {
		snprintf( out, PLUGIN_BUFFER_SIZE,  "%u", arg[0].uint32 );
	} else {
		snprintf( out, PLUGIN_BUFFER_SIZE, "%s", methods[(arg[0].uint32)-1] );
	}
}
