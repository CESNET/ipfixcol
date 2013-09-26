#define _GNU_SOURCE
#include <stdio.h>
#include "plugin_header.h"

void format( const union plugin_arg * arg, int plain_numbers, char *out)
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
