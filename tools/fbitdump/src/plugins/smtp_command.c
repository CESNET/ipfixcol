/*
 * File:    smtp_command.c
 * Author:  Jan Remes (xremes00@stud.fit.vutbr.cz)
 * Project: ipfixcol
 *
 * Description: This file contains fbitdump plugin for displaying SMTP status
 *              code field in text format
 *
 * Copyright (C) 2015 CESNET
 */

/**** INCLUDES and DEFINES *****/
#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>

#include "plugin_header.h"

#define SEP_CHAR '|'
#define DEFAULT_CHAR '-'
#define CMD_UNKNOWN 0x8000

/***** TYPEDEFs and DATA *****/

typedef char item_t[4];

static const item_t values[] = {
	{ "EH"  },  // EHLO
	{ "HE"  },  // HELO
	{ "ML"  },  // MAIL TO
	{ "RC"  },  // RCPT FROM
	{ "D"   },  // DATA
	{ "RST" },  // RSET
	{ "VF"  },  // VRFY
	{ "EX"  },  // EXPN
	{ "HLP" },  // HELP
	{ "N"   },  // NOOP
	{ "Q"   },  // QUIT
};

#define NAMES_SIZE sizeof(values)

// Get room for:
// - Unknown flag
// - null byte
#define FLAGSIZE NAMES_SIZE + 2

/***** FUNCTIONS *****/
/**
 * Check buffer size
 */
int init(const char *params, void **conf)
{
	*conf = NULL;
	if (PLUGIN_BUFFER_SIZE < FLAGSIZE) {
		return 1;
	} else {
		return 0;
	}
}

char *info()
{
	return \
"Converts 'SMTP command flags' field into text representation\n \
SMTP commands present in the flow are printed in comma-separated list, using\n \
abbreviated codes\n \
\te.g. EHLO -> EH, QUIT -> Q, RSET -> RST\n \
Unknown commands use the 'U' code\n";
}

/**
 * Fill the buffer with text representation of field's content
 */
void format(const plugin_arg_t * arg, int plain_numbers,
		char buffer[PLUGIN_BUFFER_SIZE], void *conf)
{
	// get rid of the warning
	if (plain_numbers) {;}

	int offset = 0;
	int i;

	// Filling in flag values
	for (i = 0; i < NAMES_SIZE; i++) {
		if (arg->val->uint32 & (((uint32_t)1) << i)) {
			snprintf(buffer + offset, PLUGIN_BUFFER_SIZE - offset, "%s,", values[i]);
			// there is extra comma
			offset += (strlen(values[i]) + 1);
		}
	}

	if (arg->val->uint32 & CMD_UNKNOWN) {
		buffer[offset] = 'U';
		buffer[offset + 1] = '\0';
	} else {
		if (offset) {
			// remove trailing comma
			buffer[offset - 1] = '\0';
		}
	}
}

/**
 * Parse text data and return inner format
 */
void parse(char *input, char out[PLUGIN_BUFFER_SIZE], void *conf)
{
	int i;
	uint32_t result = 0;

	for (i = 0; i < NAMES_SIZE; i++) {
		if (strcasestr(input, values[i])) {
			result |= ((uint32_t)1) << i;
		}
	}

	if (strcasestr(input, "U")) {
		result |= CMD_UNKNOWN;
	}

	snprintf(out, PLUGIN_BUFFER_SIZE, "%u", result);
}
