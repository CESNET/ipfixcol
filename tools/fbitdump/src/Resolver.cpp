/**
 * \file Resolver.cpp
 * \author Michal Srb <michal.srb@cesnet.cz>
 * \brief DNS resolver
 *
 * Copyright (C) 2011 CESNET, z.s.p.o.
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


#include "Resolver.h"

namespace fbitdump {

Resolver::Resolver() : configured(false), cacheOn(false)
{
}

Resolver::Resolver(char *nameserver) : configured(false), cacheOn(false)
{
	this->setNameserver(nameserver);
}

int Resolver::setNameserver(char *nameserver)
{
	if (nameserver == NULL) {
		return false;
	}

	int ret;
	struct addrinfo *result;
	struct addrinfo hints;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_protocol = SOCK_STREAM;

	ret = getaddrinfo(nameserver, 0, &hints, &result);
	if (ret != 0) {
		std::cerr << "Unable to resolve address for " << nameserver << std::endl;
		return -1;
	}

	res_init();

	struct sockaddr_in *sock4 = NULL;
	struct sockaddr_in6 *sock6 = NULL;
	if (result->ai_addr->sa_family == AF_INET) {
		sock4 = (struct sockaddr_in *) result->ai_addr;
		memcpy((void *)&_res.nsaddr_list[0].sin_addr, &(sock4->sin_addr), result->ai_addrlen);
		_res.nscount = 1;
	} else if (result->ai_addr->sa_family == AF_INET6) {
		sock6 = (struct sockaddr_in6 *) result->ai_addr;
		memcpy((void *)&_res.nsaddr_list[0].sin_addr, &(sock6->sin6_addr), result->ai_addrlen);
		_res.nscount = 1;
	} else {
		std::cerr << "error: unknown address family" << std::endl;
	}

	freeaddrinfo(result);

	this->nameserver = nameserver;
	this->configured = true;

	return 0;
}

const char *Resolver::getNameserver()
{
	if (this->nameserver.empty()) {
		return NULL;
	}

	return this->nameserver.c_str();
}

bool Resolver::isConfigured()
{
	return this->configured;
}

bool Resolver::reverseLookup(uint32_t address, char *result, int len)
{
	if (!this->isConfigured()) {
		/* configure the resolver first */
#ifdef DEBUG
		std::cerr << "DNS resolver is not configured yet" << std::endl;
#endif

		return false;
	}

	char buf[INET_ADDRSTRLEN];
	//memset(buf, 0, INET_ADDRSTRLEN);

	/* try cache first */
	if (this->cacheOn) {
		char *cacheResult;
		struct in_addr in_addr;

		in_addr.s_addr = htonl(address);
		inet_ntop(AF_INET, &in_addr, buf, INET_ADDRSTRLEN);

		cacheResult = (char *) this->cacheSearch(buf, AF_INET);
		if (cacheResult) {
			/* cache hit */
			strncpy(result, cacheResult, len);

			return true;
		}
	}

	int ret;
	struct sockaddr_in sock;

	memset(&sock, 0, sizeof(sock));
	sock.sin_family = AF_INET;
	sock.sin_addr.s_addr = htonl(address);

	ret = getnameinfo((const struct sockaddr *)&sock, sizeof(sock), result, len, NULL, 0, 0);
	if (ret != 0) {
		return false;
	}

	/* add this result to the cache, if enabled */
	if (this->cacheOn) {
		char *persistentResult = (char *) malloc(strlen(result)+1);
		if (!persistentResult) {
			std::cerr << "malloc() error (" << __FILE__ << ":" << __LINE__ << ")" << std::endl;
			return false;
		}

		strcpy(persistentResult, result);
		this->addToCache(buf, persistentResult, AF_INET);
	}

	return true;
}

