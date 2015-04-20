/**
 * \file Resolver.h
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


#ifndef RESOLVER_H_
#define RESOLVER_H_

#include <iostream>
#include <cstring>
#include <map>
#include <stdexcept>

namespace fbitdump {

/**
 * \brief Class for DNS lookups
 *
 * Uses given IPv4 nameserver to resolve addresses to hostnames
 * The adresses are currently cached in std::map structure
 */
class Resolver {
public:
	Resolver(char *nameserver) throw (std::invalid_argument);
	~Resolver();

    /**
     * \brief Returns resolver
     *
     * @return object which provides DNS resolving functionality
     */
	const char *getNameserver() const;

	/**
	 * \brief reverse DNS lookup for IPv4 address
	 *
	 * @param[in] inaddr numeric presentation of IPv4 address
	 * @param[out] result contains domain name corresponding to the IP address
	 * @return true on success, false otherwise
	 */
	bool reverseLookup(uint32_t inaddr, std::string &result);

	/**
	 * \brief reverse DNS lookup for IPv6 address
	 *
	 * @param[in] inaddr_part1 numeric presentation of IPv6 address, first part of the address
	 * @param[in] inaddr_part2 numeric presentation of IPv6 address, second part of the address
	 * @param[out] result contains domain name corresponding to the IP address
	 * @return true on success, false otherwise
	 */
	bool reverseLookup6(uint64_t inaddr_part1, uint64_t inaddr_part2, std::string &result);

private:
	std::string nameserver;
	bool configured;

	std::map<uint32_t, std::string> dnsCache;
	std::map<uint64_t, std::map<uint64_t, std::string> > dnsCache6;

    /**
     * \brief Initialise resolver to use nameserver
     *
     * Sets configured flag if completed without error
     * Works only for IPv4 nameservers, it is difficult to enforce own IPv6 nameserver
     *
     * @param nameserver Nameserver address to be used
     */
	void setNameserver(char *nameserver) throw (std::invalid_argument);

};

} /* namespace fbitdump */

#endif
