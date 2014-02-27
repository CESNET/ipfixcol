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
#include <string.h>

#include "preprocessor.h"
#include "data_manager.h"
#include "queues.h"
#include "ipfixcol.h"

/** Identifier to MSG_* macros */
static char *msg_module = "preprocessor";

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
 * \brief List of data manager configurations
 */
static struct data_manager_config *data_mngmts = NULL;

static struct ipfix_template_manager *tm = NULL;

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
 * \brief Fill in udp_info structure when managing UDP input
 *
 * @param[in] input_info 	Input information from input plugin
 * @param[in,out] udp_conf 	UDP template configuration structure to be filled in
 * @return void
 */
static void preprocessor_udp_init (struct input_info_network *input_info, struct udp_conf *udp_conf) {
	if (input_info->type == SOURCE_TYPE_UDP) {
		if (((struct input_info_network*) input_info)->template_life_time != NULL) {
			udp_conf->template_life_time = atoi(((struct input_info_network*) input_info)->template_life_time);
		} else {
			udp_conf->template_life_time = TM_UDP_TIMEOUT;
		}
		if (input_info->template_life_packet != NULL) {
			udp_conf->template_life_packet = atoi(input_info->template_life_packet);
		} else {
			udp_conf->template_life_packet = 0;
		}
		if (input_info->options_template_life_time != NULL) {
			udp_conf->options_template_life_time = atoi(((struct input_info_network*) input_info)->options_template_life_time);
		} else {
			udp_conf->options_template_life_time = TM_UDP_TIMEOUT;
		}
		if (input_info->options_template_life_packet != NULL) {
			udp_conf->options_template_life_packet = atoi(input_info->options_template_life_packet);
		} else {
			udp_conf->options_template_life_packet = 0;
		}
	}
}

/**
 * \brief Process one template from template set
 *
 * \param[in] tm   template manager
 * \param[in] tmpl template
 * \param[in] max_len maximal length of this template
 * \param[in] type type of the template
 * \param[in] msg_counter message counter
 * \param[in] input_info input info structure
 * \return length of the template
 */
static int preprocessor_process_one_template(struct ipfix_template_mgr *tm, void *tmpl, int max_len, int type, uint32_t msg_counter, struct input_info *input_info)
{
	struct ipfix_template_record *template_record;
	struct ipfix_template *template;
	int ret;

	template_record = (struct ipfix_template_record*) tmpl;
	struct ipfix_template_key key;
	key.odid = 0;
	key.crc = 0;
	key.tid = ntohs(template_record->template_id);

	/* check for withdraw all templates message */
	/* these templates are no longer used (checked in data_manager_withdraw_templates()) */
	if (input_info->type == SOURCE_TYPE_UDP && ntohs(template_record->count) == 0) {
		/* got withdrawal message with UDP -> this is wrong */
		MSG_WARNING(msg_module, "Got template withdraw message on UDP. Ignoring.");
		return TM_TEMPLATE_WITHDRAW_LEN;
	} else if ((ntohs(template_record->template_id) == IPFIX_TEMPLATE_FLOWSET_ID ||
				ntohs(template_record->template_id) == IPFIX_OPTION_FLOWSET_ID) &&
			ntohs(template_record->count) == 0) {
		/* withdraw template or option template */
		tm_remove_all_templates(tm, type);
		/* don't try to parse the withdraw template */
		return TM_TEMPLATE_WITHDRAW_LEN;
		/* check for withdraw template message */
	} else if (ntohs(template_record->count) == 0) {
		ret = tm_remove_template(tm, &key);
		/* Log error when removing unknown template */
		if (ret == 1) {
			MSG_WARNING(msg_module, "%s withdraw message received for unknown Template ID: %u",
					(type==TM_TEMPLATE)?"Template":"Options template", ntohs(template_record->template_id));
		}
		return TM_TEMPLATE_WITHDRAW_LEN;
		/* check whether template exists */
	} else if ((template = tm_get_template(tm, &key)) == NULL) {
		/* add template */
		/* check that the template has valid ID ( < 256 ) */
		if (ntohs(template_record->template_id) < 256) {
			MSG_WARNING(msg_module, "%s ID %i is reserved and not valid for data set!", (type==TM_TEMPLATE)?"Template":"Options template", ntohs(template_record->template_id));
		} else {
			MSG_NOTICE(msg_module, "New %s ID %i", (type==TM_TEMPLATE)?"template":"options template", ntohs(template_record->template_id));
			template = tm_add_template(tm, tmpl, max_len, type, &key);
		}
	} else {
		/* template already exists */
		MSG_WARNING(msg_module, "%s ID %i already exists. Rewriting.",
				(type==TM_TEMPLATE)?"Template":"Options template", template->template_id);
		template = tm_update_template(tm, tmpl, max_len, type, &key);
	}
	if (template == NULL) {
		MSG_WARNING(msg_module, "Cannot parse %s set, skipping to next set",
				(type==TM_TEMPLATE)?"template":"options template");
		return 0;
		/* update UDP timeouts */
	} else if (input_info->type == SOURCE_TYPE_UDP) {
		template->last_message = msg_counter;
		template->last_transmission = time(NULL);
	}

	/* return the length of the original template:
	 * = template length - template header length + template record header length */
	if (type == TM_TEMPLATE) {
		return template->template_length - sizeof(struct ipfix_template) + sizeof(struct ipfix_template_record);
	}

	/* length of the options template */
	return template->template_length - sizeof(struct ipfix_template) + sizeof(struct ipfix_options_template_record);
}

