/**
 * \file proxy.c
 * \author Michal Kozubik <michal.kozubik@cesnet.cz>
 * \brief Storage plugin that forwards data to network
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

#include <ipfixcol.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <ipfixcol/ipfix_message.h>

#include "forwarding.h"

/* API version constant */
IPFIXCOL_API_VERSION;

/* Identifier for MSG_* */
static const char *msg_module = "forwarding storage";

/**
 * \brief Get index to array of template record for given Template ID
 * \param[in] conf Plugin configuration
 * \param[in] tid Template ID
 * \param[in] type Template type
 * \return Index on success, -1 otherwise
 */
int forwarding_get_record_index(forwarding *conf, uint16_t tid, int type)
{
	int i, c;
	for (i = 0, c = 0; i < conf->records_max && c < conf->records_cnt; ++i) {
		if (conf->records[i] != NULL) {
			c++;
			if (conf->records[i]->record->template_id == tid && conf->records[i]->type == type) {
				return i;
			}
		}
	}

	return -1;
}

/**
 * \brief Get template record with given Template ID
 * \param[in] conf Plugin configuration
 * \param[in] tid Template ID
 * \param[in] type Template type
 * \return Pointer to template
 */
struct forwarding_template_record *forwarding_get_record(forwarding *conf, uint16_t tid, int type)
{
	int i = forwarding_get_record_index(conf, tid, type);
	if (i < 0) {
		return NULL;
	}
	return conf->records[i];
}

/**
 * \brief Remove template record with givet Template ID from array
 * \param[in] conf Plugin configuration
 * \param[in] tid Template ID
 * \param[in] type Template type
 */
void forwarding_remove_record(forwarding *conf, uint32_t tid, int type)
{
	int i = forwarding_get_record_index(conf, tid, type);
	if (i >= 0) {
		free(conf->records[i]);
		conf->records[i] = NULL;
		conf->records_cnt--;
	}
}

/**
 * \brief Add new template record into array
 * \param[in] conf Plugin configuration
 * \param[in] record New template record
 * \param[in] type Template type
 * \param[in] len Template records length
 * \return Pointer to inserted template record
 */
struct forwarding_template_record *forwarding_add_record(forwarding *conf, struct ipfix_template_record *record, int type, int len)
{
	int i;
	MSG_DEBUG(msg_module, "%d adding", ntohs(record->template_id));
	if (conf->records_cnt == conf->records_max) {
		/* Array is full, need more memory */
		conf->records_max += 32;
		conf->records = realloc(conf->records, conf->records_max * sizeof(struct forwarding_template_record *));
		if (conf->records == NULL) {
			MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)", __FILE__, __LINE__);
			return NULL;
		}

		/* Initialize new pointers */
		for (i = conf->records_cnt; i < conf->records_max; ++i) {
			conf->records[i] = NULL;
		}
	}

	/* Find place for template */
	for (i = 0; i < conf->records_max; ++i) {
		if (conf->records[i] == NULL) {
			conf->records[i] = calloc(1, sizeof(struct forwarding_template_record));
			if (conf->records[i] == NULL) {
				MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)", __FILE__, __LINE__);
				return NULL;
			}

			conf->records[i]->record = record;
			conf->records[i]->type = type;
			conf->records[i]->length = len;
			conf->records_cnt++;
			return conf->records[i];
		}
	}

	return NULL;
}

/**
 * \brief Initialize template records array
 * \param[in] conf Plugin configuration
 * \return 0 on success
 */
int forwarding_init_records(forwarding *conf)
{
	conf->records_cnt = 0;
	conf->records_max = 32;

	conf->records = calloc(conf->records_max, sizeof(struct forwarding_template_record *));
	if (conf->records== NULL) {
		return 1;
	}

	return 0;
}

const char *get_default_port(forwarding *conf)
{
	return (conf->default_port != NULL) ? conf->default_port : DEFAULT_PORT;
}

const char *get_default_protocol(forwarding *conf)
{
	return (conf->default_protocol != NULL) ? conf->default_protocol : DEFAULT_PROTOCOL;
}

