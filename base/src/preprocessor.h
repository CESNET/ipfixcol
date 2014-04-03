/**
 * \file preprocessor.h
 * \author Petr Velan <petr.velan@cesnet.cz>
 * \brief Data Manager's functions
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

#ifndef PREPROCESSOR_H_
#define PREPROCESSOR_H_

#include <stdint.h>
#include <pthread.h>

#include "ipfixcol.h"
#include "config.h"
#include "queues.h"

/**
 * \brief Does first basic parsing of raw ipfix message
 *
 * Creates pointers to data and template sets, creates data manager for
 * observation id if it does not exist.
 *
 * @param[in] packet Raw packet from input plugin
 * @param[in] len Length of the packet
 * @param[in] input_info Input information from input plugin
 * @param[in] storage_plugins List of storage plugins that should be passed to data manager
 * @return void
 */
void preprocessor_parse_msg (void* packet, int len, struct input_info* input_info, struct storage_list* storage_plugins);

/**
 * \brief This function sets the queue for preprocessor and inits crc computing.
 *
 * @param out_queue preprocessor's output queue
 * @param template_mgr collector's Template Manager
 * @return 0 on success, negative value otherwise
 */
int preprocessor_init(struct ring_buffer *out_queue, struct ipfix_template_mgr *template_mgr);


/**
 * \brief Returns pointer to preprocessors output queue.
 *
 * @return preprocessors output queue
 */
struct ring_buffer *get_preprocessor_output_queue();


/**
 * \brief Close all data managers and their storage plugins
 *
 * @return void
 */
void preprocessor_close ();

#endif /* PREPROCESSOR_H_ */
