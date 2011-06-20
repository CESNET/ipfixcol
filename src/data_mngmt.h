/**
 * \file data_mngmt.h
 * \author Radek Krejci <rkrejci@cesnet.cz>
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

#ifndef DATA_MNGMT_H_
#define DATA_MNGMT_H_

#include <stdint.h>
#include <pthread.h>

#include "../ipfixcol.h"

#include "queues.h"

struct data_manager_config {
	uint32_t observation_domain_id;
	pthread_t thread_id;
	unsigned int plugins_count;
	struct ring_buffer *in_queue;
	struct ring_buffer *store_queue;
	struct storage* plugins;
	struct data_manager_config *next;
};

void parse_ipfix (void* packet, struct input_info* input_info, struct storage* storage_plugins);

struct data_manager_config* create_data_manager (uint32_t observation_domain_id, struct storage* storage_plugins);


#endif /* DATA_MNGMT_H_ */
