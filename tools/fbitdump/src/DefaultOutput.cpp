#include <inttypes.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "Configuration.h"
#include "Resolver.h"
#include "plugins/plugin_header.h"
#include "DefaultOutput.h"


using namespace fbitdump;

char * printProtocol(const union plugin_arg * val, int plain_numbers) {
	char * ret;

	if (!Configuration::instance->getPlainNumbers()) {
		asprintf( &ret, "%s", protocols[val[0].uint8] );
	} else {
		asprintf( &ret, "%d", (uint16_t)val[0].uint8 );
	}

	return ret;
}

char * printIPv4(const union plugin_arg * val, int plain_numbers)
{
	int ret;
	Resolver *resolver;

	resolver = Configuration::instance->getResolver();

	/* translate IP address to domain name, if user wishes so */
	if (resolver != NULL) {
		std::string host;

		ret = resolver->reverseLookup(val[0].uint32, host);
		if (ret == true) {
			return strdup( host.c_str() );
		}

		/* Error during DNS lookup, print IP address instead */
	}

	/*
	 * user don't want to see domain names, or DNS is somehow broken.
	 * print just IP address
	 */
	char * buf = (char *)malloc( sizeof( char ) * INET_ADDRSTRLEN );
	struct in_addr in_addr;

	in_addr.s_addr = htonl(val[0].uint32);
	inet_ntop(AF_INET, &in_addr, buf, INET_ADDRSTRLEN);

	/* IP address in printable form */
	return buf;
}

char * printIPv6(const union plugin_arg * val, int plain_numbers)
{
	int ret;
	Resolver *resolver;

	resolver = Configuration::instance->getResolver();

	/* translate IP address to domain name, if user wishes so */
	if (resolver != NULL) {
		std::string host;

		ret = resolver->reverseLookup6(val[0].uint64, val[1].uint64, host);
		if (ret == true) {
			return strdup( host.c_str() );
		}

		/* Error during DNS lookup, print IP address instead */
	}

	/*
	 * user don't want to see domain names, or DNS is somehow broken.
	 * print just IP address
	 */
	char * buf = (char *)malloc( sizeof( char ) * INET6_ADDRSTRLEN );
	struct in6_addr in6_addr;

	*((uint64_t*) &in6_addr.s6_addr) = htobe64(val[0].uint64);
	*(((uint64_t*) &in6_addr.s6_addr)+1) = htobe64(val[1].uint64);
	inet_ntop(AF_INET6, &in6_addr, buf, INET6_ADDRSTRLEN);

	return buf;
}

char * printTimestamp32(const union plugin_arg * val, int plain_numbers)
{
	time_t timesec = val[0].uint32;
	struct tm *tm = localtime(&timesec);

	return printTimestamp(tm, 0);
}

char * printTimestamp64(const union plugin_arg * val, int plain_numbers)
{
	time_t timesec = val[0].uint64/1000;
	uint64_t msec = val[0].uint64 % 1000;
	struct tm *tm = localtime(&timesec);

	return printTimestamp(tm, msec);
}

char * printTimestamp(struct tm *tm, uint64_t msec)
{
	char buff[23];

	strftime(buff, sizeof(buff), "%Y-%m-%d %T", tm);
	/* append miliseconds */
	sprintf(&buff[19], ".%03u", (const unsigned int) msec);

	return strdup(buff);
}

char * printTCPFlags(const union plugin_arg * val, int plain_numbers)
{
	std::string result = "......";

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

	return strdup( result.c_str() );
}

char * printDuration(const union plugin_arg * val, int plain_numbers)
{
	static std::ostringstream ss;
	static std::string str;
	ss << std::fixed;
	ss.precision(3);

	ss << (float) val[0].uint64/1000;

	str = ss.str();
	ss.str("");

	return strdup( str.c_str() );
}
