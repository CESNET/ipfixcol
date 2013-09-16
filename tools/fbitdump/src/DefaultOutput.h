/* 
 * File:   defaultoutput.h
 * Author: fuf
 *
 * Created on 9. září 2013, 10:32
 */

#ifndef DEFAULTOUTPUT_H
#define	DEFAULTOUTPUT_H

#ifdef	__cplusplus
extern "C" {
#endif

#include "Configuration.h"
#include "Resolver.h"
#include "plugins/plugin_header.h"
#include <inttypes.h>
#include <arpa/inet.h>
#include <netdb.h>

using namespace fbitdump;

void printProtocol(const union plugin_arg * val, int plain_numbers, char * buff);
void printIPv4(const union plugin_arg * val, int plain_numbers, char * buff);

void printIPv6(const union plugin_arg * val, int plain_numbers, char * buff);

void printTimestamp32(const union plugin_arg * va, int plain_numbers, char * buff);
void printTimestamp64(const union plugin_arg * val, int plain_numbers, char * buff);

void printTimestamp(struct tm *tm, uint64_t msec, char * buff );

void printTCPFlags(const union plugin_arg * val, int plain_numbers, char * buff);

void printDuration(const union plugin_arg * val, int plain_numbers, char * buff);



#ifdef	__cplusplus
}
#endif

#endif	/* DEFAULTOUTPUT_H */

