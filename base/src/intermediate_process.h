/**
 * \file intermediate_process.h
 * \author Michal Srb <michal.srb@cesnet.cz>
 * \author Michal Kozubik <kozubik@cesnet.cz>
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
 * \defgroup intermediateProcess Intermediate_API
 * \ingroup internalAPIs
 *
 * In ipfixmed, Intermediate Process is a thread that picks up data from its
 * input queue and calls function process_message() on it.
 *
 * @{
 */

#include "queues.h"
#include "config.h"

/**
 * \brief Initialize Intermediate Process.
 *
 * \param[in] conf intermediate plugin structure
 * \param[in] ip_id source ID for creating templates
 * \return 0 on success, negative value otherwise
 */
int ip_init(struct intermediate *conf, uint32_t ip_id);

/**
 * \brief Set new input queue
 * 
 * \param conf intermediate process
 * \param in_queue new input queue
 * \return 0 on success
 */
int ip_change_in_queue(struct intermediate *conf, struct ring_buffer *in_queue);

/**
 * \brief Destroy Intermediate Process
 *
 * \param[in] conf configuration structure
 * \return 0 on success, negative value otherwise
 */
int ip_destroy(struct intermediate *conf);

/**
 * \brief Stop Intermediate Process
 *
 * \param[in] conf configuration structure
 * \return 0 on success, negative value otherwise
 */
int ip_stop(struct intermediate *conf);

/**@}*/
