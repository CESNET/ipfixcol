/**
 * \file preprocessor.h
 * \author Petr Velan <petr.velan@cesnet.cz>
 * \brief Data Manager's functions
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

#ifndef PREPROCESSOR_H_
#define PREPROCESSOR_H_

#include <stdint.h>
#include <pthread.h>

#include "ipfixcol.h"
#include "config.h"
#include "configurator.h"
#include "queues.h"

/**
 * \brief
 *
 * Structure holding UDP specific template configuration
 */
struct udp_conf {
	uint16_t template_life_time;
	uint16_t template_life_packet;
	uint16_t options_template_life_time;
	uint16_t options_template_life_packet;
};


/**
 * \brief Does first basic parsing of raw ipfix message
 *
 * Creates pointers to data and template sets, creates data manager for
 * observation id if it does not exist.
 *
 * @param[in] packet Raw packet from input plugin
 * @param[in] len Length of the packet
 * @param[in] input_info Input information from input plugin
 * @param[in] source_status Status of source (new, opened, closed)
 * @return void
 */
void preprocessor_parse_msg (void* packet, int len, struct input_info* input_info, int source_state);

/**
 * \brief Returns pointer to preprocessors output queue.
 *
 * @return preprocessors output queue
 */
struct ring_buffer *get_preprocessor_output_queue();

/**
 * \brief Set new preprocessor output queue
 * 
 * @param out_queue
 */
void preprocessor_set_output_queue(struct ring_buffer *out_queue);

/**
 * \brief Set new preprocessor configurator
 *
 * @param conf configurator
 */
void preprocessor_set_configurator(configurator *config);


/**
 * \brief Close all data managers and their storage plugins
 */
void preprocessor_close ();

#endif /* PREPROCESSOR_H_ */