bool Resolver::reverseLookup6(uint64_t in6_addr_part1, uint64_t in6_addr_part2, char *result, int len)
{
	if (!this->isConfigured()) {
		/* configure the resolver first */
#ifdef DEBUG
		std::cerr << "DNS resolver is not configured yet" << std::endl;
#endif

		return false;
	}

	char buf[INET6_ADDRSTRLEN];
	//memset(buf, 0, INET6_ADDRSTRLEN);

	/* try cache first */
	if (this->cacheOn) {
		struct in6_addr in6_addr;
		char *cacheResult;

		*((uint64_t*) &in6_addr.s6_addr) = htobe64(in6_addr_part1);
		*(((uint64_t*) &in6_addr.s6_addr)+1) = htobe64(in6_addr_part2);
		inet_ntop(AF_INET6, &in6_addr, buf, INET6_ADDRSTRLEN);

		cacheResult = (char *) this->cacheSearch(buf, AF_INET6);
		if (cacheResult) {
			/* cache hit */
			strncpy(result, cacheResult, len);
			return true;
		}
	}

	int ret;
	struct sockaddr_in6 sock6;
	struct in6_addr *in6addr = &(sock6.sin6_addr);

	memset(&sock6, 0, sizeof(sock6));
	sock6.sin6_family = AF_INET6;
	*((uint64_t *) in6addr->s6_addr) = htobe64(in6_addr_part1);
	*(((uint64_t *) in6addr->s6_addr)+1) = htobe64(in6_addr_part2);

	ret = getnameinfo((const struct sockaddr *)&sock6, sizeof(sock6), result, len, NULL, 0, 0);
	if (ret != 0) {
		return false;
	}

	/* add this result to the cache, if enabled */
	if (this->cacheOn) {
		char *persistentResult = (char *) malloc(strlen(result)+1);
		if (!persistentResult) {
			std::cerr << "malloc() error (" << __FILE__ << ":" << __LINE__ << ")" << std::endl;
			return false;
		}

		strcpy(persistentResult, result);
		this->addToCache(buf, persistentResult, AF_INET6);
	}

	return true;
}

void Resolver::enableCache()
{
	int ret;

	/* these structures must be zeroed first */
	memset(&(this->ipv4Htab), 0, sizeof(struct hsearch_data));
	memset(&(this->ipv4Htab), 0, sizeof(struct hsearch_data));

	ret = hcreate_r(Resolver::ipv4CacheSize, &(this->ipv4Htab));
	if (ret == 0) {
		std::cerr << "Error, DNS cache disabled" << std::endl;
		return;
	}

	ret = hcreate_r(Resolver::ipv6CacheSize, &(this->ipv6Htab));
	if (ret == 0) {
		std::cerr << "Error, DNS cache disabled" << std::endl;
		hdestroy_r(&(this->ipv4Htab));
		return;
	}

	this->cacheOn = true;
}

void Resolver::disableCache()
{
	if (!this->cacheEnabled()) {
		return;
	}

	hdestroy_r(&(this->ipv4Htab));
	hdestroy_r(&(this->ipv6Htab));

	this->cacheOn = false;
}

bool Resolver::cacheEnabled()
{
	return this->cacheOn;
}

bool Resolver::addToCache(char *key, void *data, int af)
{
	ENTRY e;
	int ret;
	ENTRY *retval;

	if (!key || !data) {
		/* invalid input */
		return false;
	}

	e.key = key;
	e.data = data;

	switch (af) {
	case (AF_INET):
		ret = hsearch_r(e, ENTER, &retval, &(this->ipv4Htab));
		if (ret == 0) {
			/* adding failed */
			std::cerr << "Unable to add entry to the cache" << std::endl;
			return false;
		}
		break;

	case (AF_INET6):
		ret = hsearch_r(e, ENTER, &retval, &(this->ipv6Htab));
		if (ret == 0) {
			/* adding failed */
			return false;
		}
		break;

	default:
		return false;
	}

	return true;
}

void *Resolver::cacheSearch(char *key, int af)
{
	ENTRY e;
	ENTRY *ep;
	int ret;

	if (!key) {
		return NULL;
	}

	e.key = key;

	switch (af) {
	case (AF_INET):
		ret = hsearch_r(e, FIND, &ep, &(this->ipv4Htab));
		if (ret == 0) {
			return NULL;
		}
		break;

	case (AF_INET6):
		ret = hsearch_r(e, FIND, &ep, &(this->ipv6Htab));
		if (ret == 0) {
			return NULL;
		}
		break;

	default:
		return NULL;
	}

	return ep->data;
}

Resolver::~Resolver()
{
	if (this->cacheEnabled()) {
		this->disableCache();
	}
}

} /* namespace fbitdump */
