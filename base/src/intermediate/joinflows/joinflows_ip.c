/**
 * \file joinflows_ip.c
 * \author Michal Kozubik <kozubik.michal@gmail.com>
 * \brief Intermediate Process that is able to join multiple flows
 * into the one.
 *
 * Copyright (C) 2014 CESNET, z.s.p.o.
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
#include <string.h>

#include <ipfixcol.h>
#include "../../intermediate_process.h"
#include "../../ipfix_message.h"

#include <libxml/parser.h>
#include <libxml/tree.h>

/* module name for MSG_* */
static const char *msg_module = "joinflows";

struct mapped_template {
	int references;
	struct ipfix_template *templ;
};

/* mapping structure */
struct mapping {
	uint32_t orig_odid;  /* original ODID */
	uint32_t new_odid;	 /* new ODID */
	uint16_t orig_tid;   /* original Template ID */
	uint16_t new_tid;	 /* new Template ID */
	struct mapped_template *new_templ;
	struct ipfix_template *orig_templ;  /* pointer to original template for comparing */
	struct mapping *next; /* next mapping */
};

/* structure for each mapped source ODID */
struct source {
	uint32_t orig_odid;      /* original ODID */
	uint32_t new_odid;		 /* new ODID */
	struct input_info *input_info;
	struct mapping *mapping; /* mapping common for all source ODIDs mapped on the same new ODID */
};

/* plugin's configuration structure */
struct joinflows_ip_config {
	char *params;  /* input parameters */
	void *ip_config; /* internal process configuration */
	uint8_t odid_counter; /* number of mapped ODIDs */
	uint8_t map_counter;
	uint8_t default_map;	  /* flag indicating mapping every unmentioned ODIDs */
	struct source sources[128]; /* source structure for each ODID */
	struct mapping mappings[128]; /* mappings */
	struct source default_source;  /* mapping for unmentioned ODIDs */
	uint32_t ip_id; /* source ID for Template Manager */
	struct ipfix_template_mgr *tm; /* Template Manager */
};

/**
 * \brief Compare two templates
 *
 * \param[in] first First template
 * \param[in] second Second template
 * \return non-zero if templates are equal
 */
int template_compare(struct ipfix_template *first, struct ipfix_template *second)
{
	/* Check valid pointers */
	if (first == NULL || second == NULL) {
		return 0;
	}

	/* Same pointers? */
	if (first == second) {
		return 1;
	}

	/* Check template length, data length and number of fields */
	if (first->template_length != second->template_length
			|| first->data_length != second->data_length
			|| first->field_count != second->field_count) {
		return 0;
	}

	uint16_t *field1 = (uint16_t *) first->fields;
	uint16_t *field2 = (uint16_t *) second->fields;

	/* Check each field ID */
	uint16_t i;
	for (i = 0; i < first->field_count; ++i, field1 += 2, field2 += 2) {
		if (*field1 != *field2) {
			return 0;
		}
	}

	/* Templates are equal */
	return 1;
}


#define TEMPLATE_FIELD_LEN 4
#define TEMPLATE_ENT_NUM_LEN 4
/**
 * \brief Copy ipfix_template fields and convert them to host byte order
 *
 * \param[out] to Destination
 * \param[in] from Source
 * \param[in] length template size
 */
static void joinflows_copy_fields(uint8_t *to, uint8_t *from, uint16_t length)
{
	int i;
	uint16_t offset = 0;
	uint8_t *template_ptr, *tmpl_ptr;

	template_ptr = to;
	tmpl_ptr = from;

	while (offset < length) {
		for (i=0; i < TEMPLATE_FIELD_LEN / 2; i++) {
			*((uint16_t*)(template_ptr+offset+i*2)) = htons(*((uint16_t*)(tmpl_ptr+offset+i*2)));
		}
		offset += TEMPLATE_FIELD_LEN;
//		if (*((uint16_t *) (template_ptr+offset-TEMPLATE_FIELD_LEN)) & 0x8000) { /* enterprise element has first bit set to 1*/
//			MSG_DEBUG(msg_module, "enterprise");
//			for (i=0; i < TEMPLATE_ENT_NUM_LEN / 4; i++) {
//				*((uint32_t*)(template_ptr+offset)) = htonl(*((uint32_t*)(tmpl_ptr+offset)));
//			}
//			offset += TEMPLATE_ENT_NUM_LEN;
//		}
	}

	return;
}

