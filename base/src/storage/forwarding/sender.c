/**
 * \file storage/forwarding/sender.c
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Connection to a remote host (source file)
 *
 * Copyright (C) 2016 CESNET, z.s.p.o.
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

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
// Network API
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
// IPFIXcol API
#include <ipfixcol.h>

#include "sender.h"

/** Invalid socket value */
#define SOCKET_INVALID (-1)
/** Maximum size of internal buffer (2^19) */
#define BUFFER_SIZE (524288)

/** Module description */
static const char *msg_module = "forwarding(sender)";

/**
 * \brief Sender to destination node
 */
struct _fwd_sender {
	char *dst_addr;       /**< Destination IP address     */
	char *dst_port;       /**< Destination port           */
	int socket_fd;        /**< Socket                     */

	uint8_t *buffer_data; /**< Buffer for unsent parts of messsages */
	size_t buffer_valid;  /**< Valid part of the buffer             */
};

/**
 * \brief Network address and service translation
 * \param[in] addr Destination address
 * \param[in] port Destination port
 * \return Same as getaddrinfo()
 * \warning Returned structure MUST be freed using freeaddrinfo()
 */
static struct addrinfo *sender_getaddrinfo(const char *addr, const char *port)
{
	int ret_val;
	struct addrinfo hints;
	struct addrinfo *info;

	// Network address and service translation
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	ret_val = getaddrinfo(addr, port, &hints, &info);
	if (ret_val != 0) {
		// Failed to translate a given address
		MSG_ERROR(msg_module, "Failed to translate address (%s).",
			gai_strerror(ret_val));
		return NULL;
	}

	return info;
}

/**
 * \brief Close a socket connection
 * \param[in,out] s Sender structure
 */
static void sender_socket_close(fwd_sender_t *s)
{
	if (s->socket_fd == SOCKET_INVALID) {
		// Already closed
		return;
	}

	close(s->socket_fd);
	s->socket_fd = SOCKET_INVALID;

	// Clear the buffer
	s->buffer_valid = 0;
}

/** Create a new sender */
fwd_sender_t *sender_create(const char *addr, const char *port)
{
	// Check parameters
	if (!addr || !port) {
		return NULL;
	}

	// Just try to translate a given address
	struct addrinfo *info;
	info = sender_getaddrinfo(addr, port);
	if (!info) {
		return NULL;
	}
	freeaddrinfo(info);

	// Create a structure
	fwd_sender_t *res;
	res = (fwd_sender_t *) calloc(1, sizeof(fwd_sender_t));
	if (!res) {
		MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)",
			__FILE__, __LINE__);
		return NULL;
	}

	res->buffer_data = NULL;
	res->buffer_valid = 0;
	res->socket_fd = SOCKET_INVALID;

	res->dst_addr = strdup(addr);
	res->dst_port = strdup(port);
	if (!res->dst_addr || !res->dst_port) {
		// Failed to copy parameters
		sender_destroy(res);
		return NULL;
	}

	return res;
}

/** Destroy a sender */
void sender_destroy(fwd_sender_t *s)
{
	if (!s) {
		return;
	}

	// Address & port
	free(s->dst_addr);
	free(s->dst_port);
	free(s->buffer_data);

	// Socket
	sender_socket_close(s);
	free(s);
}

/** Get destination address */
const char *sender_get_address(const fwd_sender_t *s)
{
	return s->dst_addr;
}

/** Get destination port */
const char *sender_get_port(const fwd_sender_t *s)
{
	return s->dst_port;
}

/** (Re)connect to the destination */
int sender_connect(fwd_sender_t *s)
{
	if (s->socket_fd != SOCKET_INVALID) {
		// Socket already connected -> close
		sender_socket_close(s);
	}

	// Get a translation of an address
	struct addrinfo *dst_info;
	dst_info = sender_getaddrinfo(s->dst_addr, s->dst_port);
	if (!dst_info) {
		return 1;
	}

	// Create a new socket and try to connect
	struct addrinfo *p;
	int new_fd;

	for (p = dst_info; p != NULL; p = p->ai_next) {
		// Create socket
		new_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (new_fd == SOCKET_INVALID) {
			// Failed to create a new socket
			continue;
		}

		if (connect(new_fd, p->ai_addr, p->ai_addrlen) != 0) {
			// Failed to connect
			close(new_fd);
			continue;
		}

		// Success
		break;
	}

	freeaddrinfo(dst_info);
	if (p == NULL) {
		// Failed to create the socket & connect
		return 1;
	}

	s->socket_fd = new_fd;
	return 0;
}

/**
 * \brief Get a new memory in the internal buffer of unsent messages
 * \param[in,out] s Sender structure
 * \param[in] len Required size of the memory
 * \return Pointer or NULL
 */
