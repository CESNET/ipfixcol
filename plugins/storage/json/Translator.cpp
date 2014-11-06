/* 
 * File:   Formatter.cpp
 * Author: michal
 * 
 * Created on 14. říjen 2014, 15:27
 */

#include "Translator.h"
#include "protocols.h"

#include <arpa/inet.h>
#include <vector>

#define BUFF_SIZE 128

Translator::Translator()
{
	buffer.reserve(BUFF_SIZE);
}

Translator::~Translator()
{
}


/**
 * \brief Format flags
 */
std::string Translator::formatFlags(uint16_t flags)
{
	std::string result = "......";
	flags = ntohs(flags);
	
	if (flags & 0x20) {
		result[0] = 'U';
	}
	if (flags & 0x10) {
		result[1] = 'A';
	}
	if (flags & 0x08) {
		result[2] = 'P';
	}
	if (flags & 0x04) {
		result[3] = 'R';
	}
	if (flags & 0x02) {
		result[4] = 'S';
	}
	if (flags & 0x01) {
		result[5] = 'F';
	}
	
	return result;
}

/**
 * \brief Format IPv6
 */
std::string Translator::formatIPv4(uint32_t addr)
{
	buffer.clear();
	
	inet_ntop(AF_INET, &addr, buffer.data(), INET_ADDRSTRLEN);
	return std::string(buffer.data());
}

/**
 * \brief Format IPv6
 */
std::string Translator::formatIPv6(uint8_t* addr)
{
	buffer.clear();
	
	inet_ntop(AF_INET6, (struct in6_addr *) addr, buffer.data(), INET6_ADDRSTRLEN);
	return std::string(buffer.data());
}

/**
 * \brief Format timestamp
 */
std::string Translator::formatMac(uint8_t* addr)
{
	buffer.clear();
	
	snprintf(buffer.data(), BUFF_SIZE, "%02x:%02x:%02x:%02x:%02x:%02x", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
	return std::string(buffer.data());
}

/**
 * \brief Format protocol
 */
std::string Translator::formatProtocol(uint8_t proto)
{
	return std::string(protocols[proto]);
}

/**
 * \brief Format timestamp
 */
std::string Translator::formatTimestamp(uint64_t tstamp, t_units units)
{
	buffer.clear();
	
	tstamp = be64toh(tstamp);
	
	/* Convert to milliseconds */
	switch (units) {
	case t_units::SEC:
		tstamp *= 1000;
		break;
	case t_units::MICROSEC:
		tstamp /= 1000;
		break;
	case t_units::NANOSEC:
		tstamp /= 1000000;
		break;
	default: /* MILLI is default */
		break;
	}
	
	time_t timesec = tstamp / 1000;
	uint64_t msec  = tstamp % 1000;
	struct tm *tm  = localtime(&timesec);
	
	strftime(buffer.data(), 20, "%FT%T", tm);
	/* append miliseconds */
	sprintf(&(buffer.data()[19]), ".%03u", (const unsigned int) msec);
	
	return std::string(buffer.data());
}