/**
 * \brief Update original template (add field 405 - original ODID)
 *
 * \param[in] orig_templ Original template
 * \param[in] new_tid Template ID of new template
 * \return pointer to updated template
 */
struct mapped_template *updated_templ(struct ipfix_template *orig_templ, uint16_t new_tid)
{
	struct mapped_template *new_mapped;
	struct ipfix_template *new_templ;
	static uint16_t field_num = 450;
	static uint16_t field_len = 4;
	uint16_t len;

	/* Allocate memory for updated template */
	new_mapped = malloc(sizeof(struct mapped_template));
	if (new_mapped == NULL) {
		return NULL;
	}

	new_templ = malloc(orig_templ->template_length + 4);
	if (new_templ == NULL) {
		free(new_mapped);
		return NULL;
	}

	/* Compute length of fields */
	len = orig_templ->template_length - sizeof(struct ipfix_template) + sizeof(template_ie);

	/* Copy all informations and add field 405 at the end */
	memcpy(new_templ, orig_templ, orig_templ->template_length);
	memcpy(((uint8_t *)new_templ->fields) + len, &field_num, 2);
	memcpy(((uint8_t *)new_templ->fields) + len + 2, &field_len, 2);

	/* Set new values */
	new_templ->template_id = new_tid;
	new_templ->template_length += 4;
	new_templ->data_length += 4;
	new_templ->field_count++;

	/* References will be set later */
	new_templ->references = 0;

	new_mapped->templ = new_templ;
	new_mapped->references = 1;
	return new_mapped;
}

/**
 * \brief Find mapping for given ODID and TID
 *
 * \param[in] map Mapping header
 * \param[in] orig_odid Original ODID
 * \param[in] orig_tid Original Template ID
 * \return pointer to mapping
 */
struct mapping *mapping_lookup(struct mapping *map, uint32_t orig_odid, uint16_t orig_tid)
{
	if (map == NULL) {
		return NULL;
	}
	struct mapping *aux_map = map;
	while (aux_map) {
		if (aux_map->orig_odid == orig_odid && aux_map->orig_tid == orig_tid) {
			return aux_map;
		}
		aux_map = aux_map->next;
	}

	return NULL;
}

/**
 * \brief Create new mapping
 *
 * \param[in] map Mapping header
 * \param[in] orig_odid Original ODID
 * \param[in] orig_tid Original Template ID
 * \param[in] orig_templ Original template
 * \return pointer to new mapping
 */
struct mapping *mapping_create(struct mapping *map, uint32_t orig_odid, uint16_t orig_tid, struct ipfix_template *orig_templ)
{
	struct mapping *aux_map, *new_map;

	new_map = malloc(sizeof(struct mapping));
	if (new_map == NULL) {
		return NULL;
	}

	/* Find last mapping in map */
	aux_map = map;
	while (aux_map->next) {
		aux_map = aux_map->next;
	}

	/* Fill in informations of new mapping */
	new_map->orig_odid = orig_odid;
	new_map->orig_tid = orig_tid;
	new_map->orig_templ = orig_templ;

	new_map->new_odid = map->new_odid;
	new_map->new_tid = aux_map->new_tid + 1;

	/* Create updated template */
	new_map->new_templ = updated_templ(orig_templ, new_map->new_tid);

	return new_map;

}

/**
 * \brief Insert mapping into map
 *
 * \param[in] map Mapping header
 * \param[in] new_map Inserted mapping
 * \return pointer to Inserted mapping
 */
