/**
 * \file ipfix_parser.c
 * \author Radek Krejci <rkrejci@cesnet.cz>
 * \brief Simple parsing of IPFIX packets for Storage plugins.
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

#include <stdlib.h>
#include <pthread.h>

#include <commlbr.h>

#include "data_mngmt.h"
#include "queues.h"
#include "../ipfixcol.h"

static struct data_manager_config *data_mngmts = NULL;

/**
 * \brief Search for Data manager handling specified Observation Domain ID
 *
 * \todo: improve search e.g. by some kind of sorting data_mngmts
 *
 * @param[in] id Observation domain ID of wanted Data manager.
 * @return Desired Data manager's configuration structure if exists, NULL if
 * there is no Data manager for specified Observation domain ID
 */
static struct data_manager_config *get_data_mngmt_config (uint32_t id)
{
	struct data_manager_config *aux_cfg = data_mngmts;

	while (aux_cfg) {
		if (aux_cfg->observation_domain_id == id) {
			break;
		}
		aux_cfg = aux_cfg->next;
	}

	return (aux_cfg);
}


void parse_ipfix (void* packet, struct input_info* input_info, struct storage* storage_plugins)
{
	struct ipfix_message* msg;
	struct data_manager_config *config = NULL;

	if (packet == NULL || input_info == NULL || storage_plugins == NULL) {
		VERBOSE (CL_VERBOSE_OFF, "Invalid parameters in function parse_ipfix().");
		return;
	}

	msg = (struct ipfix_message*) malloc (sizeof (struct ipfix_message));
	msg->pkt_header = (struct ipfix_header*) packet;
	msg->input_info = input_info;
	MSG (CL_VERBOSE_BASIC, "Processing data for Observation domain ID %d.",
			msg->pkt_header->observation_domain_id);

	/* check IPFIX version */
	if (msg->pkt_header->version != htons(IPFIX_VERSION)) {
		VERBOSE (CL_VERBOSE_BASIC, "Unexpected IPFIX version detected (%X), skipping packet.",
				msg->pkt_header->version);
		free (msg);
		return;
	}

	/* get appropriate data manager's config according to Observation domain ID */
	config = get_data_mngmt_config (msg->pkt_header->observation_domain_id);
	if (config == NULL) {
		/*
		 * no data manager config for this observation domain ID found -
		 * we have a new observation domain ID, so create new data manager for
		 * it
		 */
		config = create_data_manager (msg->pkt_header->observation_domain_id, storage_plugins);
		if (config == NULL) {
			VERBOSE (CL_VERBOSE_BASIC, "Unable to create data manager for Observation Domain ID %d, skipping data.",
					msg->pkt_header->observation_domain_id);
			free (msg);
			return;
		}
	}

	/**
	 * \todo process IPFIX and fillup the ipfix_message structure
	 */


	if (rbuffer_write (config->in_queue, msg, 1) != 0) {
		VERBOSE (CL_VERBOSE_BASIC, "Unable to write into Data manager's input queue, skipping data.");
		free (packet);
	}
}
