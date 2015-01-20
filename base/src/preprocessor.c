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
#include <ipfixcol.h>
#include "crc.h"

/** Identifier to MSG_* macros */
static char *msg_module = "preprocessor";

static struct ring_buffer *preprocessor_out_queue = NULL;
struct ipfix_template_mgr *tm = NULL;

/* Sequence number counter for each ODID */
struct odid_info {
	uint32_t odid, sequence_number;
	uint32_t free_tid;
	int sources;
	struct odid_info *next;
};

struct odid_info *odid_info = NULL;

/**
 * \brief Get sequence number counter for given ODID
 * \param[in] odid Observation Domain ID
 * \return Pointer to sequence number counter
 */
struct odid_info *odid_info_get(uint32_t odid)
{
	struct odid_info *aux_info = odid_info;
	while (aux_info) {
		if (aux_info->odid == odid) {
			return aux_info;
		}

		aux_info = aux_info->next;
	}
	return NULL;
}

/**
 * \brief Add new SN counter
 * \param[in] odid Observation Domain ID
 * \return Pointer to sequence number counter
 */
struct odid_info *odid_info_add(uint32_t odid)
{
	struct odid_info *aux_info;

	aux_info = calloc(1, sizeof(struct odid_info));
	if (!aux_info) {
		MSG_ERROR(msg_module, "Not enought memory (%s:%d)", __FILE__, __LINE__);
		return NULL;
	}
	aux_info->odid = odid;
	aux_info->sources = 1;
	aux_info->free_tid = 256;

	if (!odid_info) {
		odid_info = aux_info;
	} else {
		aux_info->next = odid_info->next;
		odid_info->next = aux_info;
	}

	return aux_info;
}

/**
 * \brief Add new source for SN counter
 * \param[in] odid Observation Domain ID
 * \return Pointer to sequence number counter
 */
struct odid_info *odid_info_add_source(uint32_t odid)
{
	struct odid_info *aux_info = odid_info_get(odid);

	if (!aux_info) {
		return odid_info_add(odid);
	}

	aux_info->sources++;
	MSG_NOTICE(msg_module, "[%u] Accepted data from %d. source with this ODID", odid, aux_info->sources);
	return aux_info;
}

/**
 * \brief Remove source from SN counter
 * \param[in] odid Observation Domain ID
 */
void odid_info_remove_source(uint32_t odid)
{
	struct odid_info *aux_info = odid_info_get(odid);
	if (!aux_info) {
		return;
	}

	aux_info->sources--;
	if (aux_info->sources <= 0) {
		aux_info->sequence_number = 0;
	}
}

/**
 * \brief Get odid_info struct or add a new one
 * 
 * @param odid ODID
 * @return odid_info
 */
struct odid_info *odid_info_get_or_add(uint32_t odid)
{
	struct odid_info *aux_info = odid_info_get(odid);
	if (!aux_info) {
		aux_info = odid_info_add(odid);
	}
	
	return aux_info;
}

/**
 * \brief Get sequence number for given ODID
 * \param[in] odid Observation Domain ID
 * \return Pointer to sequence number value
 */
uint32_t *odid_info_get_sequence_number(uint32_t odid)
{
	struct odid_info *aux_info = odid_info_get_or_add(odid);
	if (!aux_info) {
		return NULL;
	}

	return &(aux_info->sequence_number);
}

/**
 * \brief Get free template ID
 * 
 * @param odid ODID
 * @return template id
 */
uint32_t odid_info_get_free_tid(uint32_t odid)
{
	struct odid_info *aux_info = odid_info_get_or_add(odid);
	if (!aux_info) {
		return 256;
	}
	
	return aux_info->free_tid++;
}

/**
 * \brief Remove all counters
 */
void odid_info_destroy()
{
	struct odid_info *aux_info = odid_info;
	while (aux_info) {
		odid_info = odid_info->next;
		free(aux_info);
		aux_info = odid_info;
	}
}