sisoconf *create_sender(forwarding *conf, const char *ip, const char *port)
{
	if (ip == NULL) {
		MSG_ERROR(msg_module, "IP address not specified");
		return NULL;
	}

	if (port == NULL) {
		port = get_default_port(conf);
	}

	const char *proto = get_default_protocol(conf);

	sisoconf *sender = siso_create();
	if (sender == NULL) {
		MSG_ERROR(msg_module, "Memory error - cannot create sender object");
		return NULL;
	}

	if (siso_create_connection(sender, ip, port, proto) != SISO_OK) {
		MSG_ERROR(msg_module, "%s", siso_get_last_err(sender));
		free(sender);
		return NULL;
	}

	return sender;
}

sisoconf *add_sender(forwarding *conf, sisoconf *sender)
{
	if (conf->senders_cnt == conf->senders_max) {
		conf->senders_max = (conf->senders_max == 0) ? 10 : conf->senders_max * 2;
		conf->senders = realloc(conf->senders, sizeof(sisoconf*) * conf->senders_max);
		if (conf->senders == NULL) {
			MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)", __FILE__, __LINE__);
			return NULL;
		}
	}

	conf->senders[conf->senders_cnt++] = sender;
	return sender;
}

sisoconf *process_destination(forwarding *conf, xmlDoc *doc, xmlNodePtr node)
{
	const char *ip = NULL, *port = NULL;
	for (; node != NULL; node = node->next) {
		if (!xmlStrcmp(node->name, (const xmlChar *) "ip")) {
			if (ip) {
				free((void *) ip);
			}
			ip = (const char *) xmlNodeListGetString(doc, node->xmlChildrenNode, 1);
		} else if (!xmlStrcmp(node->name, (const xmlChar *) "port")) {
			if (port) {
				free((void *) port);
			}
			port = (const char *) xmlNodeListGetString(doc, node->xmlChildrenNode, 1);
		}
	}

	sisoconf *sender = create_sender(conf, ip, port);

	if (ip) {
		free((void *) ip);
	}

	if (port) {
		free((void *) port);
	}

	return sender;
}

void load_default_values(forwarding *conf, xmlDoc *doc, xmlNodePtr node)
{
	for (; node != NULL; node = node->next) {
		if (!xmlStrcmp(node->name, (const xmlChar *) "defaultPort")) {
			if (conf->default_port) {
				free((void *) conf->default_port);
			}
			conf->default_port = (const char *) xmlNodeListGetString(doc, node->xmlChildrenNode, 1);
		} else if (!xmlStrcmp(node->name, (const xmlChar *) "protocol")) {
			if (conf->default_protocol) {
				free((void *) conf->default_protocol);
			}
			conf->default_protocol = (const char *) xmlNodeListGetString(doc, node->xmlChildrenNode, 1);
		}
	}

	conf->udp_connection = strcasecmp(get_default_protocol(conf), "UDP") == 0;
	conf->distribution = DT_TO_ALL;
}

/**
 * \brief Initialize configuration by xml file
 * \param[in] conf Plugin configuration
 * \param[in] doc XML document file
 * \param[in] root XML root element
 * \return
 */
bool forwarding_init_conf(forwarding *conf, xmlDoc *doc, xmlNodePtr root)
{
	load_default_values(conf, doc, root->children);

	xmlNodePtr cur = root->children;
	xmlChar *aux_str = NULL;

	while (cur) {
		if (!xmlStrcmp(cur->name, (const xmlChar *) "destination")) {
			sisoconf *sender = process_destination(conf, doc, cur->children);
			if (sender == NULL || add_sender(conf, sender) == NULL) {
				return false;
			}
		} else if (!xmlStrcasecmp(cur->name, (const xmlChar *) "distribution")) {
			aux_str = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			if (!strcasecmp((const char *) aux_str, "RoundRobin")) {
				conf->distribution = DT_ROUND_ROBIN;
			}

		/* UDP configuration */
		} else if (!xmlStrcasecmp(cur->name, (const xmlChar *) "templateLifeTime")) {
			aux_str = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			conf->udp.template_life_time = atoi((char *) aux_str);
		} else if (!xmlStrcasecmp(cur->name, (const xmlChar *) "optionsTemplateLifeTime")) {
			aux_str = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			conf->udp.options_template_life_time = atoi((char *) aux_str);
		} else if (!xmlStrcasecmp(cur->name, (const xmlChar *) "templateLifePacket")) {
			aux_str = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			conf->udp.template_life_packet = atoi((char *) aux_str);
		} else if (!xmlStrcasecmp(cur->name, (const xmlChar *) "optionsTemplateLifePacket")) {
			aux_str = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			conf->udp.options_template_life_packet = atoi((char *) aux_str);

		/* Cases handled by load_default_values; avoid warnings in next if-statement */
		} else if (!xmlStrcasecmp(cur->name, (const xmlChar *) "defaultPort")
				|| !xmlStrcasecmp(cur->name, (const xmlChar *) "protocol")) {
			/* Do nothing */

		/* Report unknown elements */
		} else if (xmlStrcmp(cur->name, (const xmlChar *) "fileFormat")) {
			MSG_WARNING(msg_module, "Unknown element '%s'", cur->name);
		}

		if (aux_str) {
			xmlFree(aux_str);
			aux_str = NULL;
		}

		cur = cur->next;
	}

	if (conf->senders_cnt == 0) {
		MSG_ERROR(msg_module, "No valid destination found");
		return false;
	}

	if (forwarding_init_records(conf)) {
		return false;
	}

	return true;
}

