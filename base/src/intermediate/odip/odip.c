/**
 * \file joinflows_ip.c
 * \author Michal Kozubik <kozubik.michal@gmail.com>
 * \brief Intermediate Process that is able to join multiple flows
 * into the one.
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

/**
 * \defgroup odipInter ODIP Intermediate Process
 * \ingroup intermediatePlugins
 *
 * This plugin adds information about exporter's IP address into each 
 * data record (if it does not contain it yet). Supported are both IPv4 and IPv6.
 *
 * @{
 */

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include <ipfixcol.h>

/* API version constant */
IPFIXCOL_API_VERSION;

#define ODIP4_FIELD  403
#define ODIP6_FIELD  404
#define ODIP4_LENGTH 4
#define ODIP6_LENGTH 16

/* module name for MSG_* */
static const char *msg_module = "odip";

/* plugin's configuration structure */
struct odip_ip_config {
	char *params;                  /* input parameters */
	void *ip_config;               /* internal process configuration */
	uint32_t ip_id;                /* source ID for Template Manager */
	struct ipfix_template_mgr *tm; /* Template Manager */
};

/* structure for processing message */
struct odip_processor {
	uint8_t *msg;
	struct metadata *metadata;
	uint16_t metadata_index;
	uint16_t offset, length;
	struct input_info_network *info;
	int type;
	uint8_t add_orig_odip;
	struct ipfix_template_key key;
	struct ipfix_template_mgr *tm;
};

/**
 * \brief Initialize joinflows plugin
 *
 * \param[in] params Plugin parameters
 * \param[in] ip_config Internal process configuration
 * \param[in] ip_id Source ID into Template Manager
 * \param[in] template_mgr Template Manager
 * \param[out] config Plugin configuration
 * \return 0 if everything OK
 */
int intermediate_init(char *params, void *ip_config, uint32_t ip_id, struct ipfix_template_mgr *template_mgr, void **config)
{
	(void) params;
	struct odip_ip_config *conf;
	conf = (struct odip_ip_config *) calloc(1, sizeof(*conf));
	if (!conf) {
		MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)", __FILE__, __LINE__);
		return -1;
	}

	conf->ip_id = ip_id;
	conf->ip_config = ip_config;
	conf->tm = template_mgr;
	
	*config = conf;
	MSG_NOTICE(msg_module, "Plugin initialization completed successfully");
	return 0;
}

/**
 * \brief Function to process each template record
 * 
 * @param rec template record
 * @param rec_len length of record
 * @param data processing data
 */
void templates_processor(uint8_t *rec, int rec_len, void *data)
{
	struct ipfix_template_record *record = (struct ipfix_template_record *) rec;
	struct odip_processor *proc = (struct odip_processor *) data;
	
	/* which version of ip should be added */
	int add = 0;
	if (proc->info->l3_proto == 4 && template_record_get_field(record, 0, ODIP4_FIELD, NULL) == NULL) {
		add = 4;
	} else if (proc->info->l3_proto == 6 && template_record_get_field(record, 0, ODIP6_FIELD, NULL) == NULL) {
		add = 6;
	}
	
	struct ipfix_template_record *new_rec = (struct ipfix_template_record *) (proc->msg + proc->offset);
	
	/* copy record to new msg */
	memcpy(proc->msg + proc->offset, rec, rec_len);
	proc->offset += rec_len;
	proc->length += rec_len;
	
	/* add information about source IP (if needed) */
	switch (add) {
		case 4: {
			// MSG_DEBUG(msg_module, "[%u] %d - added info about exporter's source address (IPv4)", proc->info->odid, htons(new_rec->template_id));
			uint16_t odip4_field = htons(ODIP4_FIELD);
			uint16_t odip4_length = htons(ODIP4_LENGTH);
			memcpy(proc->msg + proc->offset, &odip4_field, 2);
			memcpy(proc->msg + proc->offset + 2, &odip4_length, 2);
			proc->offset += 4;
			proc->length += 4;
			new_rec->count = htons(ntohs(new_rec->count) + 1);
			break; }
		case 6: {
			// MSG_DEBUG(msg_module, "[%u] %d - added info about exporter's source address (IPv6)", proc->info->odid, htons(new_rec->template_id));
			uint16_t odip6_field = htons(ODIP6_FIELD);
			uint16_t odip6_length = htons(ODIP6_LENGTH);
			memcpy(proc->msg + proc->offset, &odip6_field, 2);
			memcpy(proc->msg + proc->offset + 2, &odip6_length, 2);
			proc->offset += 4;
			proc->length += 4;
			new_rec->count = htons(ntohs(new_rec->count) + 1);
			add = 4;
			break; }
		default:
			// MSG_DEBUG(msg_module, "[%u] %d - already contains information about exporter's source address", proc->info->odid, htons(new_rec->template_id));
			break;
	}
	
	/* save template into template manager */
	proc->key.tid = ntohs(new_rec->template_id);
	struct ipfix_template *t = tm_add_template(proc->tm, (void *) new_rec, rec_len + add, proc->type, &(proc->key));
	new_rec->template_id = htons(t->template_id);
}

