/**
 * \file storage/forwarding/destination.h
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Destination manager (header file)
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
 * \defgroup destination_manager Packet distribution and distribution models
 * \ingroup forwardingStoragePlugin
 *
 * @{
 */


#ifndef DESTINATION_H
#define DESTINATION_H

#include <stdbool.h>
#include "sender.h"
#include "packet.h"
#include "templates.h"

/**
 * \brief Mode of flow distribution
 */
enum DIST_MODE {
	DIST_INVALID,           /**< Invalid type                            */
	DIST_ALL,               /**< Distribute flows to all destinations    */
	DIST_ROUND_ROBIN        /**< Distribute using Round Robin            */
};

// Structure prototype
typedef struct _fwd_dest fwd_dest_t;

/**
 * \brief Create new destination manager
 * \param[in] tmplt_mgr  Template manager
 * \return Pointer or NULL
 */
fwd_dest_t *dest_create(fwd_tmplt_mgr_t *tmplt_mgr);

/**
 * \brief Destroy the destination manager
 * \param[in,out] dst_mgr Destination manager
 */
void dest_destroy(fwd_dest_t *dst_mgr);

/**
 * \brief Add new destination
 * \param[in,out] dst_mgr Destination manager
 * \param[in] sndr New sender
 * \return On success returns 0. Otherwise returns non-zero value.
 */
int dest_add(fwd_dest_t *dst_mgr, fwd_sender_t *sndr);

/**
 * \brief Try to reconnect to all disconnected destinations
 *
 * For automatic reconnection see dest_connector_start()
 * \param[in,out] dst_mgr Destination manager
 * \param[in] verbose Print warning message about disconnected destinations
 */
void dest_reconnect(fwd_dest_t *dst_mgr, bool verbose);

/**
 * \brief Enable automatic reconnection of disconnected destination
 *
 * Create a new thread that periodically tries to reconnect clients, so
 * no manual reconnection (i.e. dest_reconnect()) is required.
 * \param[in,out] dst_mgr Destination manager
 * \param[in] period Reconnection period (in milliseconds)
 * \return On success returns 0. Otherwise returns non-zero value.
 */
int dest_connector_start(fwd_dest_t *dst_mgr, int period);

/**
 * \brief Disable automatic reconnection of disconnected clients
 * \param[in,out] dst_mgr Destination manager
 * \return On success returns 0. Otherwise returns non-zero value.
 */
int dest_connector_stop(fwd_dest_t *dst_mgr);

/**
 * \brief Send prepared packet(s)
 * \param[in,out] dst_mgr     Destination manager
 * \param[in,out] bldr_all    Prepared packets - all parts (Packet builder)
 * \param[in,out] bldr_tmplts Prepared packets - only templates (Packet builder)
 * \param[in] mode Distribution mode
 * \warning Make sure that nobody is using a Template manager of the Forwarding
 *   plugin, because the manager is not thread-safety.
 */
void dest_send(fwd_dest_t *dst_mgr, fwd_bldr_t *bldr_all,
	fwd_bldr_t *bldr_tmplts, enum DIST_MODE mode);


#endif // DESTINATION_H

/**@}*/