/**
 * \brief Initialize plugin
 * \param[in] params Parameters
 * \param[out] config Plugin configuration
 * \return 0 on success
 */
int storage_init(char *params, void **config)
{
	forwarding *conf = NULL;
	xmlDoc *doc = NULL;
	xmlNodePtr root = NULL;

	MSG_DEBUG(msg_module, "Initialization");

	if (!params) {
		MSG_ERROR(msg_module, "Missing plugin configuration");
		return -1;
	}

	conf = calloc(1, sizeof(forwarding));
	if (!conf) {
		MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)", __FILE__, __LINE__);
		return -1;
	}

	doc = xmlParseDoc(BAD_CAST params);
	if (!doc) {
		MSG_ERROR(msg_module, "Could not parse plugin configuration");
		free(conf);
		return -1;
	}

	root = xmlDocGetRootElement(doc);
	if (!root) {
		MSG_ERROR(msg_module, "Could not get plugin configuration root element");
		free(conf);
		return -1;
	}

	if (xmlStrcmp(root->name, (const xmlChar *) "fileWriter")) {
		MSG_ERROR(msg_module, "Plugin configuration root node != 'fileWriter'");
		goto init_err;
	}

	if (!forwarding_init_conf(conf, doc, root)) {
		goto init_err;
	}

	*config = conf;

	xmlFreeDoc(doc);
	return 0;

init_err:
	xmlFreeDoc(doc);
	free(conf);
	return -1;
}

/**
 * \brief Store everything - flush buffers
 * \param[in] config Plugin configuration
 * \return 0 on success
 */
int store_now(const void *config)
{
	(void) config;
	MSG_DEBUG(msg_module, "Flushing data");
	return 0;
}

/**
 * \brief Conver IPFIX message into packet
 * \param[in] msg IPFIX message structure
 * \param[out] packet_length Length of packet
 * \return pointer to packet
 */
void *msg_to_packet(const struct ipfix_message *msg, int *packet_length)
{
	int i, len, offset = 0;

	*packet_length = ntohs(msg->pkt_header->length);
	void *packet = calloc(1, *packet_length);
	if (!packet) {
		return NULL;
	}

	/* Copy header */
	memcpy(packet, msg->pkt_header, IPFIX_HEADER_LENGTH);
	offset += IPFIX_HEADER_LENGTH;

	/* Copy template sets */
	for (i = 0; i < MSG_MAX_TEMPL_SETS && msg->templ_set[i]; ++i) {
		len = ntohs(msg->templ_set[i]->header.length);
		memcpy(packet + offset, msg->templ_set[i], len);
		offset += len;
	}

	/* Copy option template sets */
	for (i = 0; i < MSG_MAX_OTEMPL_SETS && msg->opt_templ_set[i]; ++i) {
		len = ntohs(msg->opt_templ_set[i]->header.length);
		memcpy(packet + offset, msg->opt_templ_set[i], len);
		offset += len;
	}

	/* Copy data sets */
	for (i = 0; i < MSG_MAX_DATA_COUPLES && msg->data_couple[i].data_set; ++i) {
		len = ntohs(msg->data_couple[i].data_set->header.length);
		memcpy(packet + offset, msg->data_couple[i].data_set, len);
		offset += len;
	}

	return packet;
}

void send_data(sisoconf *sender, void *data, int length)
{
	if (siso_send(sender, data, length) != SISO_OK) {
		MSG_ERROR(msg_module, "%s", siso_get_last_err(sender));
	}
}

