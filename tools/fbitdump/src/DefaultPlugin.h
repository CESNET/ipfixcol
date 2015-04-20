/**
 * \file DefaultPlugin.h
 * \author Michal Kozubik <kozubik.michal@google.com>
 * \brief Default plugin for parsing input filter
 *
 * Copyright (C) 2015 CESNET, z.s.p.o.
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
 * 3. Neither the name of the Company nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * ALTERNATIVELY, provided that this notice is retained in full, this
 * product may be distributed under the terms of the GNU General Public
 * License (GPL) version 2 or later, in which case the provisions
 * of the GPL apply INSTEAD OF those given above.
 *
 * This software is provided ``as is, and any express or implied
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
 */

#ifndef DEFAULTPLUGIN_H_
#define DEFAULTPLUGIN_H_

#include "Filter.h"

using namespace fbitdump;

/* input parsing */
void parseFlags(char *strFlags, char *out, void *conf);
void parseProto(char *strProto, char *out, void *conf);
void parseHostname(parserStruct *ps, uint8_t af_type);
void parseDuration(char *duration, char *out, void *conf);

/* output formatting */
void printProtocol(const plugin_arg_t * val, int plain_numbers, char * buff, void *conf);
void printIPv4(const plugin_arg_t * val, int plain_numbers, char * buff, void *conf);

void printIPv6(const plugin_arg_t * val, int plain_numbers, char * buff, void *conf);

void printTimestamp32(const plugin_arg_t * va, int plain_numbers, char * buff, void *conf);
void printTimestamp64(const plugin_arg_t * val, int plain_numbers, char * buff, void *conf);

void printTimestamp(struct tm *tm, uint64_t msec, char * buff );

void printTCPFlags(const plugin_arg_t * val, int plain_numbers, char * buff, void *conf);

void printDuration(const plugin_arg_t * val, int plain_numbers, char * buff, void *conf);



#endif /* DEFAULTPLUGIN_H_ */
