/**
 * \file Resolver.cpp
 * \author Michal Srb <michal.srb@cesnet.cz>
 * \brief Class for managing user input of fbitdump
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

#include "Resolver.h"

namespace fbitdump {

Resolver::Resolver() : configured(false)
{
}

Resolver::Resolver(char *nameserver) : configured(false)
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

Resolver::~Resolver()
{
	/* nothing to do */
}

} /* namespace */
