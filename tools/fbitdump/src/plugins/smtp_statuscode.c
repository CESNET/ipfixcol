/*
 * File:    smtp_statuscode.c
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
#define SC_UNKNOWN 0x80000000
#define SC_SPAM 0x40000000

/***** TYPEDEFs and DATA *****/

typedef struct {
	unsigned position;
	char flag;
} item_t;

static const item_t values[] = {
	{  0, 'S' }, // 211 System status, or system help reply
	{  1, 'H' }, // 214 Help message
	{  2, 'R' }, // 220 <domain> Service ready
	{  3, 'C' }, // 221 <domain> Service closing transmission channel
	{  4, 'O' }, // 250 Requested mail action okay, completed
	{  5, 'U' }, // 251 User not local; will forward to <forward-path>
	{  6, 'V' }, // 252 Cannot VRFY user, but will accept message and attempt delivery
	/* Vertical bar will be here */
	{  8, 'I' }, // 354 Start mail input; end with <CRLF>.<CRLF>
	/* Vertical bar will be here */
	{ 10, 'N' }, // 421 Service not available, closing transmission channel
	{ 11, 'M' }, // 450     Requested mail action not taken: mailbox unavailable
	{ 12, 'L' }, // 451     Requested action aborted: local error in processing
	{ 13, 'S' }, // 452     Requested action not taken: insufficient system storage
	{ 14, 'P' }, // 455     Cannot accomodate parameter (http://tools.ietf.org/html/rfc1869)
	/* Vertical bar will be here */
	{ 16, 'C' }, // 500     Syntax error, command unrecognised
	{ 17, 'A' }, // 501     Syntax error in parameters or arguments
	{ 18, 'I' }, // 502     Command not implemented
	{ 19, 'S' }, // 503     Bad sequence of commands
	{ 20, 'P' }, // 504     Command parameter not implemented
	{ 21, 'M' }, // 550     Requested action not taken: mailbox unavailable
	{ 22, 'U' }, // 551     User not local; please try <forward-path>
	{ 23, 'E' }, // 552     Requested mail action aborted: exceeded storage allocation
	{ 24, 'N' }, // 553     Requested action not taken: mailbox name not allowed
	{ 25, 'F' }, // 554     Transaction failed
	{ 26, 'R' }, // 555     Cannot recognize or implement parameter (http://tools.ietf.org/html/rfc1869)
	/* Vertical bar will be here */
};

static const int NAMES_SIZE = sizeof(values) / sizeof(item_t);

// Get room for:
// - 4 separators
// - Spam flag
// - Unknown flag
// - null byte
#define FLAGSIZE (NAMES_SIZE + 7)

/***** FUNCTIONS *****/

static int is_sep_position(unsigned i)
{
	switch(i) {
		case 7:
		case 9:
		case 15:
		case 27:
			return 1;
		default:
			return 0;
	}
}

/**
 * Check for buffer's size
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
"Converts 'status code flags' field to more readable form\n \
Status codes present in the flow are represented by uppercase letters\n \
in their position within the field. The letters are grouped by the first digit\n \
of their status codes and are ordered by status code value.\n \
Not present status codes are represented by dash ('-')\n \
See 'man fbitdump-plugins' for thorough explanation\n";
}

/**
 * Fill the buffer with text representation of field's content
 */
void format(const plugin_arg_t * arg, int plain_numbers,
		char buffer[PLUGIN_BUFFER_SIZE], void *conf)
{
	// get rid of the warning
	if (plain_numbers) {;}

	int i;

	// Initializing the field
	for (i = 0; i < FLAGSIZE - 1; i++) {
		if (is_sep_position(i)) {
			buffer[i] = SEP_CHAR;
		} else {
			buffer[i] = DEFAULT_CHAR;
		}
	}
	buffer[FLAGSIZE - 1] = '\0';

	// Filling in flag values
	for (i = 0; i < NAMES_SIZE; i++) {
		if(arg->val->uint32 & (((uint32_t)1) << i)) {
			buffer[values[i].position] = values[i].flag;
		}
	}

	if (arg->val->uint32 & SC_UNKNOWN) {
		buffer[FLAGSIZE - 2] = 'U';
	}

	if (arg->val->uint32 & SC_SPAM) {
		buffer[FLAGSIZE - 3] = 'S';
	}
}

/**
 * Parse text data and return inner format
 */
void parse(char *input, char out[PLUGIN_BUFFER_SIZE], void *conf)
{
	int i;
	int offset = 0;
	uint32_t result = 0;

	for (i = 0; i < NAMES_SIZE; i++) {
		if (is_sep_position(i)) {
			offset++;
			continue;
		}

		if (input[i+offset] == values[i].flag) {
			result |= ((uint32_t)1) << i;
		}
	}

	if (input[FLAGSIZE - 2] == 'U') {
		result |= SC_UNKNOWN;
	}

	if (input[FLAGSIZE - 3] == 'S') {
		result |= SC_SPAM;
	}

	snprintf(out, PLUGIN_BUFFER_SIZE, "%u", result);
}
