/**
 * \file tls_version.c
 * \author Martin Bajanik <396204@mail.muni.cz>
 * \date 2014
 * \brief Fbitdump plugin to format and parse TLS versions.
 *
 * \copyright
 * Copyright (C) 2014 Masaryk University, Institute of Computer Science
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of the Masaryk University nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * This software is provided ``as is'', and any express or implied
 * warranties, including, but not limited to, the implied warranties of
 * merchantability and fitness for a particular purpose are disclaimed.
 * In no event shall the company or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even
 * if advised of the possibility of such damage.
 *
 * $Id$
 */
 
#define _GNU_SOURCE
#include <stdio.h>
#include <strings.h>
#include "plugin_header.h"
#include "tls_values.h"

char *info()
{
	return "Converts TLS Version number to human readable string.\n"
		"Printed values can be also passed to filter for parsing.";
}

/** Funcion to assign given values to strings */
void format(const plugin_arg_t *arg, int plain_numbers, char out[PLUGIN_BUFFER_SIZE], void *conf)
{
	char *str = NULL;
	char num[PLUGIN_BUFFER_SIZE];
	int counter = 0;
	
	while (version[counter].strptr) {
		if (version[counter].value == arg->val[0].uint16) {
			str = version[counter].strptr;
		}
		counter++;
	}
	
	if (str != NULL) {
		snprintf(out, PLUGIN_BUFFER_SIZE, "%s", str);
	} else {
		snprintf(num, sizeof(num), "%u", arg->val[0].uint16);
		str = num;
	}
}

/** Function to parse given strings */
void parse(char *input, char out[PLUGIN_BUFFER_SIZE], void *conf)
{
	int code;
	if (!strcasecmp(input, "SSLv3.0")) {
		code = 0x0300;
	} else if (!strcasecmp(input, "TLSv1.0")) {
		code = 0x0301;
	} else if (!strcasecmp(input, "TLSv1.1")) {
		code = 0x0302;
	} else if (!strcasecmp(input, "TLSv1.2")) {
		code = 0x0303;
	} else if (!strcasecmp(input, "SSLv2.0")) {
		code = 0x0002;
	} else if (!strcasecmp(input, "SSLv2.0 (SSLv3.0)")) {
		code = 0x2300;
	} else if (!strcasecmp(input, "SSLv2.0 (TLSv1.0)")) {
		code = 0x2301;
	} else if (!strcasecmp(input, "SSLv2.0 (TLSv1.1)")) {
		code = 0x2302;
	} else if (!strcasecmp(input, "SSLv2.0 (TLSv1.2)")) {
		code = 0x2303;
	} else if (!strcasecmp(input, "SKYPE")) {
		code = 0x0005;
	} else {
		snprintf(out, PLUGIN_BUFFER_SIZE, "%s", "");
		return;
	}

	snprintf(out, PLUGIN_BUFFER_SIZE, "0x%x", code);
}