void send_except_one(forwarding *conf, void *data, int length, int except_index)
{
	for (int i = 0; i < conf->senders_cnt; ++i) {
		if (i != except_index) {
			send_data(conf->senders[i], data, length);
		}
	}
}

/**
 * \brief Check whether template should be sent
 * \param[in] conf Plugin configuration
 * \param[in] rec Forwarding template record
 * \return 0 if it wasnt sent
 */
int forwarding_udp_sent(forwarding *conf, struct forwarding_template_record *rec)
{
	uint32_t act_time = time(NULL);
	uint32_t life_time = (rec->type == TM_TEMPLATE) ?
			conf->udp.template_life_time : conf->udp.options_template_life_time;
	uint32_t life_packets = (rec->type == TM_TEMPLATE) ?
			conf->udp.template_life_packet : conf->udp.options_template_life_packet;

	/* Check template timeout */
	if (life_time) {
		if (rec->last_sent + life_time > act_time) {
			return 1;
		}
		rec->last_sent = act_time;
	}

	/* Check template life packets */
	if (life_packets) {
		if (rec->packets < life_packets) {
			return 1;
		}
		rec->packets = 0;
	}

	return 0;
}

/**
 * \brief Checks whether template record was send earlier
 *
 * Also controls template updates and inserts not-sent records into array
 *
 * \param[in] conf Plugin configuration
 * \param[in] rec Template record
 * \param[in] len Template record's length
 * \param[in] type Template type
 * \return 0 if it wasnt sent
 */
int forwarding_record_sent(forwarding *conf, struct ipfix_template_record *rec, int len, int type)
{
	struct forwarding_template_record *aux_record;
	int i = forwarding_get_record_index(conf, rec->template_id, type);

	if (i >= 0) {
		if (tm_compare_template_records(conf->records[i]->record, rec)) {
			/* records are equal */
			if (conf->udp_connection) {
				return forwarding_udp_sent(conf, conf->records[i]);
			}
			return 1;
		}
		/* stored record is old, update it */
		forwarding_remove_record(conf, conf->records[i]->record->template_id, type);
	}

	/* Store new record */
	struct ipfix_template_record *stored = calloc(1, len);
	if (!stored) {
		MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)", __FILE__, __LINE__);
		return 0;
	}

	memmove(stored, rec, len);
	aux_record = forwarding_add_record(conf, stored, type, len);
	if (aux_record) {
		aux_record->last_sent = time(NULL);
	}
	return 0;
}

/**
 * \brief Remove template sets without any template records
 * \param[in] msg IPFIX message
 */
void forwarding_remove_empty_sets(struct ipfix_message *msg)
{
	int i, j, len;
	for (i = 0, j = 0; i < MSG_MAX_TEMPL_SETS && msg->templ_set[i]; ++i) {
		len = ntohs(msg->templ_set[i]->header.length);
		if (len <= 4) {
			/* Set correct message length */
			msg->pkt_header->length = htons(ntohs(msg->pkt_header->length) - len);

			/* Shift all template sets behind this (there must not be hole) */
			for (j = i; j < MSG_MAX_TEMPL_SETS - 1 && msg->templ_set[j]; ++j) {
				msg->templ_set[j] = msg->templ_set[j + 1];
			}
		}
	}
}

/**
 * \brief Process each (option) template
 *
 * \param[in] rec Template record
 * \param[in] rec_len Record's length
 * \param[in] data processor's data
 */
void forwarding_process_template(uint8_t *rec, int rec_len, void *data)
{
	struct forwarding_process *proc = (struct forwarding_process *) data;
	if (forwarding_record_sent(proc->conf, (struct ipfix_template_record *) rec, rec_len, proc->type)) {
		return;
	}

	memcpy(proc->msg + proc->offset, rec, rec_len);
	proc->offset += rec_len;
	proc->length += rec_len;
}

/**
 * \brief Remove templates already sent in past (TCP only)
 * \param[in] conf Plugin configuration
 * \param[in] msg IPFIX message
 * \param[in] new_msg new IPFIX message
 * \return Length of new message
 */
