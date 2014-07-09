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

void format( const union plugin_arg * arg, int plain_numbers, char out[PLUGIN_BUFFER_SIZE])
{
	char *str = NULL;
	char num[PLUGIN_BUFFER_SIZE];

	if (plain_numbers) {
		snprintf(out, PLUGIN_BUFFER_SIZE, "%u", arg[0].uint8);
		return;
	}

	switch (arg[0].uint8) {
		case 0:
			str = "No Error";
			break;
		case 1:
			str = "Format Error";
			break;
		case 2:
			str = "Server Failure";
			break;
		case 3:
			str = "Non-Existent Domain";
			break;
		case 4:
			str = "Not Implemented";
			break;
		case 5:
			str = "Query Refused ";
			break;
		case 6:
			str = "Name Exists when it should not";
			break;
		case 7:
			str = "RR Set Exists when it should not";
			break;
		case 8:
			str = "RR Set that should exist does not";
			break;
		case 9:
			str = "Server Not Authoritative for zone";
			break;
		case 10:
			str = "Name not contained in zone";
			break;
		case 16:
			str = "Bad OPT Version/TSIG Signature Failure ";
			break;
		case 17:
			str = "Key not recognized";
			break;
		case 18:
			str = "Signature out of time window ";
			break;
		case 19:
			str = "Bad TKEY Mode";
			break;
		case 20:
			str = "Duplicate key name";
			break;
		case 21:
			str = "Algorithm not supported";
			break;
		default:
			snprintf(num, PLUGIN_BUFFER_SIZE, "%u", arg[0].uint8);
			str = num;
			break;
	}

	snprintf(out, PLUGIN_BUFFER_SIZE, "%s", str);
}

void parse(char *input, char out[PLUGIN_BUFFER_SIZE])
{
	int code;
	if (!strcasecmp(input, "No Error")) {
		code = 0;
	} else if (!strcasecmp(input, "Format Error")) {
		code = 1;
	} else if (!strcasecmp(input, "Server Failure")) {
		code = 2;
	} else if (!strcasecmp(input, "Non-Existent Domain")) {
		code = 3;
	} else if (!strcasecmp(input, "Not Implemented")) {
		code = 4;
	} else if (!strcasecmp(input, "Query Refused")) {
		code = 5;
	} else if (!strcasecmp(input, "Name Exists when it should not")) {
		code = 6;
	} else if (!strcasecmp(input, "RR Set Exists when it should not")) {
		code = 7;
	} else if (!strcasecmp(input, "RR Set that should exist does not")) {
		code = 8;
	} else if (!strcasecmp(input, "Server Not Authoritative for zone")) {
		code = 9;
	} else if (!strcasecmp(input, "Name not contained in zone")) {
		code = 10;
	} else if (!strcasecmp(input, "Bad OPT Version/TSIG Signature Failure")) {
		code = 16;
	} else if (!strcasecmp(input, "Key not recognized")) {
		code = 17;
	} else if (!strcasecmp(input, "Signature out of time window")) {
		code = 18;
	} else if (!strcasecmp(input, "Bad TKEY Mode")) {
		code = 19;
	} else if (!strcasecmp(input, "Duplicate key name")) {
		code = 20;
	} else if (!strcasecmp(input, "Algorithm not supported")) {
		code = 21;
	} else {
		snprintf(out, PLUGIN_BUFFER_SIZE, "");
		return;
	}
	snprintf(out, PLUGIN_BUFFER_SIZE, "%d", code);
}
