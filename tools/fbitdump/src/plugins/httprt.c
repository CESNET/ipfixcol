#define _GNU_SOURCE
#include <stdio.h>
#include <strings.h>
#include "plugin_header.h"

static const char *methods[] = {
	"GET", 
	"POST", 
	"HTTP", 
	"HEAD", 
	"PUT", 
	"OPTIONS", 
	"DELETE", 
	"TRACE", 
	"CONNECT", 
	"PATCH"
};

#define MSG_CNT (sizeof(methods) / sizeof(methods[0]))

char *info()
{
	return \
"Converts HTTP method name to value and vice versa.\n \
Supported methods: GET, POST, HTTP, HEAD, PUT, OPTIONS, DELETE, TRACE, CONNECT, PATCH";
}

void format(const plugin_arg_t * arg, int plain_numbers, char buff[PLUGIN_BUFFER_SIZE], void *conf)
{
	if (arg->val[0].uint32 > MSG_CNT || arg->val[0].uint32 == 0 || plain_numbers) {
		snprintf(buff, PLUGIN_BUFFER_SIZE, "%u", arg->val[0].uint32);
	} else {
		snprintf(buff, PLUGIN_BUFFER_SIZE, "%s", methods[(arg->val[0].uint32) - 1]);
	}
}

void parse(char *input, char out[PLUGIN_BUFFER_SIZE], void *conf)
{
	int code, size = MSG_CNT;
	
	for (code = 0; code < size; code++) {
		if (!strcasecmp(input, methods[code])) {
			break;
		}
	}
	
	// Return empty string if HTTP method name was not found
	if (code == size) {
		snprintf(out, PLUGIN_BUFFER_SIZE, "%s", "");
		return;
	}
	
	snprintf(out, PLUGIN_BUFFER_SIZE, "%d", code + 1);
}