int forwarding_remove_sent_templates(forwarding *conf, const struct ipfix_message *msg, uint8_t *new_msg)
{
	int i;
	struct forwarding_process proc;
	struct ipfix_template_set *templ_set;
	struct ipfix_options_template_set *otempl_set;

	proc.conf = conf;
	proc.msg = new_msg;
	proc.offset = IPFIX_HEADER_LENGTH;

	/* Copy unsent templates to new message */
	proc.type = TM_TEMPLATE;
	for (i = 0; i < MSG_MAX_TEMPL_SETS && msg->templ_set[i] != NULL; ++i) {
		memcpy(proc.msg + proc.offset, &(msg->templ_set[i]->header), 4);
		templ_set = (struct ipfix_template_set *) (proc.msg + proc.offset);
		proc.offset += 4;
		proc.length = 4;

		template_set_process_records(msg->templ_set[i], TM_TEMPLATE, &forwarding_process_template, (void *) &proc);

		/* Empty set; only set header was added */
		if (proc.length == 4) {
			proc.offset = proc.length;
			continue;
		}

		/* Update set length in set header (due to potentially removed templates) */
		templ_set->header.length = htons(proc.length);
	}

	/* Copy unsent option templates to new message */
	proc.type = TM_OPTIONS_TEMPLATE;
	for (i = 0; i < MSG_MAX_OTEMPL_SETS && msg->opt_templ_set[i]; ++i) {
		memcpy(proc.msg + proc.offset, &(msg->opt_templ_set[i]->header), 4);
		otempl_set = (struct ipfix_options_template_set *) (proc.msg + proc.offset);
		proc.offset += 4;
		proc.length = 4;

		template_set_process_records((struct ipfix_template_set *) msg->opt_templ_set[i], TM_OPTIONS_TEMPLATE, &forwarding_process_template, (void *) &proc);

		/* Empty set; only set header was added */
		if (proc.length == 4) {
			proc.offset = proc.length;
			continue;
		}

		/* Update set length in set header (due to potentially removed templates) */
		otempl_set->header.length = htons(proc.length);
	}

	return proc.offset;
}

/**
 * \brief Add (option) template set into message
 * \param[in] msg IPFIX message
 * \param[in] set (Option) Template set
 */
void forwarding_add_set(struct ipfix_message *msg, struct ipfix_template_set *set)
{
	int i;

	/* Find first empty slot */
	for (i = 0; i < MSG_MAX_TEMPL_SETS && msg->templ_set[i]; ++i);

	if (i == MSG_MAX_TEMPL_SETS) {
		MSG_ERROR(msg_module, "Could not add template to IPFIX message; message already contains %u templates", MSG_MAX_TEMPL_SETS);
	} else {
		msg->templ_set[i] = set;
		msg->pkt_header->length = htons(ntohs(msg->pkt_header->length) + ntohs(set->header.length));
	}
}

/**
 * \brief Add templates according to udp_conf (life time && life packets)
 * \param[in] conf Plugin configuration
 * \param[in] msg IPFIX message
 * \param[in] new_msg new IPFIX message
 * \param[in] offset offset of new message
 * \return new offset
 */
int forwarding_update_templates(forwarding *conf, const struct ipfix_message *msg, uint8_t *new_msg, int offset)
{
	int i, tid, tset_len = 4, otset_len = 4;
	struct ipfix_template_set *templ_set = NULL, *option_set = NULL;
	struct forwarding_template_record *rec = NULL;

	/* Check each used template */
	for (i = 0; i < MSG_MAX_DATA_COUPLES && msg->data_couple[i].data_set; ++i) {
		if (!msg->data_couple[i].data_template) {
			/* Data set without template, skip it */
			continue;
		}
		tid = ntohs(msg->data_couple[i].data_template->template_id);
		rec = forwarding_get_record(conf, tid, msg->data_couple[i].data_template->template_type);
		if (!rec) {
			/* Data without template */
			continue;
		}

		rec->packets++;

		if (forwarding_udp_sent(conf, rec)) {
			/* Template doesnt need to be sent */
			continue;
		}

		/* Send template - add it to (options) template set */
		if (rec->type == TM_TEMPLATE) {
			templ_set = realloc(templ_set, tset_len + rec->length);
			memcpy((uint8_t *) templ_set + tset_len, rec->record, rec->length);
			tset_len += rec->length;
		} else {
			option_set = realloc(option_set, otset_len + rec->length);
			memcpy((uint8_t *) option_set + otset_len, rec->record, rec->length);
			otset_len += rec->length;
		}
	}

	/* Add sets into IPFIX message */
	if (templ_set) {
		templ_set->header.flowset_id = htons(IPFIX_TEMPLATE_FLOWSET_ID);
		templ_set->header.length = htons(tset_len);
		memcpy(new_msg + offset, templ_set, tset_len);
		free(templ_set);
		offset += tset_len;
	}

	if (option_set) {
		option_set->header.flowset_id = htons(IPFIX_OPTION_FLOWSET_ID);
		option_set->header.length = htons(otset_len);
		memcpy(new_msg + offset, option_set, otset_len);
		free(option_set);
		offset += otset_len;
	}

	return offset;
}

