/**
 * \file joinflows_ip.c
 * \author Michal Srb <michal.srb@cesnet.cz>
 * \brief Intermediate Process that is able to join multiple flows
 * into the one.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
//#include <glib.h>

#include <ipfixcol.h>
#include "../../intermediate_process.h"
#include "../../ipfix_message.h"


static char *msg_module = "joinflows IP";

struct mapping {
	uint16_t orig;
	uint16_t new;
	struct mapping *next;
};


uint16_t *mapping_lookup(struct mapping *map, uint16_t orig)
{
	if (map == NULL) {
		return NULL;
	}
	struct mapping *aux_map = map;
	while (aux_map) {
		if (aux_map->orig == orig) {
			return &(aux_map->new);
		}
		aux_map = aux_map->next;
	}
	return NULL;
}

struct mapping *mapping_insert(struct mapping *map, uint16_t orig, uint16_t new)
{
	struct mapping *new_map = malloc(sizeof(struct mapping));
	if (new_map == NULL) {
		return NULL;
	}
	new_map->orig = orig;
	new_map->new = new;

	new_map->next = map;
	map = new_map;

	return new_map;
}

void mapping_destroy(struct mapping *map)
{
	struct mapping *aux_map = map;

	while (aux_map) {
		map = aux_map->next;
		free(aux_map);
		aux_map = map;
	}
}

struct source {
	uint32_t orig_odid;
	uint32_t new_odid;
	uint16_t first_template_id;
	uint16_t next_template_id;
	struct ipfix_template_mgr *tm;
	struct mapping *tmapping;           /* mapping of the templates */
};

/* plugin's configuration structure */
struct joinflows_ip_config {
	char *params;
	void *ip_config;
	uint32_t odid_inter[128];
	uint32_t odid_new;
	uint8_t odid_new_set;
	uint8_t odid_counter;
	struct source sources[128];
	uint32_t sequence_number;
	uint32_t ip_id;
	struct ipfix_template_mgr *tm;
};

static void joinflows_copy_fields(uint8_t *to, uint8_t *from, uint16_t length)
{
	int i;
	uint16_t offset = 0;
	uint8_t *template_ptr, *tmpl_ptr;

	template_ptr = to;
	tmpl_ptr = from;
	while (offset < length) {
		for (i=0; i < 4 / 2; i++) {
			*((uint16_t*)(template_ptr+offset+i*2)) = ntohs(*((uint16_t*)(tmpl_ptr+offset+i*2)));
		}
		offset += 4;
		if (*((uint16_t *) (template_ptr+offset-4)) & 0x8000) { /* enterprise element has first bit set to 1*/
			for (i=0; i < 4 / 4; i++) {
				*((uint32_t*)(template_ptr+offset)) = ntohl(*((uint32_t*)(tmpl_ptr+offset)));
			}
			offset += 4;
		}
	}
	return;
}


static uint16_t joinflows_template_length(struct ipfix_template_record *template, int max_len, int type, uint32_t *data_length, struct source *source)
{
	uint8_t *fields;
	int count;
	uint16_t fields_length = 0;
	uint16_t tmpl_length;
	uint32_t data_record_length = 0;
	uint16_t tmp_data_length;
	uint16_t orig_id;
	uint16_t *res = NULL;

	tmpl_length = sizeof(struct ipfix_template) - sizeof(template_ie);

	if (type == TM_TEMPLATE) { /* template */
		fields = (uint8_t *) template->fields;
	} else { /* options template */
		fields = (uint8_t *) ((struct ipfix_options_template_record*) template)->fields;
	}



	orig_id = ntohs(template->template_id);

	res = mapping_lookup(source->tmapping, orig_id);
	if (!res) {
		/* get new unique Template ID */
//		template->template_id = htons(source->next_template_id);
//		source->next_template_id += 1;
//		MSG_DEBUG(msg_module, "Template ID %hu is now %hu", orig_id, ntohs(template->template_id));

		/* remember this mapping */
//		int *store_orig = (int *) malloc(sizeof(int));
//		int *store_new = (int *) malloc(sizeof(int));
//
//		*store_orig = (int) orig_id;
//		*store_new = (int) ntohs(template->template_id);
		source->tmapping = mapping_insert(source->tmapping, orig_id, ntohs(template->template_id));
	} else {
		/* we already have mapped this template ID */
//		MSG_DEBUG(msg_module, "Template ID %hu is already mapped to %hu", orig_id, (uint16_t) (*res));
		template->template_id = htons((uint16_t) (*res));
	}

	for (count=0; count < ntohs(template->count); count++) {
		/* count data record length */
		tmp_data_length = ntohs(*((uint16_t *) (fields + fields_length + 2)));

		if (tmp_data_length == 0xffff) {
			/* this Information Element has variable length */
			data_record_length |= 0x80000000;  /* taint this variable. we can't count on it anymore,
			                                    * but it can tell us what is the smallest length
			                                    * of the Data Record possible */
			data_record_length += 1;           /* every field is at least 1 byte long */
		} else {
			/* actual length is stored in the template */
			data_record_length += tmp_data_length;
		}
		/* enterprise element has first bit set to 1 */
		if (ntohs((*((uint16_t *) (fields+fields_length)))) & 0x8000) {
			fields_length += 4;
		}
		fields_length += 4;

		if (fields_length > max_len) {
			/* oops, no more template fields... we reached end of message
			 * or end of the set. what can we do, this message is obviously
			 * malformed, skip it */
			return 0;
		}
	}

	if (data_length != NULL) {
		*data_length = data_record_length;
	}
	tmpl_length += fields_length;
	return tmpl_length;
}


