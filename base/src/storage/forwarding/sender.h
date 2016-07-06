/**
 * \file storage/forwarding/sender.h
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Connection to a remote host (header file)
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

/**
 * \defgroup sender Packet sender 
 * \ingroup forwardingStoragePlugin
 *
 * @{
 */

#ifndef SENDER_H
#define SENDER_H

#include <sys/socket.h>
#include <stdbool.h>
#include <stdlib.h>

/** \brief Mode of sending operation */
enum SEND_MODE {
	MODE_BLOCKING,        /**< Blocking send operation     */
	MODE_NON_BLOCKING     /**< Non-blocking send operation */
};

/** \brief Return status of sending operation */
enum SEND_STATUS {
	STATUS_INVALID,   /**< Invalid arguments                                 */
	STATUS_OK,        /**< All data successfully sent                        */
	STATUS_BUSY,      /**< Nothing was sent. The operation would block.      */
	STATUS_CLOSED     /**< Socket is closed or broken. Use sender_connect(). */
};

/* Prototypes */
typedef struct _fwd_sender fwd_sender_t;

/**
 * \brief Create a new sender
 * \param[in] addr Destination IP address
 * \param[in] port Destination port
 * \return On success returns pointer to new sender. Otherwise returns NULL.
 */
fwd_sender_t *sender_create(const char *addr, const char *port);

/**
 * \brief Destroy a sender
 * \param[in,out] s Sender structure (can be NULL)
 */
void sender_destroy(fwd_sender_t *s);

/**
 * \brief Get destination address
 * \param[in] s Sender structure
 * \return Address
 */
const char *sender_get_address(const fwd_sender_t *s);

/**
 * \brief Get destination port
 * \param s Sender structure
 * \return Port
 */
const char *sender_get_port(const fwd_sender_t *s);

/**
 * \brief (Re)connect to the destination
 *
 * Create a socket and try to connect. Previous connection is closed.
 * \param[in,out] s Sender structure
 * \return On success returns 0. Otherwise returns non-zero value.
 */
int sender_connect(fwd_sender_t *s);

/**
 * \brief Send data to the destination
 *
 * When \p required is True and the destination is busy, record will be stored
 * into internal structures and send during next call of any sending function.
 * In this case (i.e. \p required == True) return value is never STATUS_BUSY.
 * \param[in,out] s Sender structure
 * \param[in] buf Data
 * \param[in] len Length of data
 * \param[in] mode Mode of sending operation
 * \param[in] required Required delivery
 * \return Status of the operation
 */
enum SEND_STATUS sender_send(fwd_sender_t *s, const void *buf, size_t len,
	enum SEND_MODE mode, bool required);

/**
 * \brief Send data to the destination
 *
 * When \p required is True and the destination is busy, record will be stored
 * into internal structures and send during next call of any sending function.
 * In this case (i.e. \p required == True) return value is never STATUS_BUSY.
 * \param[in,out] s Sender structure
 * \param[in] io Array of packet parts
 * \param[in] parts Number of parts
 * \param[in] mode Mode of sending operation
 * \param[in] required Required delivery
 * \return Status of the operation
 */
enum SEND_STATUS sender_send_parts(fwd_sender_t *s, struct iovec *io,
	size_t parts, enum SEND_MODE mode, bool required);

#endif // SENDER_H

/**@}*/
