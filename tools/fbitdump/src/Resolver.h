/**
 * \file Resolver.h
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


#ifndef RESOLVER_H_
#define RESOLVER_H_

#include <iostream>
#include <cstring>
#include <search.h>

namespace fbitdump {

/**
 * \brief Class for DNS lookups
 */
class Resolver {
private:
	std::string nameserver;
	bool configured;
	bool cacheOn;         /* indicates whether caching is on or off */

	/* associative arrays that represent cache */
	struct hsearch_data ipv4Htab;
	struct hsearch_data ipv6Htab;

	/* cache for 300+300 IPv4+IPv6 entries */
	static const unsigned long int ipv4CacheSize = 300;
	static const unsigned long int ipv6CacheSize = 300;


public:
	Resolver();
	Resolver(char *nameserver);
	~Resolver();

    /**
     * \brief Returns resolver
     *
     * @return object which provides DNS resolving functionality
     */
	int setNameserver(char *nameserver);

    /**
     * \brief Returns resolver
     *
     * @return object which provides DNS resolving functionality
     */
	const char *getNameserver();

    /**
     * \brief Returns resolver
     *
     * @return true if DNS is configured, false otherwise
     */
	bool isConfigured();

	/**
	 * \brief reverse DNS lookup for IPv4 address
	 *
	 * @param[in] inaddr numeric presentation of IPv4 address
	 * @param[out] result contains domain name corresponding to the IP address
	 * @return true on success, false otherwise
	 */
	bool reverseLookup(uint32_t inaddr, char *result, int len);

	/**
	 * \brief reverse DNS lookup for IPv6 address
	 *
	 * @param[in] inaddr_part1 numeric presentation of IPv6 address, first part of the address
	 * @param[in] inaddr_part2 numeric presentation of IPv6 address, second part of the address
	 * @param[out] result contains domain name corresponding to the IP address
	 * @return true on success, false otherwise
	 */
	bool reverseLookup6(uint64_t inaddr_part1, uint64_t inaddr_part2, char *result, int len);

	void enableCache();

	void disableCache();

	bool cacheEnabled();

	bool addToCache(char *key, void *data, int af);

	void *cacheSearch(char *key, int af);
};

} /* namespace fbitdump */

#endif
