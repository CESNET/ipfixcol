/**
 * \file preprocessor.c
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
#include <arpa/inet.h>

#include <commlbr.h>

#include "preprocessor.h"
#include "data_manager.h"
#include "queues.h"
#include "../ipfixcol.h"

/**
 * \brief List of data manager configurations
 */
static struct data_manager_config *data_mngmts = NULL;

/**
 * \brief Search for Data manager handling specified Observation Domain ID
 *
 * \todo: improve search e.g. by some kind of sorting data_managers
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

/**
 * \brief Search for Data manager handling input specified by 
 * input_info structure
 *
 * @param[in] info Structure input_info specifying data manager
 * @param[out] prev Data manager configuration preceeding the desired one
 * @return Desired Data manager's configuration structure if exists, NULL 
 * otherwise
 */
static struct data_manager_config *get_data_mngmt_by_input_info (struct input_info *info, struct data_manager_config **prev)
{
    struct data_manager_config *aux_cfg = data_mngmts;
    struct input_info_network *ii_network1, *ii_network2;

	while (aux_cfg) {
        /* input types must match */
        if (aux_cfg->input_info->type == info->type) {
            /* file names must match for files */
            if (info->type == SOURCE_TYPE_IPFIX_FILE && 
                strcmp(((struct input_info_file*) info)->name, ((struct input_info_file*) aux_cfg->input_info)->name) == 0) {
                break;
            } else {/* we have struct input_info_network */
                ii_network1 = (struct input_info_network*) aux_cfg->input_info;
                ii_network2 = (struct input_info_network*) info;
                /* ports and protocols must match */

                if (ii_network1->dst_port == ii_network2->dst_port && 
                        ii_network1->src_port == ii_network2->src_port &&
                        ii_network1->l3_proto == ii_network2->l3_proto) {
                    /* compare addresses, dependent on IP protocol version*/
                    if (ii_network1->l3_proto == 4) {
                        if (ii_network1->src_addr.ipv4.s_addr == ii_network2->src_addr.ipv4.s_addr) {
                            break;
                        }
                    } else {
                        if (ii_network1->src_addr.ipv6.s6_addr32[0] == ii_network2->src_addr.ipv6.s6_addr32[0] &&
                            ii_network1->src_addr.ipv6.s6_addr32[1] == ii_network2->src_addr.ipv6.s6_addr32[1] &&
                            ii_network1->src_addr.ipv6.s6_addr32[2] == ii_network2->src_addr.ipv6.s6_addr32[2] &&
                            ii_network1->src_addr.ipv6.s6_addr32[3] == ii_network2->src_addr.ipv6.s6_addr32[3]) {
                            break;
                        }
                    }
                }
	    	}
        }
        /* save previous configuration */
        *prev = aux_cfg;

		aux_cfg = aux_cfg->next;
	}

	return (aux_cfg);
 
}


/**
 * \brief Closes all data managers
 *
 * Calls the data_manager_close function on all open managers.
 *
 * @return void
 */
void preprocessor_close()
{
	struct data_manager_config *aux_cfg = data_mngmts, *tmp_cfg;

	while (aux_cfg) {
        tmp_cfg = aux_cfg;
        aux_cfg = aux_cfg->next;
        data_manager_close(&tmp_cfg);
	}
    data_mngmts = NULL;
    return;    
}

void preprocessor_parse_msg (void* packet, struct input_info* input_info, struct storage_list* storage_plugins)
{
	struct ipfix_message* msg;
	struct data_manager_config *config = NULL, *prev_config = NULL;

	if (input_info == NULL || storage_plugins == NULL) {
		VERBOSE (CL_VERBOSE_OFF, "Invalid parameters in function preprocessor_parse_msgx().");
		return;
	}

	/* connection closed, close data manager */
    if (packet == NULL) {
        config = get_data_mngmt_by_input_info (input_info, &prev_config);

        if (!config) {
        	VERBOSE(CL_VERBOSE_BASIC, "Data manager NOT found, probably more exporters with same OID.");
        	return;
        }
        /* remove data manager from the list */
        if (prev_config == NULL) {
            data_mngmts = NULL;
        } else {
            prev_config->next = config->next;
        }

        /* close and free data manager */
        data_manager_close(&config);
        return;
    }

	msg = (struct ipfix_message*) calloc (1, sizeof (struct ipfix_message));
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
		config = data_manager_create (msg->pkt_header->observation_domain_id, storage_plugins, input_info);
		if (config == NULL) {
			VERBOSE (CL_VERBOSE_BASIC, "Unable to create data manager for Observation Domain ID %d, skipping data.",
					msg->pkt_header->observation_domain_id);
			free (msg);
			return;
		}
	    /* add config to data_mngmts structure */
	    if (data_mngmts == NULL) {
	        data_mngmts = config;
	    } else {
	        data_mngmts->next = config;
	    }
	}


	/* process IPFIX packet and fillup the ipfix_message structure */
    uint8_t *p = packet + IPFIX_HEADER_LENGTH;
    int t_set_count = 0, ot_set_count = 0, d_set_count = 0;
    struct ipfix_set_header *set_header;
    while (p < (uint8_t*) packet + ntohs(msg->pkt_header->length)) {
        set_header = (struct ipfix_set_header*) p;
        switch (ntohs(set_header->flowset_id)) {
            case IPFIX_TEMPLATE_FLOWSET_ID:
                msg->templ_set[t_set_count++] = (struct ipfix_template_set *) set_header;
                break;
            case IPFIX_OPTION_FLOWSET_ID:
                 msg->opt_templ_set[ot_set_count++] = (struct ipfix_options_template_set *) set_header;
                break;
            default:
                if (set_header->flowset_id < IPFIX_MIN_RECORD_FLOWSET_ID) {
                    VERBOSE (CL_VERBOSE_BASIC, "Unknown Set ID %d", set_header->flowset_id);
                } else {
                    msg->data_set[d_set_count++].data_set = (struct ipfix_data_set*) set_header;
                }
                break;
        }
        p += ntohs(set_header->length);
    }


	if (rbuffer_write (config->in_queue, msg, 1) != 0) {
		VERBOSE (CL_VERBOSE_BASIC, "Unable to write into Data manager's input queue, skipping data.");
		free (packet);
	}
}