/**
 * \brief Process templates
 *
 * Currently template management does not conform to RFC 5101 in following:
 * - If template is reused without previous withdrawal or timeout (UDP),
 *   only warning is logged and template is updated (template MUST be of
 *   the same length).
 * - If template is not found, data is not coupled with template,
 *   i.e. data_set[x]->template == NULL
 * - When template is malformed and cannot be added to template manager,
 *   rest of the template set is discarded (template length cannot be
 *   determined)
 *
 * @param[in,out] template_mgr	Template manager
 * @param[in] msg IPFIX			message
 * @param[in] udp_conf			UDP template configuration
 * @return uint32_t Number of received data records
 */
static uint32_t preprocessor_process_templates(struct ipfix_template_mgr *template_mgr, struct ipfix_message *msg, struct udp_conf *udp_conf)
{
	uint8_t *ptr;
	uint32_t records_count = 0;
	int i, ret;
	int max_len;     /* length to the end of the set = max length of the template */
	uint16_t count;
	uint16_t length = 0;
	uint16_t offset = 0;
	uint8_t var_len = 0;
	struct ipfix_template *template;
	uint16_t min_data_length;
	uint16_t data_length;
	uint32_t msg_counter = 0;

	struct ipfix_template_key key;
	key.odid = 0;
	key.crc = 0;


	/* check for new templates */
	for (i=0; msg->templ_set[i] != NULL && i<1024; i++) {
		ptr = (uint8_t*) &msg->templ_set[i]->first_record;
		while (ptr < (uint8_t*) msg->templ_set[i] + ntohs(msg->templ_set[i]->header.length)) {
			max_len = ((uint8_t *) msg->templ_set[i] + ntohs(msg->templ_set[i]->header.length)) - ptr;
			ret = preprocessor_process_one_template(template_mgr, ptr, max_len, TM_TEMPLATE, msg_counter, msg->input_info);
			if (ret == 0) {
				break;
			} else {
				ptr += ret;
			}
		}
	}

	/* check for new option templates */
	for (i=0; msg->opt_templ_set[i] != NULL && i<1024; i++) {
		ptr = (uint8_t*) &msg->opt_templ_set[i]->first_record;
		max_len = ((uint8_t *) msg->opt_templ_set[i] + ntohs(msg->opt_templ_set[i]->header.length)) - ptr;
		while (ptr < (uint8_t*) msg->opt_templ_set[i] + ntohs(msg->opt_templ_set[i]->header.length)) {
			ret = preprocessor_process_one_template(template_mgr, ptr, max_len, TM_OPTIONS_TEMPLATE, msg_counter, msg->input_info);
			if (ret == 0) {
				break;
			} else {
				ptr += ret;
			}
		}
	}

	/* add template to message data_couples */
	for (i=0; msg->data_couple[i].data_set != NULL && i<1023; i++) {
		key.tid = ntohs(msg->data_couple[i].data_set->header.flowset_id);
		msg->data_couple[i].data_template = tm_get_template(template_mgr, &key);
		if (msg->data_couple[i].data_template == NULL) {
			MSG_WARNING(msg_module, "Data template with ID %i not found!", key.tid);
		} else {
			/* Increasing number of references to template */
			msg->data_couple[i].data_template->references++;

			if ((msg->input_info->type == SOURCE_TYPE_UDP) && /* source UDP */
					((time(NULL) - msg->data_couple[i].data_template->last_transmission > udp_conf->template_life_time) || /* lifetime expired */
					(udp_conf->template_life_packet > 0 && /* life packet should be checked */
					(uint32_t) (msg_counter - msg->data_couple[i].data_template->last_message) > udp_conf->template_life_packet))) {
				MSG_WARNING(msg_module, "Data template ID %i expired! Using old template.",
				                                               msg->data_couple[i].data_template->template_id);
			}

			/* compute sequence number */
			if (msg->data_couple[i].data_template->data_length & 0x80000000) {
				/* damn... there is a Information Element with variable length. we have to
				 * compute number of the Data Records in current Set by hand */

				/* template for current Data Set */
				template = msg->data_couple[i].data_template;
				/* every Data Record has to be at least this long */
				min_data_length = (uint16_t) msg->data_couple[i].data_template->data_length;

				data_length = ntohs(msg->data_couple[i].data_set->header.length) - sizeof(struct ipfix_set_header);


				length = 0;   /* position in Data Record */
				while (data_length - length >= min_data_length) {
					offset = 0;
					for (count = 0; count < template->field_count; count++) {
						if (template->fields[offset].ie.length == 0xffff) {
							/* this element has variable length. read first byte from actual data record to determine
							 * real length of the field */
							var_len = msg->data_couple[i].data_set->records[length];
							if (var_len < 255) {
								/* field length is var_len */
								length += var_len;
								length += 1; /* first 1 byte contains information about field length */
							} else {
								/* field length is more than 255, actual length is stored in next two bytes */
								length += ntohs(*((uint16_t *) (msg->data_couple[i].data_set->records + length + 1)));
								length += 3; /* first 3 bytes contain information about field length */
							}
						} else {
							/* length from template */
							length += template->fields[offset].ie.length;
						}

						if (template->fields[offset].ie.id & 0x8000) {
							/* do not forget on Enterprise Number */
							offset += 1;
						}
						offset += 1;
					}

					/* data record found */
					records_count += 1;
				}
			} else {
				/* no elements with variable length */
				records_count += ntohs(msg->data_couple[i].data_set->header.length) / msg->data_couple[i].data_template->data_length;
			}
		}
	}

	/* return number of data records */
	return records_count;
}