struct mapping *mapping_insert(struct mapping *map, struct mapping *new_map)
{
	struct mapping *aux_map;

	/* Find last mapping in map */
	aux_map = map;
	while (aux_map->next) {
		aux_map = aux_map->next;
	}

	new_map->next = NULL;
	aux_map->next = new_map;

	return new_map;
}

/**
 * \brief Create mapping from another one
 *
 * \param[in] orig_map Original mapping
 * \param[in] orig_odid Original ODID
 * \param[in] orig_tid Original Template ID
 * \return pointer to new mapping
 */
struct mapping *mapping_copy(struct mapping *orig_map, uint32_t orig_odid, uint16_t orig_tid)
{
	struct mapping *new_map = calloc(1, sizeof(struct mapping));
	if (new_map == NULL) {
		return NULL;
	}

	/* ODID and TID are different, rest is the same */
	new_map->orig_odid = orig_odid;
	new_map->orig_tid = orig_tid;

	new_map->orig_templ = orig_map->orig_templ;
	new_map->new_odid = orig_map->new_odid;
	new_map->new_tid = orig_map->new_tid;
	new_map->new_templ = orig_map->new_templ;
	new_map->new_templ->references++;

	return new_map;
}

/**
 * \brief Find mapping with given template
 *
 * \param[in] map Mapping header
 * \param[in] orig_templ Original template
 * \return pointer to mapping
 */
struct mapping *mapping_find_equal(struct mapping *map, struct ipfix_template *orig_templ)
{
	struct mapping *aux_map;

	aux_map = map;
	while (aux_map) {
		if (template_compare(aux_map->orig_templ, orig_templ)) {
			return aux_map;
		}
		aux_map = aux_map->next;
	}

	return NULL;
}

struct mapping *mapping_equal(struct mapping *map, uint32_t orig_odid, uint16_t orig_tid, struct ipfix_template *orig_templ)
{
	struct mapping *new_mapping = NULL, *equal_mapping = NULL;

	equal_mapping = mapping_find_equal(map, orig_templ);
	if (equal_mapping != NULL) {
		new_mapping = mapping_copy(equal_mapping, orig_odid, orig_tid);
	}
	return new_mapping;
}

/**
 * \brief Find equal mapping or create a new one
 *
 * \param[in,out] map Mapping header
 * \param[in] orig_odid Original ODID
 * \param[in] orig_tid Originam Template ID
 * \param[in] orig_templ Original Template
 * \return pointer to mapping
 */
struct mapping *mapping_equal_or_new(struct mapping *map, uint32_t orig_odid, uint16_t orig_tid, struct ipfix_template *orig_templ)
{
	struct mapping *new_mapping, *equal_mapping;

	/* Mapping not found -> find mapping with equal template */
	equal_mapping = mapping_find_equal(map, orig_templ);

	if (equal_mapping == NULL) {
		/* Found new mapping */
		MSG_NOTICE(msg_module, "New mapping for ODID %d and Template ID %d", orig_odid, orig_tid);
		new_mapping = mapping_create(map, orig_odid, orig_tid, orig_templ);
	} else {
		/* Found equal mapping -> copy it with differenc original IDs
		 * so next time it can be found only by mapping_lookup
		 */
		MSG_NOTICE(msg_module, "Found equal mapping for ODID %d and Template ID %d", orig_odid, orig_tid);
		new_mapping = mapping_copy(equal_mapping, orig_odid, orig_tid);
	}

	return new_mapping;
}

/**
 * \brief Remove mapping with given ODID and Template ID
 *
 * \param[in,out] map Mapping header
 * \param[in] old_map Old mapping
 */
void mapping_remove(struct mapping *map, struct mapping *old_map)
{
	struct mapping *aux_map;
	aux_map = map;

	while (aux_map) {
		if (aux_map->next == old_map) {
			aux_map->next = old_map->next;
			break;
		}

		aux_map = aux_map->next;
	}

	old_map->new_templ->references--;
	if (old_map->new_templ->references <= 0) {
		/* If there is no reference on modified template, remove it */
		free(old_map->new_templ->templ);
		free(old_map->new_templ);
	}

	/* Free mapping */
	free(old_map);
}

