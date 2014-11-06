/**
 * \file intermediate.h
 * \author Petr Velan <petr.velan@cesnet.cz>
 * \brief IPFIX Collector Intermediate plugin API.
 *
 * Copyright (C) 2014 CESNET, z.s.p.o.
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
 * \defgroup inputAPI Intermediate Plugins API
 * \ingroup publicAPIs
 *
 * These functions specify a communication interface between ipfixcol core and
 * intermediate plugins for data processing. Intermediate plugins get IPFIX
 * messages and are allowed to modify, create or drop the messages.
 * The intermediate plugins are connected in series by ring buffers, therefore
 * each message will pass all plugins one by one, unless some plugin discards
 * it.
 *
 * @{
 */
#ifndef IPFIXCOL_INTERMEDIATE_H_
#define IPFIXCOL_INTERMEDIATE_H_

#include "api.h"

API int intermediate_plugin_init(char *params, void *ip_config, uint32_t ip_id, struct ipfix_template_mgr *template_mgr, void **config);

API int intermediate_plugin_close(void *config);

API int process_message(void *config, void *message);

/**
* \brief Pass processed IPFIX message to the output queue.
*
* \param[in] config configuration structure
* \param[in] message IPFIX message
* \return 0 on success, negative value otherwise
*/
API int pass_message(void *config, struct ipfix_message *message);

/**
* \brief Drop IPFIX message.
*
* \param[in] config configuration structure
* \param[in] message IPFIX message
* \return 0 on success, negative value otherwise
*/
API int drop_message(void *config, struct ipfix_message *message);

#endif /* IPFIXCOL_INTERMEDIATE_H_ */

/**@}*/