static int joinflows_fill_template(struct ipfix_template *template, void *template_record, uint16_t template_length,
							uint32_t data_length, int type)
{
	struct ipfix_template_record *tmpl = (struct ipfix_template_record*) template_record;

	/* set attributes common to both template types */
	template->template_type = type;
	template->field_count = ntohs(tmpl->count);
	template->template_id = ntohs(tmpl->template_id);
	template->template_length = template_length;
	template->data_length = data_length;

	/* set type specific attributes */
	if (type == TM_TEMPLATE) { /* template */
		template->scope_field_count = 0;
		/* copy fields and convert to host byte order */
		joinflows_copy_fields((uint8_t*)template->fields,
					(uint8_t*)tmpl->fields,
					template_length - sizeof(struct ipfix_template) + sizeof(template_ie));
	} else { /* option template */
		template->scope_field_count = ntohs(((struct ipfix_options_template_record*) template_record)->scope_field_count);
		if (template->scope_field_count == 0) {
			MSG_WARNING(msg_module, "Option template scope field count is 0");
			return 1;
		}
		joinflows_copy_fields((uint8_t*)template->fields,
							(uint8_t*)((struct ipfix_options_template_record*) template_record)->fields,
							template_length - sizeof(struct ipfix_template) + sizeof(template_ie));
	}
	return 0;
}


static struct ipfix_template *joinflows_add_template(struct ipfix_template_mgr *tm, void *template, int max_len, int type, struct source *source, uint32_t ip_id)
{
	struct ipfix_template *new_tmpl = NULL;
	uint32_t data_length = 0;
	uint32_t tmpl_length;

	tmpl_length = joinflows_template_length(template, max_len, type, &data_length, source);
	if (tmpl_length == 0) {
		/* template->count probably points beyond current set area */
		MSG_WARNING(msg_module, "Template %d is malformed (bad template count), skipping.",
				ntohs(((struct ipfix_template_record *) template)->template_id));
		return NULL;
	}

	/* allocate memory for new template */
	if ((new_tmpl = malloc(tmpl_length)) == NULL) {
		MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)", __FILE__, __LINE__);
	}

	if (joinflows_fill_template(new_tmpl, template, tmpl_length, data_length, type) == 1) {
		free(new_tmpl);
		return NULL;
	}
//	g_hash_table_insert(tm->templates, (gpointer) &(new_tmpl->template_id), new_tmpl);

	struct ipfix_template_key *key = tm_key_create(source->new_odid, ip_id, ntohs(((struct ipfix_template_record *) template)->template_id));

	tm_insert_template(tm, new_tmpl, key);
	return new_tmpl;
}


static int joinflows_process_one_template(struct ipfix_template_mgr *tm, void *tmpl, int max_len, int type, uint32_t msg_counter,
		struct input_info *input_info, struct source *source, uint32_t ip_id)
{
	struct ipfix_template_record *template_record;
	struct ipfix_template *template;
	int ret;

	template_record = (struct ipfix_template_record*) tmpl;

	struct ipfix_template_key *key = tm_key_create(source->new_odid, ip_id, ntohs(template_record->template_id));

