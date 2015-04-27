#define _GNU_SOURCE
#include <stdio.h>
#include <strings.h>
#include "plugin_header.h"

char *info()
{
	return "";
}

void format(const plugin_arg_t * arg, int plain_numbers, char buff[PLUGIN_BUFFER_SIZE], void *conf)
{
	char *methods[]={"INVITE", "ACK", "BYE", "CANCEL", "OPTIONS", "REGISTER", "PRACK", "SUBSCRIBE",
		"NOTIFY", "PUBLISH", "INFO", "REFER", "MESSAGE", "UPDATE"};

	if (arg->val[0].uint32 > 14 || arg->val[0].uint32 == 0 || plain_numbers) {
		snprintf( buff, PLUGIN_BUFFER_SIZE,  "%u", arg->val[0].uint32 );
	} else {
		snprintf( buff, PLUGIN_BUFFER_SIZE, "%s", methods[(arg->val[0].uint32)-1] );
	}
}

void parse(char *input, char out[PLUGIN_BUFFER_SIZE], void *conf)
{
	int code;
	if (!strcasecmp(input, "INVITE")) {
		code = 1;
	} else if (!strcasecmp(input, "ACK")) {
		code = 2;
	} else if (!strcasecmp(input, "BYE")) {
		code = 3;
	} else if (!strcasecmp(input, "CANCEL")) {
		code = 4;
	} else if (!strcasecmp(input, "OPTIONS")) {
		code = 5;
	} else if (!strcasecmp(input, "REGISTER")) {
		code = 6;
	} else if (!strcasecmp(input, "PRACK")) {
		code = 7;
	} else if (!strcasecmp(input, "SUBSCRIBE")) {
		code = 8;
	} else if (!strcasecmp(input, "NOTIFY")) {
		code = 9;
	} else if (!strcasecmp(input, "PUBLISH")) {
		code = 10;
	} else if (!strcasecmp(input, "INFO")) {
		code = 11;
	} else if (!strcasecmp(input, "REFER")) {
		code = 12;
	} else if (!strcasecmp(input, "MESSAGE")) {
		code = 13;
	} else if (!strcasecmp(input, "UPDATE")) {
		code = 14;
	} else {
		snprintf(out, PLUGIN_BUFFER_SIZE, "%s", "");
		return;
	}

	snprintf(out, PLUGIN_BUFFER_SIZE, "%d", code);
}