/**
 * \brief This function sets the queue for preprocessor and inits crc computing.
 *
 * @param out_queue preprocessor's output queue
 * @return 0 on success, negative value otherwise
 */
int preprocessor_init(struct ring_buffer *out_queue, struct ipfix_template_mgr *template_mgr)
{
	if (preprocessor_out_queue) {
		MSG_WARNING(msg_module, "Redefining preprocessor's output queue.");
	}

	/* Set output queue */
	preprocessor_out_queue = out_queue;

	tm = template_mgr;

	return 0;
}

/**
 * \brief Returns pointer to preprocessors output queue.
 *
 * @return preprocessors output queue
 */
struct ring_buffer *get_preprocessor_output_queue()
{
	return preprocessor_out_queue;
}

/**
 * \brief Compute 32b CRC from input informations 
 * 
 * @param input_info Input informations
 * @return crc32
 */
uint32_t preprocessor_compute_crc(struct input_info *input_info)
{
	if (input_info->type == SOURCE_TYPE_IPFIX_FILE) {
		struct input_info_file *input_file = (struct input_info_file *) input_info;
		return crc32(input_file->name, strlen(input_file->name));
	}
	
	struct input_info_network *input = (struct input_info_network *) input_info;

	char buff[35];
	uint8_t size;
	if (input->l3_proto == 6) { /* IPv6 */
		memcpy(buff, &(input->src_addr.ipv6.__in6_u.__u6_addr8), 32);
		size = 34;
	} else { /* IPv4 */
		memcpy(buff, &(input->src_addr.ipv4.s_addr), 8);
		size = 10;
	}
	memcpy(buff + size - 2, &(input->src_port), 2);
	buff[size] = '\0';

	return crc32(buff, size);
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
 * \param[in] key template key with filled crc and odid
 * \return length of the template
 */
static int preprocessor_process_one_template(struct ipfix_template_mgr *tm, void *tmpl, int max_len, int type, 
	uint32_t msg_counter, struct input_info *input_info, struct ipfix_template_key *key)
{
	struct ipfix_template_record *template_record;
	struct ipfix_template *template;
	int ret;

	template_record = (struct ipfix_template_record*) tmpl;
	
	key->tid = ntohs(template_record->template_id);

	/* check for withdraw all templates message */
	/* these templates are no longer used (checked in data_manager_withdraw_templates()) */
	if (input_info->type == SOURCE_TYPE_UDP && ntohs(template_record->count) == 0) {
		/* got withdrawal message with UDP -> this is wrong */
		MSG_WARNING(msg_module, "[%u] Got template withdraw message on UDP. Ignoring.", input_info->odid);
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
		ret = tm_remove_template(tm, key);
		MSG_NOTICE(msg_module, "[%u] Got %s withdraw message.", input_info->odid, (type==TM_TEMPLATE)?"Template":"Options template");
		/* Log error when removing unknown template */
		if (ret == 1) {
			MSG_WARNING(msg_module, "[%u] %s withdraw message received for unknown Template ID: %u", input_info->odid,
					(type==TM_TEMPLATE)?"Template":"Options template", ntohs(template_record->template_id));
		}
		return TM_TEMPLATE_WITHDRAW_LEN;
		/* check whether template exists */
	} else if ((template = tm_get_template(tm, key)) == NULL) {
		/* add template */
		/* check that the template has valid ID ( < 256 ) */
		if (ntohs(template_record->template_id) < 256) {
			MSG_WARNING(msg_module, "[%u] %s ID %i is reserved and not valid for data set!", key->odid, (type==TM_TEMPLATE)?"Template":"Options template", ntohs(template_record->template_id));
		} else {
			MSG_NOTICE(msg_module, "[%u] New %s ID %i", key->odid, (type==TM_TEMPLATE)?"template":"options template", ntohs(template_record->template_id));
			template = tm_add_template(tm, tmpl, max_len, type, key);
			/* Set new template ID according to ODID */
			if (template) {
				template->template_id = odid_info_get_free_tid(key->odid);
			}
		}
	} else {
		/* template already exists */
		MSG_WARNING(msg_module, "[%u] %s ID %i already exists. Rewriting.", key->odid,
				(type==TM_TEMPLATE)?"Template":"Options template", template->template_id);
		template = tm_update_template(tm, tmpl, max_len, type, key);
	}
	if (template == NULL) {
		MSG_WARNING(msg_module, "[%u] Cannot parse %s set, skipping to next set", key->odid,
				(type==TM_TEMPLATE)?"template":"options template");
		return 0;
		/* update UDP timeouts */
	} else if (input_info->type == SOURCE_TYPE_UDP) {
		template->last_message = msg_counter;
		template->last_transmission = time(NULL);
	}
	
	/* Set new template id to original template record */
	template_record->template_id = htons(template->template_id);

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
 * @return uint32_t Number of received data records
 */
static uint32_t preprocessor_process_templates(struct ipfix_template_mgr *template_mgr, struct ipfix_message *msg)
{
	uint8_t *ptr;
	uint32_t records_count = 0;
	int i, ret;
	int max_len;     /* length to the end of the set = max length of the template */
	uint32_t msg_counter = 0;

	struct udp_conf udp_conf = {0};
	struct ipfix_template_key key;
	
	msg->data_records_count = msg->templ_records_count = msg->opt_templ_records_count = 0;

	key.odid = ntohl(msg->pkt_header->observation_domain_id);
	key.crc = preprocessor_compute_crc(msg->input_info);

	preprocessor_udp_init((struct input_info_network *) msg->input_info, &udp_conf);

	/* check for new templates */
	for (i = 0; i < MSG_MAX_TEMPLATES && msg->templ_set[i]; i++) {
		ptr = (uint8_t*) &msg->templ_set[i]->first_record;
		while (ptr < (uint8_t*) msg->templ_set[i] + ntohs(msg->templ_set[i]->header.length)) {
			max_len = ((uint8_t *) msg->templ_set[i] + ntohs(msg->templ_set[i]->header.length)) - ptr;
			ret = preprocessor_process_one_template(template_mgr, ptr, max_len, TM_TEMPLATE, msg_counter, msg->input_info, &key);
			if (ret == 0) {
				break;
			} else {
				msg->templ_records_count++;
				ptr += ret;
			}
		}
	}

	/* check for new option templates */
	for (i = 0; i < MSG_MAX_OTEMPLATES && msg->opt_templ_set[i]; i++) {
		ptr = (uint8_t*) &msg->opt_templ_set[i]->first_record;
		max_len = ((uint8_t *) msg->opt_templ_set[i] + ntohs(msg->opt_templ_set[i]->header.length)) - ptr;
		while (ptr < (uint8_t*) msg->opt_templ_set[i] + ntohs(msg->opt_templ_set[i]->header.length)) {
			ret = preprocessor_process_one_template(template_mgr, ptr, max_len, TM_OPTIONS_TEMPLATE, msg_counter, msg->input_info, &key);
			if (ret == 0) {
				break;
			} else {
				msg->opt_templ_records_count++;
				ptr += ret;
			}
		}
	}

	/* add template to message data_couples */
	for (i = 0; i < MSG_MAX_DATA_COUPLES && msg->data_couple[i].data_set; i++) {
		key.tid = ntohs(msg->data_couple[i].data_set->header.flowset_id);
		msg->data_couple[i].data_template = tm_get_template(template_mgr, &key);
		if (msg->data_couple[i].data_template == NULL) {
			MSG_WARNING(msg_module, "[%u] Data template with ID %i not found!", key.odid, key.tid);
		} else {
			/* Increasing number of references to template */
			tm_template_reference_inc(msg->data_couple[i].data_template);

			/* Set right flowset ID */
			msg->data_couple[i].data_set->header.flowset_id = htons(msg->data_couple[i].data_template->template_id);

			if ((msg->input_info->type == SOURCE_TYPE_UDP) && /* source UDP */
					((time(NULL) - msg->data_couple[i].data_template->last_transmission > udp_conf.template_life_time) || /* lifetime expired */
					(udp_conf.template_life_packet > 0 && /* life packet should be checked */
					(uint32_t) (msg_counter - msg->data_couple[i].data_template->last_message) > udp_conf.template_life_packet))) {
				MSG_WARNING(msg_module, "[%u] Data template ID %i expired! Using old template.", key.odid,
				                                               msg->data_couple[i].data_template->template_id);
			}

			/* compute sequence number */
			records_count += data_set_records_count(msg->data_couple[i].data_set, msg->data_couple[i].data_template);
		}
	}

	msg->data_records_count = records_count;

	/* return number of data records */
	return records_count;
}

/**
 * \brief Parse IPFIX message and send it to intermediate plugin or output managers queue
 * 
 * @param packet Received data from input plugins
 * @param len Packet length
 * @param input_info Input informations about source etc.
 * @param storage_plugins List of storage plugins
 * @param source_status Status of source (new, opened, closed)
 */
void preprocessor_parse_msg (void* packet, int len, struct input_info* input_info, struct storage_list* storage_plugins, int source_status)
{
	struct ipfix_message* msg;
	uint32_t *seqn;

	if (source_status == SOURCE_STATUS_CLOSED) {
		/* Inform intermediate plugins and output manager about closed input */
		msg = calloc(1, sizeof(struct ipfix_message));
		msg->input_info = input_info;
		msg->source_status = source_status;
		odid_info_remove_source(input_info->odid);
	} else {
		if (input_info == NULL || storage_plugins == NULL) {
			MSG_WARNING(msg_module, "Invalid parameters in function preprocessor_parse_msg()");

			if (packet) {
				free(packet);
			}
			
			packet = NULL;
			return;
		}

		if (packet == NULL) {
			MSG_WARNING(msg_module, "[%u] Received empty packet", input_info->odid);
			return;
		}

		/* process IPFIX packet and fill up the ipfix_message structure */
		msg = message_create_from_mem(packet, len, input_info, source_status);
		if (!msg) {
			free(packet);
			packet = NULL;
			return;
		}

		if (source_status == SOURCE_STATUS_NEW) {
			odid_info_add_source(ntohl(msg->pkt_header->observation_domain_id));
		}

		/* Process templates and correct sequence number */
		preprocessor_process_templates(tm, msg);

		seqn = odid_info_get_sequence_number(ntohl(msg->pkt_header->observation_domain_id));

		if (msg->input_info->sequence_number != ntohl(msg->pkt_header->sequence_number) && msg->data_records_count > 0) {
			if (!skip_seq_err) {
				MSG_WARNING(msg_module, "[%u] Sequence number does not match: expected %u, got %u", input_info->odid, msg->input_info->sequence_number , ntohl(msg->pkt_header->sequence_number));
			}

			*seqn += (ntohl(msg->pkt_header->sequence_number) - msg->input_info->sequence_number);
			msg->input_info->sequence_number = ntohl(msg->pkt_header->sequence_number);
		}

		msg->pkt_header->sequence_number = htonl(*seqn);

		msg->input_info->sequence_number += msg->data_records_count;
		*seqn += msg->data_records_count;
	}

    /* Send data to the first intermediate plugin */
	if (rbuffer_write(preprocessor_out_queue, msg, 1) != 0) {
		MSG_WARNING(msg_module, "[%u] Unable to write into Data manager's input queue, skipping data.", input_info->odid);
		message_free(msg);
		packet = NULL;
	}
}

void preprocessor_close()
{
	/* output queue will be closed by intermediate process or output manager */
	odid_info_destroy();
	return;
}
