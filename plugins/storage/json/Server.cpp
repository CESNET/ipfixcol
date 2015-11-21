/**
 * \file Server.cpp
 * \author Lukas Hutak <xhutak01@stud.fit.vutbr.cz>
 * \brief Server output
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

#include "Server.h"
#include <stdexcept>
#include <cstring>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>
#include <arpa/inet.h>

// Default server port
#define DEFAULT_PORT (4800)
// How many pending connections queue will hold
#define BACKLOG (10)

// Name of plugin
static const char *msg_module = "json_storage(server)";

/**
 * \brief Class constructor
 *
 * Parse configuration, create and bind server's socket and create acceptor's
 * thread
 */
Server::Server(const pugi::xpath_node &config)
{
	_non_blocking = false;
	_acceptor = NULL;

	// Load and check the configuration
	std::string port  = config.node().child_value("port");
	std::string blocking = config.node().child_value("blocking");

	// Check the server configuration
	if (port.empty()) {
		throw std::invalid_argument("Invalid source port specification.");
	}

	if (blocking == "yes" || blocking == "true" || blocking == "1") {
		_non_blocking = false;
	} else if (blocking == "no" || blocking == "false" || blocking == "0") {
		_non_blocking = true;
	} else {
		throw std::invalid_argument("Invalid blocking mode specification.");
	}

	int serv_fd;
	int ret_val;

	// New socket configuration
	struct addrinfo hints;
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;       // Use IPv4
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;     // Wildcard IP address
	hints.ai_protocol = IPPROTO_TCP; // TCP

	// Create new socket
	struct addrinfo *servinfo, *iter;
	if ((ret_val = getaddrinfo(NULL, port.c_str(), &hints, &servinfo)) != 0) {
		throw std::runtime_error("Server initialization failed (" +
			std::string(gai_strerror(ret_val)) + ")");
	}

	for(iter = servinfo; iter != NULL; iter = iter->ai_next) {
		serv_fd = socket(iter->ai_family, iter->ai_socktype, iter->ai_protocol);
		if ((serv_fd) == -1) {
			continue;
		}

		int yes = 1;
		if (setsockopt(serv_fd, SOL_SOCKET, SO_REUSEADDR, &yes,
				sizeof(int)) == -1) {
			continue;
		}

		if (bind(serv_fd, iter->ai_addr, iter->ai_addrlen) == -1) {
			// Failed to bind
			close(serv_fd);
			continue;
		}

		// Success
		break;
	}

	freeaddrinfo(servinfo);

	// Check if new socket is ready
	if (iter == NULL)  {
		throw std::runtime_error("Server failed to bind to specified port.");
	}

	// Make socket passive
	if (listen(serv_fd, BACKLOG) == -1) {
		close(serv_fd);
		throw std::runtime_error("Server initialization failed (" +
			std::string(strerror(errno)) + ")");
	}

	// Create thread
	_acceptor = new acceptor_t;
	_acceptor->socket_fd = serv_fd;
	_acceptor->new_clients_ready = false;
	_acceptor->stop = false;

	if (pthread_mutex_init(&_acceptor->mutex, NULL) != 0) {
		delete _acceptor;
		close(serv_fd);
		throw std::runtime_error("Mutex initialization failed");
	}

	if (pthread_create(&_acceptor->thread, NULL, &Server::thread_accept,
			_acceptor) != 0) {
		delete _acceptor;
		close(serv_fd);
		throw std::runtime_error("Acceptor thread failed");
	}
}

/**
 * \brief Class destructor
 *
 * Close all sockets and stop and destroy the acceptor.
 */
Server::~Server()
{
	// Disconnect connected clients
	for (auto &client : _clients) {
		close(client.socket);
	}

	// Stop and destroy acceptor's thread
	if (_acceptor) {
		_acceptor->stop = true;
		pthread_join(_acceptor->thread, NULL);
		pthread_mutex_destroy(&_acceptor->mutex);

		close(_acceptor->socket_fd);

		for (auto &client : _acceptor->new_clients) {
			close(client.socket);
		}

		delete _acceptor;
	}
}

/**
 * \brief Acceptor's thread function
 *
 * Wait for new clients and accept them.
 * \param[in,out] context Acceptor's structure with configured server socket
 * \return Nothing
 */
