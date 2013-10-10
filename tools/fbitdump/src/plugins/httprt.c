#define _GNU_SOURCE
#include <stdio.h>
#include "plugin_header.h"

void format( const union plugin_arg * arg, int plain_numbers, char buff[PLUGIN_BUFFER_SIZE] ) {
	char *methods[]={"GET", "POST", "HTTP", "HEAD", "PUT", "OPTIONS", "DELETE", "TRACE", "CONNECT", "PATCH"};

	if (arg[0].uint32 > 10 || arg[0].uint32 == 0 || plain_numbers) {
		snprintf( buff, PLUGIN_BUFFER_SIZE,  "%u", arg[0].uint32 );
	} else {
		snprintf( buff, PLUGIN_BUFFER_SIZE, "%s", methods[(arg[0].uint32)-1] );
	}
}