static uint8_t *sender_prepare_buffer(fwd_sender_t *s, size_t size)
{
	if (!s->buffer_data) {
		// Not initialized yet
		s->buffer_data = (uint8_t *) malloc(BUFFER_SIZE);
		if (!s->buffer_data) {
			MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)",
				__FILE__, __LINE__);
			return NULL;
		}

		s->buffer_valid = 0;
	}

	if (size + s->buffer_valid > BUFFER_SIZE) {
		// Not enought memory
		return NULL;
	}

	uint8_t *ptr = s->buffer_data + s->buffer_valid;
	s->buffer_valid += size;

	return ptr;
}

/**
 * \brief Store a packet (or a part of a packet) into internal structures
 * \param[in,out] s Sender structure
 * \param[in] data Data
 * \param[in] len Size of data
 * \return On success returns 0. Otherwise returns non-zero value.
 */
static int sender_buffer_store(fwd_sender_t *s, const uint8_t *data, size_t size)
{
	uint8_t *buffer = sender_prepare_buffer(s, size);
	if (!buffer) {
		return 1;
	}

	memcpy(buffer, data, size);
	return 0;
}

/**
 * \brief Store a packet (or a part of a packet) into internal structures
 * \param[in,out] s Sender structure
 * \param[in] io Array of packet parts
 * \param[in] parts Number of packet parts
 * \param[in] offset Drop first N bytes
 * \return On success return 0. Otherwise returns non-zero value.
 */
static int sender_buffer_store_io(fwd_sender_t *s, const struct iovec *io,
	size_t parts, size_t offset)
{
	// Get length of the packet
	size_t total_len = 0;
	for (unsigned int i = 0; i < parts; ++i) {
		total_len += io[i].iov_len;
	}

	if (total_len <= offset) {
		// Out of range
		return 1;
	}

	total_len -= offset; // Size of required memory
	uint8_t *buffer = sender_prepare_buffer(s, total_len);
	if (!buffer) {
		return 1;
	}

	// Copy data
	unsigned int pos_total = 0;
	unsigned int pos_copy = 0;

	for (unsigned int i = 0; i < parts; ++i) {
		uint8_t *begin = (uint8_t *) io[i].iov_base;
		size_t len = io[i].iov_len;

		if (pos_total < offset) {
			// Skip parts within offset area
			if (pos_total + len <= offset) {
				pos_total += len;
				continue;
			}

			// Move pointers to the end of the offset
			unsigned int diff = offset - pos_total;
			begin += diff;
			len -= diff;
			pos_total += diff;
		}

		memcpy(buffer + pos_copy, begin, len);
		pos_copy += len;
		pos_total += len;
	}

	return 0;
}

/**
 * \brief Send a content of the internal buffer
 * \param[in,out] s    Sender structure
 * \param[in]     mode Mode (blocking or non-blocking)
 * \return When the buffer is empty returns 0. Otherwise (something is still
 * in the buffer) returns non-zero value.
 */
int sender_send_buffer(fwd_sender_t *s, enum SEND_MODE mode)
{
	if (s->socket_fd == SOCKET_INVALID) {
		return 1;
	}

	if (s->buffer_valid == 0) {
		// Buffer is empty
		return 0;
	}

	// Something is in the buffer
	const uint8_t *ptr = s->buffer_data;
	size_t todo = s->buffer_valid;

	int flags = MSG_NOSIGNAL;
	flags |= (mode == MODE_NON_BLOCKING) ? MSG_DONTWAIT : 0;

	while (todo > 0) {
		// Send
		ssize_t ret;
		ret = send(s->socket_fd, ptr, todo, flags);

		if (ret != -1) {
			// Skip successfully sent data
			ptr  += ret;
			todo -= ret;
			continue;
		}

		if (errno != EAGAIN && errno != EWOULDBLOCK) {
			// Unexpected type of error
			MSG_WARNING(msg_module, "Connection to \"%s:%s\" closed (%s).",
				s->dst_addr, s->dst_port, strerror(errno));
			sender_socket_close(s);
			return 1;
		}

		// Operation would block
		if (mode == MODE_BLOCKING) {
			continue;
		}

		// Only non-blocking mode can get here
		if (todo == s->buffer_valid) {
			// Nothing was sent
			return 1;
		}

		// A part of the buffer was sent
		memmove(s->buffer_data, ptr, todo);
		s->buffer_valid = todo;
		return 1;
	}

	s->buffer_valid = 0;
	return 0;
}


/** Send data to the destination */
enum SEND_STATUS sender_send(fwd_sender_t *s, const void *buf, size_t len,
	enum SEND_MODE mode, bool required)
{
	// Send a content of the internal buffery (if any)
	if (sender_send_buffer(s, mode)) {
		if (s->socket_fd == SOCKET_INVALID) {
			// Socket closed
			return STATUS_CLOSED;
		}

		// Still connected, but busy
		if (!required) {
			return STATUS_BUSY;
		}

		// Required delivery
		if (sender_buffer_store(s, buf, len)) {
			MSG_WARNING(msg_module, "Unable to store 'required' message for "
				"'%s:%s' into the internal buffer. Connection must be closed to"
				" prevent receiving invalid messages.", sender_get_address(s),
				sender_get_port(s));
			sender_socket_close(s);
			return STATUS_CLOSED;
		}

		return STATUS_OK;
	}