	/* check for withdraw all templates message */
	/* these templates are no longer used (checked in data_manager_withdraw_templates()) */
	if (input_info->type == SOURCE_TYPE_UDP && ntohs(template_record->count) == 0) {
		/* got withdrawal message with UDP -> this is wrong */
		MSG_WARNING(msg_module, "Got template withdraw message on UDP. Ignoring.");
		return TM_TEMPLATE_WITHDRAW_LEN;
	} else if (ntohs(template_record->template_id) == IPFIX_TEMPLATE_FLOWSET_ID &&
			ntohs(template_record->count) == 0) {
		tm_remove_all_templates(tm, type);
		/* don't try to parse the withdraw template */
		return TM_TEMPLATE_WITHDRAW_LEN;
		/* check for withdraw template message */
	} else if (ntohs(template_record->count) == 0) {
		ret = tm_remove_template(tm, key);
		/* Log error when removing unknown template */
		if (ret == 1) {
			MSG_WARNING(msg_module, "%s withdraw message received for unknown Template ID: %u",
					(type==TM_TEMPLATE)?"Template":"Options template", ntohs(template_record->template_id));
		}
		return TM_TEMPLATE_WITHDRAW_LEN;
		/* check whether template exists */
	} else if ((template = tm_get_template(tm, key)) == NULL) {
		/* add template */
		/* check that the template has valid ID ( < 256 ) */
		if (ntohs(template_record->template_id) < 256) {
			MSG_WARNING(msg_module, "%s ID %i is reserved and not valid for data set!", (type==TM_TEMPLATE)?"Template":"Options template", ntohs(template_record->template_id));
		} else {
			MSG_NOTICE(msg_module, "New %s ID %i", (type==TM_TEMPLATE)?"template":"options template", ntohs(template_record->template_id));
			template = joinflows_add_template(tm, tmpl, max_len, type, source, ip_id);
		}
	} else {
		/* template already exists */
		MSG_WARNING(msg_module, "%s ID %i already exists. Rewriting.",
				(type==TM_TEMPLATE)?"Template":"Options template", template->template_id);
		template = tm_update_template(tm, tmpl, max_len, type, key);
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


static int joinflows_process_message(struct ipfix_template_mgr *tm, struct source *source, struct ipfix_message *message, uint32_t ip_id)
{
	uint32_t message_counter = 0;
	uint8_t *ptr;
	int i, ret;
	int max_len;     /* length to the end of the set = max length of the template */


	/* check for new templates */
	for (i=0; message->templ_set[i] != NULL && i<1024; i++) {
		ptr = (uint8_t*) &message->templ_set[i]->first_record;
		while (ptr < (uint8_t*) message->templ_set[i] + ntohs(message->templ_set[i]->header.length)) {
			max_len = ((uint8_t *) message->templ_set[i] + ntohs(message->templ_set[i]->header.length)) - ptr;
			ret = joinflows_process_one_template(tm, ptr, max_len, TM_TEMPLATE, message_counter, message->input_info, source, ip_id);
			if (ret == 0) {
				break;
			} else {
				ptr += ret;
			}
		}
	}

	/* check for new option templates */
	for (i=0; message->opt_templ_set[i] != NULL && i<1024; i++) {
		ptr = (uint8_t*) &message->opt_templ_set[i]->first_record;
		max_len = ((uint8_t *) message->opt_templ_set[i] + ntohs(message->opt_templ_set[i]->header.length)) - ptr;
		while (ptr < (uint8_t*) message->opt_templ_set[i] + ntohs(message->opt_templ_set[i]->header.length)) {
			ret = joinflows_process_one_template(tm, ptr, max_len, TM_OPTIONS_TEMPLATE, message_counter, message->input_info, source, ip_id);
			if (ret == 0) {
				break;
			} else {
				ptr += ret;
			}
		}
	}

	return 0;
}


int intermediate_plugin_init(char *params, void *ip_config, uint32_t ip_id, struct ipfix_template_mgr *template_mgr, void **config)
{
	struct joinflows_ip_config *conf;
	int retval;
	uint32_t interval;
    int i;


	conf = (struct joinflows_ip_config *) malloc(sizeof(*conf));
	if (!conf) {
		MSG_ERROR(msg_module, "Unable to allocate memory (%s:%d)", __FILE__, __LINE__);
		return -1;
	}
	memset(conf, 0, sizeof(*conf));

	conf->params = params;
	conf->ip_config = ip_config;
	conf->ip_id = ip_id;
	conf->tm = template_mgr;

    /* parse params */
    xmlDoc *doc = NULL;
    xmlNode *root_element = NULL;
    xmlNode *cur_node = NULL;


    /* parse xml string */
    doc = xmlParseDoc(BAD_CAST params);
    if (doc == NULL) {
    	MSG_ERROR(msg_module, "Cannot parse config xml");
        retval = 1;
        goto out;
    }
    /* get the root element node */
    root_element = xmlDocGetRootElement(doc);
    if (root_element == NULL) {
    	MSG_ERROR(msg_module, "Cannot get document root element");
        retval = 1;
        goto out;
    }

    /* go over all elements */
    for (cur_node = root_element->children; cur_node; cur_node = cur_node->next) {

        if (cur_node->type == XML_ELEMENT_NODE && cur_node->children != NULL) {
            /* copy value to memory - don't forget the terminating zero */
            char *tmp_val = malloc(sizeof(char)*strlen((char *)cur_node->children->content)+1);
            /* this is not a preferred cast, but we really want to use plain chars here */
            if (tmp_val == NULL) {
            	MSG_ERROR(msg_module, "Cannot allocate memory: %s", strerror(errno));
                retval = 1;
                goto out;
            }
            strcpy(tmp_val, (char *)cur_node->children->content);

            if (xmlStrEqual(cur_node->name, BAD_CAST "sourceODID")) {
            	conf->odid_inter[conf->odid_counter++] = (uint32_t) atoi(tmp_val);
            }
            if (xmlStrEqual(cur_node->name, BAD_CAST "newODID")) {
            	conf->odid_new = (uint32_t) atoi(tmp_val);
            	conf->odid_new_set = 1;
            }
            free(tmp_val);
        }
    }

    if (conf->odid_counter == 0) {
    	interval = 65536;
    } else {
    	interval = 65536 / conf->odid_counter;
    }

    if ((conf->odid_counter > 0) && conf->odid_new_set) {
        for (i = 0; i < conf->odid_counter; i++) {
        	conf->sources[i].tm = tm_create();
//        	conf->sources[i].tm->odid = conf->odid_inter[i];
        	conf->sources[i].tmapping = NULL;
        	conf->sources[i].first_template_id = interval * i + 256;
        	conf->sources[i].next_template_id = conf->sources[i].first_template_id;
        	conf->sources[i].new_odid = conf->odid_new;
        }
    }

	*config = conf;

	MSG_NOTICE(msg_module, "Successfully initialized");

	/* plugin successfully initialized */
	return 0;

out:
	free(conf);

	return retval;
}


/* Do nothing, just pass the message to the output queue */
int process_message(void *config, void *message)
{
	struct ipfix_message *msg;
	struct joinflows_ip_config * conf;
	int i, c;
	uint32_t orig_odid;

	msg = (struct ipfix_message *) message;
	conf = (struct joinflows_ip_config *) config;

	MSG_DEBUG(msg_module, "got IPFIX message!");

	for (i = 0; i < conf->odid_counter; i++) {
		orig_odid = ntohl(msg->pkt_header->observation_domain_id);
		if (orig_odid == conf->odid_inter[i]) {
			msg->pkt_header->observation_domain_id = htonl(conf->odid_new);
			msg->pkt_header->sequence_number = htonl(conf->sequence_number);
			conf->sequence_number += 1;

			joinflows_process_message(conf->sources[i].tm, &(conf->sources[i]), msg, conf->ip_id);

			for (c=0; msg->data_couple[c].data_set != NULL && c<1023; c++) {
				uint16_t orig_id = ntohs(msg->data_couple[c].data_set->header.flowset_id);
				uint16_t *ret = NULL;
				ret = mapping_lookup(conf->sources[i].tmapping, orig_id);

				if (ret) {
					/* we have mapped template ID */
					msg->data_couple[c].data_set->header.flowset_id = htons((uint16_t) (*ret));
				}
			}

			message_set_templates(msg, conf->sources[i].tm, conf->ip_id);

//			msg->template_manager = conf->sources[i].tm;
		}
	}

	pass_message(conf->ip_config, message);

	return 0;
}


int intermediate_plugin_close(void *config)
{
	int i;
	struct joinflows_ip_config *conf;

	conf = (struct joinflows_ip_config *) config;

	for (i = 0; i < conf->odid_counter; i++) {
		mapping_destroy(conf->sources->tmapping);

		/* free template manager */
		tm_destroy(conf->sources[i].tm);
	}

	free(conf);

	return 0;
}

