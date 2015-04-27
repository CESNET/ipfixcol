/*
 * File:    voip_rtpcodec.c
 * Author:  Jan Remes (xremes00@stud.fit.vutbr.cz)
 * Project: ipfixcol
 *
 * Description: This file contains fbitdump plugin for displaying RTP payload
 *              (VOIP_RTP_CODEC) field in text format
 *
 * Copyright (C) 2015 CESNET
 */

/**** INCLUDES and DEFINES *****/
#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>

#include "plugin_header.h"

#define NAME_SIZE 8

/***** TYPEDEFs and DATA *****/

typedef struct {
	unsigned code;
	char name[NAME_SIZE];
} item_t;

static const item_t values[] = {
	{  0,   "PCMU"  },
	{  3,   "GSM"   },
	{  4,   "G723"  },
	{  5,   "DVI4"  },
	{  6,   "DVI4"  },
	{  7,   "LPC"   },
	{  8,   "PCMA"  },
	{  9,   "G722"  },
	{ 10,   "L16"   },
	{ 11,   "L16"   },
	{ 12,   "QCELP" },
	{ 13,   "CN"    },
	{ 14,   "MPA"   },
	{ 15,   "G728"  },
	{ 16,   "DVI4"  },
	{ 17,   "DVI4"  },
	{ 18,   "G729"  },
	{ 25,   "CelB"  },
	{ 26,   "JPEG"  },
	{ 28,   "nv"    },
	{ 31,   "H261"  },
	{ 32,   "MPV"   },
	{ 33,   "MP2T"  },
	{ 34,   "H263"  }
};

/**
 * The highest number of 'Reserved'
 * @see http://www.iana.org/assignments/rtp-parameters/rtp-parameters.xhtml
 */
#define CODEC_RESERVED_MAX 19
#define CODEC_UNASSIGNED_MAX 95
#define CODEC_RTCP_MIN 72
#define CODEC_RTCP_MAX 76

static const int NAMES_SIZE = sizeof(values) / sizeof(item_t);

/***** FUNCTIONS *****/

char *info()
{
	return \
"Converts RTP codec number into its name. Strings 'Reserved', 'Unassigned' and\n \
'dynamic' are printed for corresponding numbers. They cannot be parsed back\n \
into the numerical representation because of their numerical value ambiguosity\n";
}

/**
 * Fill the buffer with text representation of field's content
 */
void format( const plugin_arg_t * arg, int plain_numbers,
		char buffer[PLUGIN_BUFFER_SIZE], void *conf)
{
	// get rid of the 'unused parameter' warning
	if (plain_numbers) {;}

	int i;

	for (i = 0; i < NAMES_SIZE; i++) {
		if (arg->val->uint8 == values[i].code) {
			snprintf(buffer, PLUGIN_BUFFER_SIZE, "%s", values[i].name);
			return;
		}
	}

	if (arg->val->uint8 >= CODEC_RTCP_MIN && arg->val->uint8 <= CODEC_RTCP_MAX) {
		snprintf(buffer, PLUGIN_BUFFER_SIZE, "Reserved");
		return;
	}

	if(arg->val->uint8 <= CODEC_RESERVED_MAX) {
		snprintf(buffer, PLUGIN_BUFFER_SIZE, "Reserved");
		return;
	} else if(arg->val->uint8 <= CODEC_UNASSIGNED_MAX) {
		snprintf(buffer, PLUGIN_BUFFER_SIZE, "Unassigned");
		return;
	} else {
		snprintf(buffer, PLUGIN_BUFFER_SIZE, "dynamic");
		return;
	}
}

/**
 * Parse text data and return inner format
 *
 * Unassigned and Reserved values are NOT parsed, as their value is ambiguous
 */
void parse(char *input, char out[PLUGIN_BUFFER_SIZE], void *conf)
{
	int i;

	for (i = 0; i < NAMES_SIZE; i++) {
		if (!strcasecmp(input, values[i].name)) {
			snprintf(out, PLUGIN_BUFFER_SIZE, "%u", values[i].code);
			return;
		}
	}

	snprintf(out, PLUGIN_BUFFER_SIZE, "%s", "");
}