void preprocessor_parse_templates(struct ipfix_message* msg, struct ipfix_template_manager *tm, struct input_info *input_info)
{
	/* Check whether there are withdraw templates (not for UDP) */
//	if (input_info->type != SOURCE_TYPE_UDP &&  data_manager_withdraw_templates(msg)) {
//		MSG_NOTICE(msg_module, "Got template withdrawal message. Waiting for storage packets.");
//		/* wait for storage plugins to consume all pending messages */
//		while (rbuffer_wait_empty(config->store_queue));
//	}

	/* process templates */
//	sequence_number += data_manager_process_templates(config->template_mgr, msg, &udp_conf, msg_counter);
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

void preprocessor_parse_msg (void* packet, int len, struct input_info* input_info, struct storage_list* storage_plugins)
{
	struct ipfix_message* msg;
	struct data_manager_config *config = NULL, *prev_config = NULL;

	if (input_info == NULL || storage_plugins == NULL) {
		MSG_WARNING(msg_module, "Invalid parameters in function preprocessor_parse_msg().");
		return;
	}
	struct ipfix_template_key key;
	key.odid = 0;
	key.crc = 0;

	/* connection closed, close data manager */
    if (packet == NULL) {
        config = get_data_mngmt_by_input_info (input_info, &prev_config);

        if (!config) {
        	MSG_WARNING(msg_module, "Data manager NOT found, probably more exporters with same OID.");
        	return;
        }
        /* remove data manager from the list */
        if (prev_config == NULL) {
        	data_mngmts = config->next;
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
	MSG_DEBUG(msg_module, "Processing data for Observation domain ID %d.",
			ntohl(msg->pkt_header->observation_domain_id));

	/* check IPFIX version */
	if (msg->pkt_header->version != htons(IPFIX_VERSION)) {
		MSG_WARNING(msg_module, "Unexpected IPFIX version detected (%X), skipping packet.",
				msg->pkt_header->version);
		free (msg);
		free (packet);
		return;
	}

	/* check whether message is not shorter than header says */
	if ((uint16_t) len < ntohs(msg->pkt_header->length)) {
		MSG_WARNING(msg_module, "Malformed IPFIX message detected (bad length), skipping packet.");
		free (msg);
		free (packet);
		return;
	}

	/* get appropriate data manager's config according to Observation domain ID */
	config = get_data_mngmt_config (ntohl(msg->pkt_header->observation_domain_id));
	if (config == NULL) {
		/*
		 * no data manager config for this observation domain ID found -
		 * we have a new observation domain ID, so create new data manager for
		 * it
		 */
		config = data_manager_create (ntohl(msg->pkt_header->observation_domain_id), storage_plugins, input_info);
		if (config == NULL) {
			MSG_WARNING(msg_module, "Unable to create data manager for Observation Domain ID %d, skipping data.",
					ntohl(msg->pkt_header->observation_domain_id));
			free (msg);
			/* free packet here, it was not passed anywhere */
			free (packet);
			return;
		}

	    /* add config to data_mngmts structure */
    	config->next = data_mngmts;
        data_mngmts = config;

        MSG_NOTICE(msg_module, "Created new Data manager for ODID %i", ntohl(msg->pkt_header->observation_domain_id));
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
                if (ntohs(set_header->flowset_id) < IPFIX_MIN_RECORD_FLOWSET_ID) {
                	MSG_WARNING(msg_module, "Unknown Set ID %d", ntohs(set_header->flowset_id));
                } else {
                    msg->data_couple[d_set_count++].data_set = (struct ipfix_data_set*) set_header;
                }
                break;
        }

        /* if length is wrong and pointer does not move, stop processing the message */
        if (ntohs(set_header->length) == 0) {
        	break;
        }

        p += ntohs(set_header->length);
    }

    if (!tm) {
    	tm = tm_create();
    	if (tm == NULL) {
    		MSG_ERROR(msg_module, "Unable to create Template Manager");
    		return;
    	}
    }

    struct udp_conf udp_conf;
    preprocessor_udp_init((struct input_info_network*) input_info, &udp_conf);
    preprocessor_process_templates(tm, msg, &udp_conf);

	if (rbuffer_write (config->in_queue, msg, 1) != 0) {
		MSG_WARNING(msg_module, "Unable to write into Data manager's input queue, skipping data.");
		free (packet);
	}
}