/**
 * \brief Destroy mapping
 *
 * \param[in,out] map Mapping header
 */
void mapping_destroy(struct mapping *map)
{
	struct mapping *aux_map = map;

	while (aux_map) {
		map = aux_map->next;
		if (aux_map->new_templ != NULL) {
			aux_map->new_templ->references--;
			if (aux_map->new_templ->references <= 0) {
				free(aux_map->new_templ->templ);
				free(aux_map->new_templ);
			}
		}
//		free(aux_map);
		aux_map = map;
	}
}

/**
 * \brief Update data set, add info about original ODID (405) into each record
 *
 * \param[in,out] couple Data couple
 * \param[in] orig_odid Original ODID
 */
void update_data_set(struct data_template_couple *couple, uint32_t orig_odid)
{
	struct ipfix_data_set *new_set, *old_set;
	struct ipfix_set_header *aux_src, *aux_dst;
	uint16_t i, new_length, records_num;
	uint32_t data_length;

	data_length = couple->data_template->data_length;
	orig_odid = htons(orig_odid);
	old_set = couple->data_set;

	/* Count number of data records in data set */
	records_num = (ntohs(old_set->header.length) - 4) / data_length;

	/* Length of new data set */
	new_length = ntohs(old_set->header.length) + records_num * 4;

	/* Allocate memory for new data set */
	new_set = calloc(1, new_length);
	if (new_set == NULL) {
		return;
	}

	/* Set ID and length */
	new_set->header.flowset_id = old_set->header.flowset_id;
	new_set->header.length = htons(new_length);

	aux_dst = &(new_set->header);
	aux_src = &(old_set->header);

	/* Copy every data record into new data set and insert orig_odid as last element */
	aux_dst++;
	aux_src++;
	for (i = 0; i < records_num; ++i) {
		memcpy(aux_dst, aux_src, data_length);
		aux_dst = (struct ipfix_set_header *) ((uint8_t *) aux_dst + data_length);
		memcpy(aux_dst, &orig_odid, 4);

		/* move to next data record */
		aux_dst++;
		aux_src = (struct ipfix_set_header *) ((uint8_t *) aux_src + data_length);
	}

	/* replace data set by new one */
	couple->data_set = new_set;

	/* TODO: segfault - why? */
//	free(old_set);
}

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
int intermediate_plugin_init(char *params, void *ip_config, uint32_t ip_id, struct ipfix_template_mgr *template_mgr, void **config)
{
	struct joinflows_ip_config *conf;
	conf = (struct joinflows_ip_config *) calloc(1, sizeof(*conf));
	if (!conf) {
		MSG_ERROR(msg_module, "Unable to allocate memory (%s:%d)", __FILE__, __LINE__);
		return -1;
	}

	if (!params) {
		MSG_ERROR(msg_module, "Missing plugin configuration!");
		free(conf);
		return -1;
	}

	/* parse configuration */
	char *to = NULL;

	xmlDoc *doc = NULL;
	xmlNode *root = NULL;
	xmlNode *curr = NULL;
	xmlNode *join = NULL;
	xmlNode *from = NULL;

	doc = xmlParseDoc(BAD_CAST params);
	if (!doc) {
		MSG_ERROR(msg_module, "Cannot parse config xml!");
		free(conf);
		return -1;
	}

	root = xmlDocGetRootElement(doc);
	if (!root) {
		MSG_ERROR(msg_module, "Cannot get document root element!");
		free(conf);
		return -1;
	}

	struct mapping *new_map = NULL;
	struct source *src = NULL;

	for (curr = root->children; curr != NULL; curr = curr->next) {
		join = curr->children;
		for (join = curr->children; join != NULL; join = join->next) {
			to = (char *) xmlGetProp(curr, (const xmlChar *) "to");
			new_map = &(conf->mappings[conf->map_counter++]);
			new_map->new_odid = atoi(to);
			new_map->new_tid = 255;
			xmlFree(to);

			for (from = join->children; from != NULL; from = from->next) {
				if (!xmlStrcmp(from->content, (const xmlChar *) "*")) {
					conf->default_map = 1;
					src = &(conf->default_source);
				} else {
					src = &(conf->sources[conf->odid_counter++]);
				}
				src->mapping = new_map;
				src->orig_odid = atoi((char *)from->content);
				src->input_info = NULL;
				src->new_odid = new_map->new_odid;
			}
		}
	}

	conf->ip_id = ip_id;
	conf->ip_config = ip_config;
	conf->tm = template_mgr;

	xmlFreeDoc(doc);
	*config = conf;
	MSG_NOTICE(msg_module, "Successfully initialized");
	return 0;
}


