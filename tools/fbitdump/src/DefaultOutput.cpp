#include <inttypes.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "Configuration.h"
#include "Resolver.h"
#include "plugins/plugin_header.h"
#include "DefaultOutput.h"


using namespace fbitdump;

void printProtocol(const union plugin_arg * val, int plain_numbers, char ret[PLUGIN_BUFFER_SIZE]) {
	if (!plain_numbers) {
		snprintf( ret, PLUGIN_BUFFER_SIZE, "%s", protocols[val[0].uint8] );
	} else {
		snprintf( ret, PLUGIN_BUFFER_SIZE, "%d", (uint16_t)val[0].uint8 );
	}
}

void printIPv4(const union plugin_arg * val, int plain_numbers, char buf[PLUGIN_BUFFER_SIZE])
{
	int ret;
	Resolver *resolver;

	resolver = Configuration::instance->getResolver();

	/* translate IP address to domain name, if user wishes so */
	if (resolver != NULL) {
		std::string host;

		ret = resolver->reverseLookup(val[0].uint32, host);
		if (ret == true) {
			snprintf( buf, PLUGIN_BUFFER_SIZE, "%s", host.c_str() );
			return;
		}

		/* Error during DNS lookup, print IP address instead */
	}

	/*
	 * user don't want to see domain names, or DNS is somehow broken.
	 * print just IP address
	 */
	struct in_addr in_addr;

	in_addr.s_addr = htonl(val[0].uint32);
	inet_ntop(AF_INET, &in_addr, buf, INET_ADDRSTRLEN);
}

void printIPv6(const union plugin_arg * val, int plain_numbers, char buf[PLUGIN_BUFFER_SIZE])
{
	int ret;
	Resolver *resolver;

	resolver = Configuration::instance->getResolver();

	/* translate IP address to domain name, if user wishes so */
	if (resolver != NULL) {
		std::string host;

		ret = resolver->reverseLookup6(val[0].uint64, val[1].uint64, host);
		if (ret == true) {
			snprintf( buf, PLUGIN_BUFFER_SIZE, "%s", host.c_str() );
		}

		/* Error during DNS lookup, print IP address instead */
	}

	/*
	 * user don't want to see domain names, or DNS is somehow broken.
	 * print just IP address
	 */
	struct in6_addr in6_addr;

	*((uint64_t*) &in6_addr.s6_addr) = htobe64(val[0].uint64);
	*(((uint64_t*) &in6_addr.s6_addr)+1) = htobe64(val[1].uint64);
	inet_ntop(AF_INET6, &in6_addr, buf, INET6_ADDRSTRLEN);
}

void printTimestamp32(const union plugin_arg * val, int plain_numbers, char buf[PLUGIN_BUFFER_SIZE])
{
	time_t timesec = val[0].uint32;
	struct tm *tm = localtime(&timesec);

	printTimestamp(tm, 0, buf);
}

void printTimestamp64(const union plugin_arg * val, int plain_numbers, char buf[PLUGIN_BUFFER_SIZE])
{
	time_t timesec = val[0].uint64/1000;
	uint64_t msec = val[0].uint64 % 1000;
	struct tm *tm = localtime(&timesec);

	printTimestamp(tm, msec, buf);
}

void printTimestamp(struct tm *tm, uint64_t msec, char buff[PLUGIN_BUFFER_SIZE])
{

	strftime(buff, 20, "%Y-%m-%d %H:%M:%S", tm);
	/* append miliseconds */
	sprintf(&buff[19], ".%03u", (const unsigned int) msec);
}

void printTCPFlags(const union plugin_arg * val, int plain_numbers, char result[PLUGIN_BUFFER_SIZE])
{
	sprintf( result, "%s", "......" );

	if (val[0].uint8 & 0x20) {
		result[0] = 'U';
	}
	if (val[0].uint8 & 0x10) {
		result[1] = 'A';
	}
	if (val[0].uint8 & 0x08) {
		result[2] = 'P';
	}
	if (val[0].uint8 & 0x04) {
		result[3] = 'R';
	}
	if (val[0].uint8 & 0x02) {
		result[4] = 'S';
	}
	if (val[0].uint8 & 0x01) {
		result[5] = 'F';
	}
}

void printDuration(const union plugin_arg * val, int plain_numbers, char buff[PLUGIN_BUFFER_SIZE])
{
	static std::ostringstream ss;
	static std::string str;
	ss << std::fixed;
	ss.precision(3);

	ss << (float) val[0].uint64/1000;

	str = ss.str();
	ss.str("");

	snprintf( buff, PLUGIN_BUFFER_SIZE, "%s", str.c_str() );
}