/**
 * \brief Function to process each data record
 * 
 * @param rec data record
 * @param rec_len length of record
 * @param templ record's template
 * @param data processing data
 */
void data_processor(uint8_t *rec, int rec_len, struct ipfix_template *templ, void *data)
{
	struct odip_processor *proc = (struct odip_processor *) data;
	(void) templ;

	/* copy whole data record */
	memcpy(proc->msg + proc->offset, rec, rec_len);
	proc->metadata[proc->metadata_index].record.record = (proc->msg + proc->offset);
	proc->metadata[proc->metadata_index].record.length = rec_len;

	proc->offset += rec_len;
	proc->length += rec_len;

	/* add information about source IP (if needed) */
	if (proc->add_orig_odip) {
		int size;
		if (proc->info->l3_proto == 4) {
			size = sizeof(proc->info->src_addr.ipv4);
		} else {
			size = sizeof(proc->info->src_addr.ipv6);
		}
		
		memcpy(proc->msg + proc->offset, &(proc->info->src_addr), size);
		proc->offset += size;
		proc->length += size;
		proc->metadata[proc->metadata_index].record.length += size;
	}

	proc->metadata_index++;
}

/**
 * \brief Copy informations between templates
 *
 * \param[in] to Destination template
 * \param[in] from Source template
 */
void odip_copy_template_info(struct ipfix_template *to, struct ipfix_template *from)
{
	to->last_message = from->last_message;
	to->last_transmission = from->last_transmission;
}

/**
 * \brief Process IPFIX message
 * 
 * @param config plugin configuration
 * @param message IPFIX message
 * @return 0 on success
 */
