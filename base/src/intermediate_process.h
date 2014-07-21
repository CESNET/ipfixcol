/**
 * \file intermediate_process.h
 * \author Michal Srb <michal.srb@cesnet.cz>
 * \brief Intermediate Process
 *
 * Copyright (C) 2012 CESNET, z.s.p.o.
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
 * \defgroup Intermediate_API
 * \ingroup ipfixmedCore
 *
 * In ipfixmed, Intermediate Process is a thread that picks up data from its
 * input queue and calls function process_message() on it.
 *
 * @{
 */

#include "queues.h"
//#include "ipfixmed.h"
#include "config.h"


/**
 * \brief Initialize Intermediate Process.
 *
 * \param[in] in_queue input queue
 * \param[in] out_queue output queue
 * \param[in] intermediate intermediate plugin structure
 * \param[in] xmldata XML configuration for this plugin
 * \param[in] ip_id source ID for creating templates
 * \param[in] template_mgr collector's Template Manager
 * \param[out] config configuration structure
 * \return 0 on success, negative value otherwise
 */
int ip_init(struct ring_buffer *in_queue, struct ring_buffer *out_queue,
		struct intermediate *intermediate, char *xmldata, uint32_t ip_id,
		struct ipfix_template_mgr *template_mgr, void **config);


/**
 * \brief Wait for data from input queue in loop.
 *
 * This function runs in separated thread.
 *
 * \param[in] config configuration structure
 * \return NULL
 */
void *ip_loop(void *config);


/**
 * \brief Pass processed IPFIX message to the output queue.
 *
 * \param[in] config configuration structure
 * \param[in] message IPFIX message
 * \return 0 on success, negative value otherwise
 */
int pass_message(void *config, struct ipfix_message *message);

/**
 * \brief Drop IPFIX message.
 *
 * \param[in] config configuration structure
 * \param[in] message IPFIX message
 * \return 0 on success, negative value otherwise
 */
int drop_message(void *config, struct ipfix_message *message);

/**
 * \brief Destroy Intermediate Process
 *
 * \param[in] config configuration structure
 * \return 0 on success, negative value otherwise
 */
int ip_destroy(void *config);

/**
 * \brief Stop Intermediate Process
 *
 * \param[in] config configuration structure
 * \return 0 on success, negative value otherwise
 */
int ip_stop(void *config);

/**@}*/
