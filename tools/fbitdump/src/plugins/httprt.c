#define _GNU_SOURCE
#include <stdio.h>
#include "plugin_header.h"

char * format( const union plugin_arg * arg, int plain_numbers ) {
	char * out;
	char *methods[]={"GET", "POST", "HTTP", "HEAD", "PUT", "OPTIONS", "DELETE", "TRACE", "CONNECT", "PATCH"};

	asprintf( &out, "%s", methods[(arg[0].int16)-1] );
	return out;
}