void update_new_msg(uint8_t **new_msg, uint16_t *new_msg_len, struct ipfix_template *new_templ)
{
	int new_id = htons(new_templ->template_id);
	int field_cnt = htons(new_templ->field_count);

	/* real length of template in message */
	int real_len = new_templ->template_length - sizeof(struct ipfix_template) + sizeof(template_ie);

	/* Check if thera are some data in message */
	if (*new_msg == NULL) {
		*new_msg_len = IPFIX_HEADER_LENGTH + 4;
		*new_msg = malloc(*new_msg_len);
	}

	/* Insert new template to message */
	*new_msg = realloc(*new_msg, *new_msg_len + new_templ->template_length);
	memcpy(*new_msg + *new_msg_len, &new_id, 2);
	memcpy(*new_msg + *new_msg_len + 2, &field_cnt, 2);

	joinflows_copy_fields(*new_msg + *new_msg_len + 4, (uint8_t *)new_templ->fields, real_len);

	/* Update length of new message */
	*new_msg_len += real_len + 4;
}

/**
 * \brief Process IPFIX message
 *
 * \param[in] config Plugin configuration
 * \param[in,out] message IPFIX message
 */
int process_message(void *config, void *message)
{
	uint8_t *new_msg = NULL;
	uint16_t new_msg_len= 0;
	uint32_t i, orig_odid, orig_tid;
	struct ipfix_message *msg;
	struct ipfix_template *orig_templ;
	struct joinflows_ip_config *conf;
	struct mapping *new_mapping;
	struct source *src;

	msg = (struct ipfix_message *) message;
	conf = (struct joinflows_ip_config *) config;


	/* Get original ODID */
	orig_odid = msg->input_info->odid;

	/* Find mapping from orig_odid */
	for (i = 0; i < conf->odid_counter; ++i) {
		if (orig_odid == conf->sources[i].orig_odid) {
			break;
		}
	}
	if (i == conf->odid_counter) {
		/* Mapping for this ODID not found */
		if (conf->default_map) {
			/* Use default source and default mapping */
			src = &(conf->default_source);
		} else {
			MSG_DEBUG(msg_module, "No mapping for ODID %d, ignoring", orig_odid);
			pass_message(conf->ip_config, message);
			return 0;
		}
	} else {
		/* Get source structure */
		src = &(conf->sources[i]);
	}

	/* Source closed */
	if (msg->source_status == SOURCE_STATUS_CLOSED) {
		msg->input_info = src->input_info;
		pass_message(conf->ip_config, (void *) msg);
		return 0;
	}

	/* Create mapped input_info structure */
	if (src->input_info == NULL) {
		if (msg->input_info->type == SOURCE_TYPE_IPFIX_FILE) {
			src->input_info = calloc(1, sizeof(struct input_info_file));
			memcpy(src->input_info, msg->input_info, sizeof(struct input_info_file));
		} else {
			src->input_info = calloc(1, sizeof(struct input_info_network));
			memcpy(src->input_info, msg->input_info, sizeof(struct input_info_network));
		}
		src->input_info->odid = src->new_odid;
	}

	/* Skip old template sets */
	for (i = 0; msg->templ_set[i] != NULL && i < 1024; ++i) {
		msg->templ_set[i] = NULL;
	}

	/* Replace template in each data couple */
	for (i = 0; msg->data_couple[i].data_set != NULL && i < 1024; ++i) {
		if (!msg->data_couple[i].data_template) {
			/* Data set without template, skip it */
			continue;
		}
		orig_templ = msg->data_couple[i].data_template;
		orig_tid = orig_templ->template_id;

		/* Add information about original ODID into data records */
		update_data_set(&(msg->data_couple[i]), orig_odid);

		/* Find mapped template */
		new_mapping = mapping_lookup(src->mapping, orig_odid, orig_tid);

		if (new_mapping == NULL) {
			/* Mapping not found - find equal or create new */
			new_mapping = mapping_equal(src->mapping, orig_odid, orig_tid, orig_templ);

			if (new_mapping == NULL) {
				new_mapping = mapping_create(src->mapping, orig_odid, orig_tid, orig_templ);

				/* Add newly created template into new message */
				update_new_msg(&new_msg, &new_msg_len, new_mapping->new_templ->templ);
			}

			mapping_insert(src->mapping, new_mapping);
		} else if (new_mapping->orig_templ != msg->data_couple[i].data_template) {
			MSG_DEBUG(msg_module, "Mapping for %d found, but template is old.", orig_tid);

			/* Remove outdated mapping */
			mapping_remove(src->mapping, new_mapping);

			/* Find equal or create new mapping */
			new_mapping = mapping_equal(src->mapping, orig_odid, orig_tid, orig_templ);

			if (new_mapping == NULL) {
				new_mapping = mapping_create(src->mapping, orig_odid, orig_tid, orig_templ);

				/* Add newly created template into new message */
				update_new_msg(&new_msg, &new_msg_len, new_mapping->new_templ->templ);
			}

			mapping_insert(src->mapping, new_mapping);
		}
		/* Now we have appropriate mapping for this data set */

		/* Replace template and count references */
		MSG_DEBUG(msg_module, "Have mapping from %d:%d to %d:%d", orig_odid, orig_tid, new_mapping->new_odid, new_mapping->new_templ->templ->template_id);

		new_mapping->new_templ->templ->references += orig_templ->references;
		msg->data_couple[i].data_template = new_mapping->new_templ->templ;
		msg->data_couple[i].data_template->references = 0;
	}

	if (new_msg != NULL) {
		struct ipfix_header *header = (struct ipfix_header *) new_msg;

		header->export_time = msg->pkt_header->export_time;
		header->version = msg->pkt_header->version;
		header->sequence_number = msg->pkt_header->sequence_number;
		header->length = htons(new_msg_len);
		header->observation_domain_id = htonl(src->mapping->new_odid);

		int tmp = htons(2);
		memcpy(new_msg + IPFIX_HEADER_LENGTH, &tmp, 2);
		tmp = htons(new_msg_len - IPFIX_HEADER_LENGTH);
		memcpy(new_msg + IPFIX_HEADER_LENGTH + 2, &tmp, 2);
		struct ipfix_message *new_message = message_create_from_mem(new_msg, new_msg_len, src->input_info, msg->source_status);
//		msg->templ_set[0] = new_message->templ_set[0];
		pass_message(conf->ip_config, (void *) new_message);
	}

	/* Change ODID to new and pass message */
	msg->input_info = src->input_info;
	msg->pkt_header->observation_domain_id = htonl(src->mapping->new_odid);
	pass_message(conf->ip_config, (void *) msg);
	return 0;
}

int intermediate_plugin_close(void *config)
{
	int i;
	struct joinflows_ip_config *conf;

	conf = (struct joinflows_ip_config *) config;

	for (i = 0; i < conf->map_counter; i++) {
		mapping_destroy(&(conf->mappings[i]));
	}

	free(conf);

	return 0;
}