void send_packet(forwarding *conf, void *msg, int length, int templ_only_len)
{
	switch (conf->distribution) {
	case DT_ROUND_ROBIN:
		/**
		 * Round Robin:
		 * send packet to only 1 destination.
		 * next packet will be sent to the next destination.
		 * Template sets MUST be sent to ALL destinations.
		 * Templates are ALWAYS before data sets so we don't need to
		 * copy them to other IPFIX packet. We can only change packet length.
		 */
		if (length == templ_only_len) {
			send_except_one(conf, msg, length, -1);
			break;
		}

		send_data(conf->senders[conf->sender_index], msg, length);

		if (templ_only_len > IPFIX_HEADER_LENGTH) {
			((struct ipfix_header *) msg)->length = htons(templ_only_len);
			send_except_one(conf, msg, templ_only_len, conf->sender_index);
		}

		conf->sender_index++;
		if (conf->sender_index == conf->senders_cnt) {
			conf->sender_index = 0;
		}
		break;
	default:
		/**
		 * Send data to ALL destinations
		 */
		send_except_one(conf, msg, length, -1);
		break;
	}
}

/**
 * \brief Store packet - make packet from message and write it into socket
 * \param[in] config Plugin configuration
 * \param[in] ipfix_msg IPFIX message
 * \param[in] template_mgr Template manager
 * \return 0 on success
 */
int store_packet(void *config, const struct ipfix_message *ipfix_msg,
		const struct ipfix_template_mgr *template_mgr)
{
	(void) template_mgr;
	forwarding *conf = (forwarding *) config;
	uint16_t length = 0, i, setlen, templ_only_len;

	uint8_t *new_msg = malloc(MSG_MAX_LENGTH);
	if (!new_msg) {
		MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)", __FILE__, __LINE__);
		return 1;
	}

	memcpy(new_msg, ipfix_msg->pkt_header, IPFIX_HEADER_LENGTH);
	length += IPFIX_HEADER_LENGTH;

	/* Remove already sent templates */
	length = forwarding_remove_sent_templates(conf, ipfix_msg, new_msg);

	if (conf->udp_connection) {
		/* Add timeouted templates */
		length = forwarding_update_templates(conf, ipfix_msg, new_msg, length);
	}

	templ_only_len = length;

	/* Copy data sets */
	for (i = 0; i < MSG_MAX_DATA_COUPLES && ipfix_msg->data_couple[i].data_set; ++i) {
		setlen = ntohs(ipfix_msg->data_couple[i].data_set->header.length);
		memcpy(new_msg + length, ipfix_msg->data_couple[i].data_set, setlen);
		length += setlen;
	}

	((struct ipfix_header *) new_msg)->length = htons(length);

	/* Send data */
	send_packet(conf, new_msg, length, templ_only_len);
	free(new_msg);

	return 0;
}


/**
 * \brief Close plugin
 * \param[in] config Plugin configuration
 * \return 0 on success
 */
int storage_close(void **config)
{
	MSG_DEBUG(msg_module, "CLOSE");
	forwarding *conf = (forwarding *) *config;

	int i, c;
	for (i = 0, c = 0; i < conf->records_max && c < conf->records_cnt; ++i) {
		if (conf->records[i]) {
			c++;
			free(conf->records[i]->record);
			free(conf->records[i]);
		}
	}

	for (i = 0; i < conf->senders_cnt; ++i) {
		siso_destroy(conf->senders[i]);
	}

	if (conf->default_port) {
		free((void *) conf->default_port);
	}

	if (conf->default_protocol) {
		free((void *) conf->default_protocol);
	}
	
	free(conf->senders);
	free(conf->records);
	free(conf);

	*config = NULL;

	return 0;
}
