#define _GNU_SOURCE
#include <stdio.h>
#include "plugin_header.h"

int init()
{
	return 0;
}

void close()
{
}

void format( const union plugin_arg * arg, int plain_numbers, char buff[PLUGIN_BUFFER_SIZE] ) {
	char *methods[]={"GET", "POST", "HTTP", "HEAD", "PUT", "OPTIONS", "DELETE", "TRACE", "CONNECT", "PATCH"};

	if (arg[0].uint32 > 10 || arg[0].uint32 == 0 || plain_numbers) {
		snprintf( buff, PLUGIN_BUFFER_SIZE,  "%u", arg[0].uint32 );
	} else {
		snprintf( buff, PLUGIN_BUFFER_SIZE, "%s", methods[(arg[0].uint32)-1] );
	}
}

void parse(char *input, char out[PLUGIN_BUFFER_SIZE])
{
	int code;
	if (!strcasecmp(input, "GET")) {
		code = 1;
	} else if (!strcasecmp(input, "POST")) {
		code = 2;
	} else if (!strcasecmp(input, "HTTP")) {
		code = 3;
	} else if (!strcasecmp(input, "HEAD")) {
		code = 4;
	} else if (!strcasecmp(input, "PUT")) {
		code = 5;
	} else if (!strcasecmp(input, "OPTIONS")) {
		code = 6;
	} else if (!strcasecmp(input, "DELETE")) {
		code = 7;
	} else if (!strcasecmp(input, "TRACE")) {
		code = 8;
	} else if (!strcasecmp(input, "CONNECT")) {
		code = 9;
	} else if (!strcasecmp(input, "PATCH")) {
		code = 10;
	} else {
		snprintf(out, PLUGIN_BUFFER_SIZE, "");
		return;
	}
	snprintf(out, PLUGIN_BUFFER_SIZE, "%d", code);
}