void *Server::thread_accept(void *context)
{
	acceptor_t *acc = (acceptor_t *) context;
	int ret_val;
	struct timeval tv;
	fd_set rfds;

	MSG_INFO(msg_module, "Waiting for connections...");

	while(1) {
		struct sockaddr_storage client_addr;
		socklen_t sin_size = sizeof(client_addr);
		int new_fd;

		// "select()" configuration
		FD_ZERO(&rfds);
		FD_SET(acc->socket_fd, &rfds);
		tv.tv_sec = 0;
		tv.tv_usec = 100000;

		ret_val = select(acc->socket_fd + 1, &rfds, NULL, NULL, &tv);
		if (ret_val == -1) {
			MSG_ERROR(msg_module, "select() - failed (%s)", strerror(errno));
			break;
		}

		if (!FD_ISSET(acc->socket_fd, &rfds)) {
			// Timeout
			if (acc->stop) {
				// End thread
				break;
			}
			continue;
		}

		new_fd = accept(acc->socket_fd, (struct sockaddr *) &client_addr,
			&sin_size);
		if (new_fd == -1) {
			MSG_ERROR(msg_module, "accept() - failed (%s)", strerror(errno));
			continue;
		}

		MSG_INFO(msg_module, "Client connected: %s",
			get_client_desc(client_addr).c_str());

		// Further receptions from the socket will be disallowed
		shutdown(new_fd, SHUT_RD);

		// Add new client to the array of new clients
		pthread_mutex_lock(&acc->mutex);
		client_t new_client {client_addr, new_fd};
		acc->new_clients.push_back(new_client);
		acc->new_clients_ready = true;
		pthread_mutex_unlock(&acc->mutex);
	}

	MSG_INFO(msg_module, "Connection acceptor terminated.");
	return NULL;
}

/**
 * \brief Send record to all connected clients
 *
 * \param[in] record Record
 */
void Server::ProcessDataRecord(const std::string &record)
{
	const char *data = record.c_str();
	size_t length = strlen(data) + 1;
	int flags = MSG_NOSIGNAL;
	if (_non_blocking) {
		flags |= MSG_DONTWAIT;
	}

	// Are there new clients?
	if (_acceptor->new_clients_ready) {
		pthread_mutex_lock(&_acceptor->mutex);

		_clients.insert(_clients.end(), _acceptor->new_clients.begin(),
			_acceptor->new_clients.end());
		_acceptor->new_clients.clear();

		_acceptor->new_clients_ready = false;
		pthread_mutex_unlock(&_acceptor->mutex);
	}

	// Send the message to all clients
	std::vector<client_t>::iterator iter = _clients.begin();
	while (iter != _clients.end()) {
		ssize_t todo = length;
		const char *ptr = data;
		bool remove_client = false;

		while (todo > 0) {
			ssize_t now = send(iter->socket, ptr, todo, flags);

			if (now == -1) {
				if (_non_blocking && (errno == EAGAIN || errno == EWOULDBLOCK)) {
					// Non blocking enabled - skip message
					// TODO: add warning?
					break;
				}

				// Other situations - close connection
				MSG_INFO(msg_module, "Client disconnected: %s (%s)",
					get_client_desc(iter->info).c_str(), strerror(errno));
				remove_client = true;
				break;
			}

			ptr  += now;
			todo -= now;
		}

		if (remove_client) {
			// Close socket and remove client's info
			close(iter->socket);
			_clients.erase(iter); // The iterator has new location...
		} else {
			// New client
			++iter;
		}
	}
}

/**
 * \brief Get a brief description about connected client
 * \param[in] client Client network info
 * \return String with client's IP and port
 */
std::string Server::get_client_desc(const struct sockaddr_storage &client)
{
	char ip_str[INET6_ADDRSTRLEN] = {0};
	uint16_t port;

	switch (client.ss_family) {
	case AF_INET: {
		// IPv4
		const struct sockaddr_in *src_ip = (struct sockaddr_in *) &client;
		inet_ntop(client.ss_family, &src_ip->sin_addr, ip_str, sizeof(ip_str));
		port = ntohs(src_ip->sin_port);

		return std::string(ip_str) + ":" + std::to_string(port);
		}
	case AF_INET6: {
		// IPv6
		const struct sockaddr_in6 *src_ip = (struct sockaddr_in6 *) &client;
		inet_ntop(client.ss_family, &src_ip->sin6_addr, ip_str, sizeof(ip_str));
		port = ntohs(src_ip->sin6_port);

		return std::string(ip_str) + ":" + std::to_string(port);
		}
	default:
		return "Unknown";
	}
}
