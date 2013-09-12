#define _GNU_SOURCE
#include <stdio.h>
#include "plugin_header.h"

char * format( const union plugin_arg * arg, int plain_numbers ) {
	char * out;
	char *methods[]={"GET", "POST", "HTTP", "HEAD", "PUT", "OPTIONS", "DELETE", "TRACE", "CONNECT", "PATCH"};

	if (arg[0].uint32 > 10 || arg[0].uint32 == 0) {
		asprintf( &out, "%u", arg[0].uint32 );
	} else {
		asprintf( &out, "%s", methods[(arg[0].uint32)-1] );
	}

	return out;
}
