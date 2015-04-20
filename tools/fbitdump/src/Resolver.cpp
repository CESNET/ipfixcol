/**
 * \file Resolver.cpp
 * \author Petr Velan <petr.velan@cesnet.cz>
 * \brief DNS resolver
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



#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <resolv.h>
#include <arpa/inet.h>
#include <stdexcept>

#include "typedefs.h"
#include "Resolver.h"

namespace fbitdump {

Resolver::Resolver(char *nameserver) throw (std::invalid_argument): configured(false)
{
	this->setNameserver(nameserver);
}

void Resolver::setNameserver(char *nameserver) throw (std::invalid_argument)
{
	if (nameserver == NULL) {
		throw std::invalid_argument("Cannot use empty nameserver");
	}

	int ret;
	struct addrinfo *result;
	struct addrinfo hints;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_protocol = 0;

	ret = getaddrinfo(nameserver, "domain", &hints, &result);
	if (ret != 0) {
		std::string err = std::string("Unable to resolve address '") + nameserver + "': " + gai_strerror(ret);
		throw std::invalid_argument(err);
	}

	res_init();

	if (result->ai_addr->sa_family == AF_INET) {
#ifdef DEBUG
		std::cerr << "Setting up IPv4 DNS server with address " << nameserver << std::endl;
#endif
		struct sockaddr_in *sock4 = (struct sockaddr_in *) result->ai_addr;
		memcpy((void *)&_res.nsaddr_list[0], sock4, result->ai_addrlen);
		_res.nscount = 1;
	} else if (result->ai_addr->sa_family == AF_INET6) {
		std::cerr << "IPv6 addresses are not supported for DNS server." << std::endl;
		/*
		struct sockaddr_in6 *sock6 = (struct sockaddr_in6 *) result->ai_addr;
		memcpy((void *)&_res.nsaddr_list[0], sock6, result->ai_addrlen);
		_res.nscount = 1;
		*/
	} else {
		std::string err = std::string("Unable to resolve address for ") + nameserver + ": " + "Unknown address family";
		throw std::invalid_argument(err);
	}

	freeaddrinfo(result);

	this->nameserver = nameserver;
}

const char *Resolver::getNameserver() const
{
	if (this->nameserver.empty()) {
		return NULL;
	}

	return this->nameserver.c_str();
}

bool Resolver::reverseLookup(uint32_t address, std::string &result)
{
	/* look into cache */
	std::map<uint32_t, std::string>::const_iterator it;
	if ((it = this->dnsCache.find(address)) != this->dnsCache.end()) {
		result = it->second;
		return true;
	}

	/* lookup the address */
	int ret;
	struct sockaddr_in sock;
	char host[NI_MAXHOST];

	memset(&sock, 0, sizeof(sock));
	sock.sin_family = AF_INET;
	sock.sin_addr.s_addr = htonl(address);

	ret = getnameinfo((const struct sockaddr *)&sock, sizeof(sock), host, NI_MAXHOST, NULL, 0, 0);
	if (ret != 0) {
		return false;
	}
	result = host;

	/* add the result to the cache */
	this->dnsCache[address] = result;

	return true;
}

bool Resolver::reverseLookup6(uint64_t in6_addr_part1, uint64_t in6_addr_part2, std::string &result)
{
	/* look into cache */
	std::map<uint64_t, std::map<uint64_t, std::string> >::const_iterator it;
	std::map<uint64_t, std::string>::const_iterator it2;
	if (((it = this->dnsCache6.find(in6_addr_part1)) != this->dnsCache6.end()) &&
			((it2 = it->second.find(in6_addr_part2)) != it->second.end())) {

		result = it2->second;
		return true;
	}

	/* lookup the address */
	int ret;
	struct sockaddr_in6 sock6;
	struct in6_addr *in6addr = &(sock6.sin6_addr);
	char host[NI_MAXHOST];

	memset(&sock6, 0, sizeof(sock6));
	sock6.sin6_family = AF_INET6;
	*((uint64_t *) in6addr->s6_addr) = htobe64(in6_addr_part1);
	*(((uint64_t *) in6addr->s6_addr)+1) = htobe64(in6_addr_part2);

	ret = getnameinfo((const struct sockaddr *)&sock6, sizeof(sock6), host, NI_MAXHOST, NULL, 0, 0);
	if (ret != 0) {
		return false;
	}
	result = host;

	/* add the result to the cache */
	this->dnsCache6[in6_addr_part1][in6_addr_part2] = result;

	return true;
}

Resolver::~Resolver()
{

}

} /* namespace fbitdump */