int intermediate_process_message(void *config, void *message)
{
	struct odip_ip_config *conf = (struct odip_ip_config *) config;
	struct ipfix_message *msg = (struct ipfix_message *) message;
	struct ipfix_message *new_msg = NULL;
	
	struct input_info_network *info = (struct input_info_network *) msg->input_info;
	struct odip_processor proc;
	int i, new_i = 0, prev_offset, tsets = 0, otsets = 0;
	
	/* source closed or not network source */
	if (msg->source_status == SOURCE_STATUS_CLOSED || msg->input_info->type == SOURCE_TYPE_IPFIX_FILE) {
		pass_message(conf->ip_config, (void *) msg);
		return 0;
	}
	
	/* allocate space for new message */
	if (info->l3_proto == 4) {
		proc.msg = calloc(1, ntohs(msg->pkt_header->length) + 4 * (msg->data_records_count + msg->templ_records_count + msg->opt_templ_records_count));
	} else {
		proc.msg = calloc(1, ntohs(msg->pkt_header->length) + 16 * msg->data_records_count + 4 * (msg->templ_records_count + msg->opt_templ_records_count));
	}
	
	if (!proc.msg) {
		MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)", __FILE__, __LINE__);
		return 1;
	}

	new_msg = calloc(1, sizeof(struct ipfix_message));
	if (!new_msg) {
		MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)", __FILE__, __LINE__);
		free(proc.msg);
		return 1;
	}
	
	/* copy header, initialize processing structure */
	memcpy(proc.msg, msg->pkt_header, IPFIX_HEADER_LENGTH);
	new_msg->pkt_header = (struct ipfix_header *) proc.msg;
	new_msg->metadata = msg->metadata;
	msg->metadata = NULL;

	proc.tm = conf->tm;
	proc.key.crc = conf->ip_id;
	proc.key.odid = info->odid;
	proc.offset = IPFIX_HEADER_LENGTH;
	proc.info = info;
	proc.metadata = new_msg->metadata;
	proc.metadata_index = 0;
	
	/* process template records */
	proc.type = TM_TEMPLATE;
	for (i = 0; i < MSG_MAX_TEMPL_SETS && msg->templ_set[i]; ++i) {
		prev_offset = proc.offset;
		memcpy(proc.msg + proc.offset, &(msg->templ_set[i]->header), 4);
		proc.offset += 4;
		proc.length = 4;

		template_set_process_records(msg->templ_set[i], proc.type, &templates_processor, (void *) &proc);

		if (proc.offset == prev_offset + 4) {
			proc.offset = prev_offset;
		} else {
			new_msg->templ_set[tsets] = (struct ipfix_template_set *) ((uint8_t *) proc.msg + prev_offset);
			new_msg->templ_set[tsets]->header.length = htons(proc.length);
			tsets++;
		}
	}
	
	/* Process option templates */
	proc.type = TM_OPTIONS_TEMPLATE;
	for (i = 0; i < MSG_MAX_OTEMPL_SETS && msg->opt_templ_set[i]; ++i) {
		prev_offset = proc.offset;
		memcpy(proc.msg + proc.offset, &(msg->opt_templ_set[i]->header), 4);
		proc.offset += 4;
		proc.length = 4;

		template_set_process_records((struct ipfix_template_set *) msg->opt_templ_set[i], proc.type, &templates_processor, (void *) &proc);

		if (proc.offset == prev_offset + 4) {
			proc.offset = prev_offset;
		} else {
			new_msg->opt_templ_set[otsets] = (struct ipfix_options_template_set *) ((uint8_t *) proc.msg + prev_offset);
			new_msg->opt_templ_set[otsets]->header.length = htons(proc.length);
			otsets++;
		}
	}
	
	new_msg->templ_set[tsets] = NULL;
	new_msg->opt_templ_set[otsets] = NULL;

	/* Process data records */
	uint16_t metadata_index = 0;
	for (i = 0; i < MSG_MAX_DATA_COUPLES && msg->data_couple[i].data_set; ++i) {
		struct ipfix_template *templ = msg->data_couple[i].data_template;
		if (!templ) {
			continue;
		}

		proc.key.tid = templ->template_id;
		struct ipfix_template *new_templ = tm_get_template(conf->tm, &(proc.key));
		if (new_templ == NULL) {
			MSG_WARNING(msg_module, "[%u] %d not found, something is wrong!", info->odid, templ->template_id);
			continue;
		}

		memcpy(proc.msg + proc.offset, &(msg->data_couple[i].data_set->header), 4);
		proc.offset += 4;
		proc.length = 4;
		if (info->l3_proto == 4) {
			proc.add_orig_odip = (bool) !template_get_field(templ, 0, ODIP4_FIELD, NULL);
		} else {
			proc.add_orig_odip = (bool) !template_get_field(templ, 0, ODIP6_FIELD, NULL);
		}

		new_msg->data_couple[new_i].data_set = ((struct ipfix_data_set *) ((uint8_t *)proc.msg + proc.offset - 4));
		new_msg->data_couple[new_i].data_template = new_templ;

		/* Copy template info and increment reference */
		odip_copy_template_info(new_templ, templ);
		tm_template_reference_inc(new_templ);

		data_set_process_records(msg->data_couple[i].data_set, templ, &data_processor, (void *) &proc);

		new_msg->data_couple[new_i].data_set->header.length = htons(proc.length);
		new_msg->data_couple[new_i].data_set->header.flowset_id = htons(new_msg->data_couple[new_i].data_template->template_id);

		/* Update templates in metadata */
		while (metadata_index < msg->data_records_count
			   && metadata_index < proc.metadata_index
			   && new_msg->metadata[metadata_index].record.templ == templ) {
			new_msg->metadata[metadata_index].record.templ = new_templ;
			metadata_index++;
		}

		/* Move to the next data_couple in new message */
		new_i++;
	}

	new_msg->data_couple[new_i].data_set = NULL;

	new_msg->pkt_header->length = htons(proc.offset);
	new_msg->input_info = msg->input_info;
	new_msg->templ_records_count = msg->templ_records_count;
	new_msg->opt_templ_records_count = msg->opt_templ_records_count;
	new_msg->data_records_count = msg->data_records_count;
	new_msg->source_status = msg->source_status;
	new_msg->live_profile = msg->live_profile;
	new_msg->plugin_id = msg->plugin_id;
	new_msg->plugin_status = msg->plugin_status;

	drop_message(conf->ip_config, message);
	pass_message(conf->ip_config, (void *) new_msg);
	return 0;
}

/**
 * \brief Close intermediate plugin
 * 
 * @param config plugin configuration
 * @return 0 on success
 */
int intermediate_close(void *config)
{
	struct odip_ip_config *conf;
	
	conf = (struct odip_ip_config *) config;
	free(conf);

	return 0;
}

/**@}*/
