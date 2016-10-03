/**
 * \file storage/forwarding/configuration.h
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Configuration of the forwarding plugin (header file)
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
 * \defgroup configuration Configuration of the forwaring plugin
 * \ingroup forwardingStoragePlugin
 *
 * @{
 */

#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include <stdint.h>
#include "sender.h"
#include "destination.h"
#include "packet.h"
#include "templates.h"

/**
 * \brief Configuration of the plugin
 */
struct plugin_config {
	char *def_port;             /**< Default port                            */
	enum DIST_MODE mode;        /**< Distribution mode                       */
	uint16_t packet_size;       /**< Maximal size per generated packet       */
	int reconn_period;          /**< Reconnection period (in milliseconds)   */

	fwd_dest_t *dest_mgr;       /**< Destination manager                     */

	fwd_bldr_t *builder_all;    /**< Packet builder (for data and templates) */
	fwd_bldr_t *builder_tmplt;  /**< Packet builder (for templates only)     */

	fwd_tmplt_mgr_t *tmplt_mgr; /**< Template manager                        */
};

/**
 * \brief Parse a configuration of the plugin
 * \param[in] cfg_string XML configurations
 * \return On success returns parsed configuration.
 */
struct plugin_config *config_parse(const char *cfg_string);

/**
 * \brief Destroy a configuration of the plugin
 * \param[in,out] cfg Configuration
 */
void config_destroy(struct plugin_config *cfg);


#endif // CONFIGURATION_H

/**@}*/