	const uint8_t *ptr = (const uint8_t *) buf;
	size_t todo = len;

	int flags = MSG_NOSIGNAL; // Never use signals
	flags |= (mode == MODE_NON_BLOCKING) ? MSG_DONTWAIT : 0;

	while (todo > 0) {
		// Send
		ssize_t ret;
		ret = send(s->socket_fd, ptr, todo, flags);

		if (ret != -1) {
			// Skip successfully sent data
			ptr  += ret;
			todo -= ret;
			continue;
		}

		if (errno != EAGAIN && errno != EWOULDBLOCK) {
			// Unexpected type of error
			MSG_WARNING(msg_module, "Connection to \"%s:%s\" closed (%s).",
				s->dst_addr, s->dst_port, strerror(errno));
			sender_socket_close(s);
			return STATUS_CLOSED;
		}

		// Operation would block
		if (mode == MODE_BLOCKING) {
			continue;
		}

		// Only non-blocking mode can get here
		if (todo == len && !required) {
			// Nothing sent & not required data -> skip
			return STATUS_BUSY;
		}

		// Required data or partially sent -> store
		if (sender_buffer_store(s, ptr, todo)) {
			MSG_WARNING(msg_module, "Unable to store a rest of the message for "
				"'%s:%s̈́'. Connection must be closed to prevent receiving "
				"invalid messages.", sender_get_address(s), sender_get_port(s));
			sender_socket_close(s);
			return STATUS_CLOSED;
		}

		return STATUS_OK;
	}

	// The whole message was sucessfully sent
	return STATUS_OK;
}

/** Send data to the destination */
enum SEND_STATUS sender_send_parts(fwd_sender_t *s, struct iovec *io,
	size_t parts, enum SEND_MODE mode, bool required)
{
	// Send the content of the internal buffery (if any)
	if (sender_send_buffer(s, mode)) {
		if (s->socket_fd == SOCKET_INVALID) {
			// Socket closed
			return STATUS_CLOSED;
		}

		// Still connected, but busy
		if (!required) {
			return STATUS_BUSY;
		}

		// Required delivery
		if (sender_buffer_store_io(s, io, parts, 0)) {
			MSG_WARNING(msg_module, "Unable to store 'required' message for "
				"'%s:%s' into the internal buffer. Connection must be closed to"
				" prevent receiving invalid messages.", sender_get_address(s),
				sender_get_port(s));
			sender_socket_close(s);
			return STATUS_CLOSED;
		}

		return STATUS_OK;
	}

	// Get length of the message
	size_t total_len = 0;
	for (unsigned int i = 0; i < parts; ++i) {
		total_len += io[i].iov_len;
	}

	// Prepare the message
	struct msghdr msg;
	memset(&msg, 0, sizeof(msg));
	msg.msg_iov = io;
	msg.msg_iovlen = parts;

	int flags = MSG_NOSIGNAL; // Never use signals
	flags |= (mode == MODE_NON_BLOCKING) ? MSG_DONTWAIT : 0;

	// Try to send
	ssize_t ret;
before_sendmsg:
	ret = sendmsg(s->socket_fd, &msg, flags);

	if (ret != -1) {
		// No error
		size_t sent = (size_t) ret;
		if (sent == total_len) {
			// Everything successfully sent
			return STATUS_OK;
		}

		// Only a part of the message was sent -> store the rest
		MSG_DEBUG(msg_module, "Packet partially sent (%u of %u)",
			(unsigned int) sent, (unsigned int) total_len);

		if (sender_buffer_store_io(s, io, parts, sent)) {
			// Failed to store the rest of the message -> disconnect
			MSG_WARNING(msg_module, "Unable to store a rest of the message for "
				"'%s:%s̈́'. Connection must be closed to prevent receiving "
				"invalid messages.", sender_get_address(s), sender_get_port(s));
			sender_socket_close(s);
			return STATUS_CLOSED;
		}

		return STATUS_OK;
	}

	// Return value is "-1" i.e. nothing was sent
	if (errno != EAGAIN && errno != EWOULDBLOCK) {
		// Unexpected type of error
		MSG_WARNING(msg_module, "Connection to \"%s:%s\" closed (%s).",
			s->dst_addr, s->dst_port, strerror(errno));
		sender_socket_close(s);
		return STATUS_CLOSED;
	}

	// Nothing sent, but send operation would block
	if (mode == MODE_BLOCKING) {
		goto before_sendmsg;
	}

	// Only non-blocking mode can get here
	if (!required) {
		return STATUS_BUSY;
	}

	// Required message must be stored !!!
	if (sender_buffer_store_io(s, io, parts, 0)) {
		MSG_WARNING(msg_module, "Unable to send 'required' message to "
			"'%s:%s'. Connection must be close to prevent receiving "
			"invalid messages.", sender_get_address(s), sender_get_port(s));
		sender_socket_close(s);
		return STATUS_CLOSED;
	}

	return STATUS_OK;
}
