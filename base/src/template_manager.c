/**
 * \file template_manager.c
 * \author Petr Velan <petr.velan@cesnet.cz>
 * \brief Data manager implementation.
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

#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <libxml/tree.h>

#include <ipfixcol.h>

/**TEMPLATE_FIELD_LEN length of standard template field */
#define TEMPLATE_FIELD_LEN 4
/**TEMPLATE_ENT_FIELD_LEN length of template enterprise number */
#define TEMPLATE_ENT_NUM_LEN 4

/** Identifier to MSG_* macros */
static char *msg_module = "template manager";

/**
 * \brief Create new Template Manager's record
 */
struct ipfix_template_mgr_record *tm_record_create()
{
	struct ipfix_template_mgr_record *tmr = NULL;
	tmr = calloc(1, sizeof(struct ipfix_template_mgr_record));
	if (!tmr) {
		MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)", __FILE__, __LINE__);
		return NULL;
	}

	/* Allocate space for templates */
	tmr->counter = 0;
	tmr->max_length = 32;
	tmr->templates = calloc(tmr->max_length, sizeof(struct ipfix_template *));
	if (!tmr->templates) {
		MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)", __FILE__, __LINE__);
		free(tmr);
		return NULL;
	}

	return tmr;
}

/**
 * \brief Find template managers record in template manager
 *
 * \param[in] tm Template Manager
 * \param[in] key Unique identifier of template in Template Manager
 * \return pointer to Template Manager's record
 */
struct ipfix_template_mgr_record *tm_record_lookup(struct ipfix_template_mgr *tm, struct ipfix_template_key *key)
{
	struct ipfix_template_mgr_record *tmp_rec;
	uint64_t table_key = ((uint64_t) key->odid << 32) | key->crc;
	
	for (tmp_rec = tm->first; tmp_rec != NULL; tmp_rec = tmp_rec->next) {
		if (tmp_rec->key == table_key) {
			return tmp_rec;
		}
	}
	
	return NULL;
}

/**
 * \brief Find (or insert if not found) template managers record in template manager
 *
 * \param[in] tm Template Manager
 * \param[in] key Unique identifier of template in Template Manager
 * \return pointer to Template Manager's record
 */
struct ipfix_template_mgr_record *tm_record_lookup_insert(struct ipfix_template_mgr *tm, struct ipfix_template_key *key)
{
	pthread_mutex_lock(&tm->tmr_lock);
	struct ipfix_template_mgr_record *tmr = tm_record_lookup(tm, key);

	/* Template Manager's record not found - create a new one */
	if (tmr == NULL) {
		if ((tmr = tm_record_create()) == NULL) {
			pthread_mutex_unlock(&tm->tmr_lock);
			return NULL;
		}
		uint64_t table_key = ((uint64_t) key->odid << 32) | key->crc;
		tmr->key = table_key;
		tmr->next = NULL;
		
		/* Insert new record at the end of list */
		if (tm->first == NULL) {
			tm->first = tmr;
		} else {
			tm->last->next = tmr;
		}
		
		tm->last = tmr;
	}
	
	pthread_mutex_unlock(&tm->tmr_lock);
	return tmr;
}

/**
 * \brief Copy ipfix_template fields and convert them to host byte order
 *
 * \param[out] to Destination
 * \param[in] from Source
 * \param[in] length template size
 */
static void tm_copy_fields(uint8_t *to, uint8_t *from, uint16_t length)
{
	int i;
	uint16_t offset = 0;
	uint8_t *template_ptr, *tmpl_ptr;

	template_ptr = to;
	tmpl_ptr = from;
	while (offset < length) {
		for (i=0; i < TEMPLATE_FIELD_LEN / 2; i++) {
			*((uint16_t*)(template_ptr+offset+i*2)) = ntohs(*((uint16_t*)(tmpl_ptr+offset+i*2)));
		}
		offset += TEMPLATE_FIELD_LEN;
		if (*((uint16_t *) (template_ptr+offset-TEMPLATE_FIELD_LEN)) & 0x8000) { /* enterprise element has first bit set to 1*/
			for (i=0; i < TEMPLATE_ENT_NUM_LEN / 4; i++) {
				*((uint32_t*)(template_ptr+offset)) = ntohl(*((uint32_t*)(tmpl_ptr+offset)));
			}
			offset += TEMPLATE_ENT_NUM_LEN;
		}
	}
	return;
}

/**
 * \brief Fills up ipfix_template structure with data from (options_)template_record
 */
static int tm_fill_template(struct ipfix_template *template, void *template_record, uint16_t template_length,
							uint32_t data_length, int type, uint32_t odid)
{
	struct ipfix_template_record *tmpl = (struct ipfix_template_record*) template_record;

	/* set attributes common to both template types */
	template->template_type = type;
	template->field_count = ntohs(tmpl->count);
	template->template_id = ntohs(tmpl->template_id);
	template->original_id = template->template_id;
	template->template_length = template_length;
	template->data_length = data_length;

	/* set type specific attributes */
	if (type == TM_TEMPLATE) { /* template */
		template->scope_field_count = 0;
		/* copy fields and convert to host byte order */
		tm_copy_fields((uint8_t*)template->fields,
					(uint8_t*)tmpl->fields,
					template_length - sizeof(struct ipfix_template) + sizeof(template_ie));
	} else { /* option template */
		template->scope_field_count = ntohs(((struct ipfix_options_template_record*) template_record)->scope_field_count);
		if (template->scope_field_count == 0) {
			MSG_WARNING(msg_module, "[%u] Option template scope field count is 0", odid);
			return 1;
		}
		tm_copy_fields((uint8_t*)template->fields,
							(uint8_t*)((struct ipfix_options_template_record*) template_record)->fields,
							template_length - sizeof(struct ipfix_template) + sizeof(template_ie));
	}
	
	template->references = 0;
	template->next = NULL;
	template->first_transmission = time(NULL);

	int i;
	for (i = 0; i < OF_COUNT; ++i) {
		template->offsets[i] = -1;
	}

	return 0;
}

/**
 * \brief Calculates ipfix_template length based on (options_)template_record
 *
 * Also fills up data_length - length of data record specified by given template
 *
 */
static uint16_t tm_template_length(struct ipfix_template_record *template, int max_len, int type, uint32_t *data_length)
{
	uint8_t *fields;
	int count;
	uint16_t fields_length = 0;
	uint16_t tmpl_length;
	uint32_t data_record_length = 0;
	uint16_t tmp_data_length;

	tmpl_length = sizeof(struct ipfix_template) - sizeof(template_ie);

	if (type == TM_TEMPLATE) { /* template */
		fields = (uint8_t *) template->fields;
	} else { /* options template */
		fields = (uint8_t *) ((struct ipfix_options_template_record*) template)->fields;
	}

	for (count=0; count < ntohs(template->count); count++) {
		/* count data record length */
		tmp_data_length = ntohs(*((uint16_t *) (fields + fields_length + 2)));

		if (tmp_data_length == 0xffff) {
			/* this Information Element has variable length */
			/* taint this variable. we can't count on it anymore,
			 * but it can tell us what is the smallest length
			 * of the Data Record possible */
			data_record_length |= 0x80000000;
			data_record_length += 1;           /* every field is at least 1 byte long */
		} else {
			/* actual length is stored in the template */
			data_record_length += tmp_data_length;
		}

		/* enterprise element has first bit set to 1 */
		if (ntohs((*((uint16_t *) (fields+fields_length)))) & 0x8000) {
			fields_length += TEMPLATE_ENT_NUM_LEN;
		}
		fields_length += TEMPLATE_FIELD_LEN;

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

/**
 * \brief Get templaterecord length
 */
uint16_t tm_template_record_length(struct ipfix_template_record *template, int max_len, int type, uint32_t *data_length)
{
	uint16_t len = tm_template_length(template, max_len, type, data_length);
	if (len > 0) {
		len = len - sizeof(struct ipfix_template) + sizeof(struct ipfix_template_record);
	}

	return len;
}

/**
 * \brief Create new ipfix template
 *
 * \param[in] template ipfix template data
 * \param[in] max_len maximum length of template
 * \param[in] type type of template (template/option template)
 * \param[in] odid Observation Domain ID
 * \return pointer to new ipfix template
 */
struct ipfix_template *tm_create_template(void *template, int max_len, int type, uint32_t odid)
{
	struct ipfix_template *new_tmpl = NULL;
	uint32_t data_length = 0;
	uint32_t tmpl_length;

	tmpl_length = tm_template_length(template, max_len, type, &data_length);
	if (tmpl_length == 0) {
		/* template->count probably points beyond current set area */
		MSG_WARNING(msg_module, "[%u] Template %d is malformed (bad template count); skipping...", odid,
				ntohs(((struct ipfix_template_record *) template)->template_id));
		return NULL;
	}

	/* allocate memory for new template */
	if ((new_tmpl = malloc(tmpl_length)) == NULL) {
		MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)", __FILE__, __LINE__);
		return NULL;
	}

	if (tm_fill_template(new_tmpl, template, tmpl_length, data_length, type, odid) == 1) {
		free(new_tmpl);
		return NULL;
	}

	return new_tmpl;
}

/**
 * \brief Insert existing template into Template Manager's record
 *
 * \param[in] tmr Template Manager's record
 * \param[in] new_tmpl IPFIX Template
 * \return pointer to inserted template
 */
struct ipfix_template *tm_record_insert_template(struct ipfix_template_mgr_record *tmr, struct ipfix_template *new_tmpl)
{
	struct ipfix_template **new_templates = NULL;
	int i;

	/* check whether allocated memory is big enough */
	if (tmr->counter == tmr->max_length) {
		new_templates = realloc(tmr->templates, tmr->max_length*2*sizeof(void *));
		if (new_templates == NULL) {
			MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)", __FILE__, __LINE__);
			free(new_tmpl);
			return NULL;
		}
		tmr->templates = new_templates;
		memset(tmr->templates + tmr->max_length, 0, tmr->max_length * sizeof(void*));
		tmr->max_length *= 2;
	}

	/* add template to managers record (first position available) */
	for (i = 0; i < tmr->max_length; i++) {
		if (tmr->templates[i] == NULL) {
			tmr->templates[i] = new_tmpl;
			/* increase the counter */
			tmr->counter++;
			break;
		}
	}

	return new_tmpl;
}

/**
 * \brief Add new template into Template Manager's record
 *
 * \param[in] tmr Template Manager's record
 * \param[in] template ipfix template which will be inserted into tmr
 * \param[in] max_len maximum size of template
 * \param[in] type type of template (template/option template)
 * \param[in] odid Observation Domain ID
 * \return pointer to added template
 */
struct ipfix_template *tm_record_add_template(struct ipfix_template_mgr_record *tmr, void *template, int max_len, int type, uint32_t odid)
{
	struct ipfix_template *new_tmpl = NULL;

	if ((new_tmpl = tm_create_template(template, max_len, type, odid)) == NULL) {
		return NULL;
	}

	return tm_record_insert_template(tmr, new_tmpl);
}

/**
 * \brief Remove template from Template Manager's record
 *
 * \param[in] tmr Template Manager's record
 * \param[in] template_id Identification number of template
 * \return 0 if template was found, 1 otherwise
 */
int tm_record_remove_template(struct ipfix_template_mgr_record *tmr, uint16_t template_id)
{
	int i;
	for (i=0; i < tmr->max_length; i++) {
		if (tmr->templates[i] != NULL && tmr->templates[i]->original_id == template_id) {
			struct ipfix_template *next = tmr->templates[i];
			while (next->next != NULL) {
				tmr->templates[i] = next->next;
				free(next);
				next = tmr->templates[i];
			}
			free(tmr->templates[i]);
			tmr->templates[i] = NULL;
			tmr->counter--;
			return 0;
		}
	}
	/* template not found */
	return 1;
}

/**
 * \brief Get template index in template manager's record
 * 
 * \param tmr Template manager's record
 * \param id Template ID
 * \return Template index or -1 if not found
 */
int tm_record_template_index(struct ipfix_template_mgr_record *tmr, uint16_t id)
{
	int i, count = 0;
	/* the array may have holes, thus the counter */
	for (i = 0; i < tmr->max_length && count < tmr->counter; i++) {
		if (tmr->templates[i] != NULL) {
			if (tmr->templates[i]->original_id == id) {
				return i;
			}
			count++;
		}
	}
	
	return -1;
}

int tm_compare_templates(struct ipfix_template *first, struct ipfix_template *second)
{
	if (first->data_length != second->data_length || first->field_count != second->field_count) {
		return 1;
	}

	uint16_t count = first->field_count;

	for (uint16_t i = 0; i < count; ++i) {
		if (first->fields[i].ie.id != second->fields[i].ie.id
				|| first->fields[i].ie.length != second->fields[i].ie.length) {
			return 1;
		}

		if (first->fields[i].ie.id >> 15) {
			i++;
			count++;

			if (first->fields[i].enterprise_number != second->fields[i].enterprise_number) {
				return 1;
			}
		}
	}

	return 0;
}

/**
 * \brief Update template in template managers record
 *
 * \param[in] tmr Template Manager's record
 * \param[in] template ipfix template record
 * \param[in] max_len maximum size of template
 * \param[in] type type of template (template/option template)
 * \param[in] odid Observation Domain ID
 * \return pointer to updated template
 */
struct ipfix_template *tm_record_update_template(struct ipfix_template_mgr_record *tmr, void *template, int max_len, int type, uint32_t odid)
{
	uint16_t id = ntohs(((struct ipfix_template_record *) template)->template_id);
	struct ipfix_template *new_tmpl = NULL;
	
	/* Get template index */
	int i = tm_record_template_index(tmr, id);
	
	if (i < 0) {
		MSG_WARNING(msg_module, "[%u] Template %u cannot be updated (not found); creating new one...", odid, id);
		return tm_record_add_template(tmr, template, max_len, type, odid);
	}
	
	/* save IDs */
	uint16_t templ_id = tmr->templates[i]->template_id;
	
	/* Create new template */
	if ((new_tmpl = tm_create_template(template, max_len, type, odid)) == NULL) {
		return NULL;
	}

	if (tm_compare_templates(new_tmpl, tmr->templates[i]) == 0) {
		/* Templates are the same, no need to update */
		free(new_tmpl);
		MSG_DEBUG(msg_module, "[%u] Received the same template as last time, not replacing", odid);
		return tmr->templates[i];
	}

	new_tmpl->template_id = templ_id;

	if (tmr->templates[i]->references == 0) {
		if (tmr->templates[i]->next == NULL) {
			/* No previous template */
			/* remove the old template */
//			MSG_DEBUG(msg_module, "No references and no previous template - removing, ID %d", id);
			if (tm_record_remove_template(tmr, id) != 0) {
				MSG_WARNING(msg_module, "[%u] Cannot remove template %i", odid, id);
			}
			/* create a new one */
			MSG_DEBUG(msg_module, "Creating new template... %d", id);
			new_tmpl = tm_record_insert_template(tmr, new_tmpl);
			if (new_tmpl) {
				new_tmpl->template_id = templ_id;
			}
			return new_tmpl;
		} else {
			/* Has some previous template(s) */
			MSG_DEBUG(msg_module, "[%u] No references, but previous template found (ID %d)", odid, id);
			struct ipfix_template *new = tmr->templates[i]->next;
			free(tmr->templates[i]);
			tmr->templates[i] = new;
		}
	} else {
		MSG_DEBUG(msg_module, "[%u] Template %d cannot be removed (%u references), but it will be marked as 'old'", odid, id, tmr->templates[i]->references);
	}

	/* Inserting new template */
	new_tmpl->next = tmr->templates[i];
	tmr->templates[i] = new_tmpl;

	MSG_DEBUG(msg_module,"[%u] Template %d added to list", odid, id, i);

	return tmr->templates[i];
}

/**
 * \brief Get pointer to template in Template Manager's record
 *
 * \param[in] tmr Template Manager's record
 * \param[in] template_id Identification number of template
 * \return pointer to ipfix template
 */
struct ipfix_template *tm_record_get_template(struct ipfix_template_mgr_record *tmr, uint16_t template_id)
{
	int i, count=0;
	/* the array may have holes, thus the counter */
	for (i=0; i < tmr->max_length && count < tmr->counter; i++) {
		if (tmr->templates[i] != NULL) {
			if (tmr->templates[i]->original_id == template_id) {
				return tmr->templates[i];
			}
			count++;
		}
	}

	/* template not found */
	return NULL;
}

/**
 * \brief Remove all templates from Template Manager's record
 *
 * \param[in] tm Template Manager
 * \param[in] tmr Template Manager's record
 * \param[in] type Type of templates to remove
 */
void tm_record_remove_all_templates(struct ipfix_template_mgr *tm, struct ipfix_template_mgr_record *tmr, int type)
{
	(void) tm;
	MSG_DEBUG(msg_module, "Removing all %stemplates", (type == TM_TEMPLATE) ? "" : "option ");
	int i;
	for (i=0; i < tmr->max_length; i++) {
		if ((tmr->templates[i] != NULL) && (tmr->templates[i]->template_type == type)) {
			struct ipfix_template *next = tmr->templates[i];
			while (next->next != NULL) {
				tmr->templates[i] = next->next;
				free(next);
				next = tmr->templates[i];
			}
			
			free(tmr->templates[i]);
			tmr->templates[i] = NULL;
		}
	}
}

/**
 * \brief Destroy Template Manager's record
 *
 * \param[in] tm Template Manager
 * \param[in] tmr Template Manager's record
 */
void tm_record_destroy(struct ipfix_template_mgr *tm, struct ipfix_template_mgr_record *tmr)
{
	tm_record_remove_all_templates(tm, tmr, TM_TEMPLATE);  /* Templates */
	tm_record_remove_all_templates(tm, tmr, TM_OPTIONS_TEMPLATE);  /* Options Templates */
	free(tmr->templates);
	free(tmr);
	return;
}

/**
 * \brief Create global Template Manager
 *
 * \return pointer to created Template Manager
 */
struct ipfix_template_mgr *tm_create() {
	struct ipfix_template_mgr *tm;

	if ((tm = malloc(sizeof(struct ipfix_template_mgr))) == NULL) {
		MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)", __FILE__, __LINE__);
		return NULL;
	}

	/* Allocate space for Template Manager's records */
	tm->first = NULL;
	tm->last = NULL;

	/* Initialize mutex */
	pthread_mutex_init(&tm->tmr_lock, NULL);
	
	return tm;
}

/**
 * \brief Destroy global template manager
 *
 * \param[in] tm Template Manager
 */
void tm_destroy(struct ipfix_template_mgr *tm)
{
	if (tm == NULL) {
		return;
	}
	struct ipfix_template_mgr_record *tmp_rec = tm->first;
	
	while (tmp_rec) {
		tmp_rec = tmp_rec->next;
		tm_record_destroy(tm, tm->first);
		tm->first = tmp_rec;
	}
	
	pthread_mutex_destroy(&tm->tmr_lock);
	
	free(tm);
	tm = NULL;
}

/**
 * \brief Add new template into template manager
 */
struct ipfix_template *tm_add_template(struct ipfix_template_mgr *tm, void *template, int max_len, int type, struct ipfix_template_key *key)
{	
	struct ipfix_template_mgr_record *tmr = tm_record_lookup_insert(tm, key);
	
	if (tmr == NULL) {
		return NULL;
	}

	/* Add template to Template Manager */
	return tm_record_add_template(tmr, template, max_len, type, key->odid);
}

/**
 * \brief Insert existing template into Template Manager
 */
struct ipfix_template *tm_insert_template(struct ipfix_template_mgr *tm, struct ipfix_template *tmpl, struct ipfix_template_key *key)
{
	struct ipfix_template_mgr_record *tmr = tm_record_lookup_insert(tm, key);

	if (tmr == NULL) {
		return NULL;
	}
	return tm_record_insert_template(tmr, tmpl);
}

/**
 * \brief Update template in template manager
 */
struct ipfix_template *tm_update_template(struct ipfix_template_mgr *tm, void *template, int max_len, int type, struct ipfix_template_key *key)
{
	struct ipfix_template_mgr_record *tmr = tm_record_lookup_insert(tm, key);

	if (tmr == NULL) {
		return NULL;
	}
	
	struct ipfix_template *templ = tm_record_update_template(tmr, template, max_len, type, key->odid);
	
	return templ;

}

/**
 * \brief Remove template from template manager
 */
int tm_remove_template(struct ipfix_template_mgr *tm, struct ipfix_template_key *key)
{
	struct ipfix_template_mgr_record *tmr = tm_record_lookup(tm, key);

	if (tmr == NULL) {
		return 1;
	}
	
	return tm_record_remove_template(tmr, key->tid);
}

int tm_remove_all_templates(struct ipfix_template_mgr *tm, int type)
{
	(void) tm;
	(void) type;
	/* dodělat */
	return 0;
}

/**
 * \brief Remove all templates for ODID
 */
void tm_remove_all_odid_templates(struct ipfix_template_mgr *tm, uint32_t odid)
{
	struct ipfix_template_mgr_record *prev_rec = tm->first, *aux_rec = tm->first;

	MSG_NOTICE(msg_module, "[%u] Removing all templates", odid);

	while (aux_rec) {
		if (aux_rec->key >> 32 == odid) {
			if (aux_rec == tm->first) {
				if (aux_rec == tm->last) {
					tm->last = NULL;
				}
				tm->first = aux_rec->next;
				tm_record_destroy(tm, aux_rec);
				prev_rec = tm->first;
				aux_rec = tm->first;
			} else {
				if (aux_rec == tm->last) {
					tm->last = prev_rec;
				}
				prev_rec->next = aux_rec->next;
				tm_record_destroy(tm, aux_rec);
				aux_rec = prev_rec->next;
			}
		} else {
			prev_rec = aux_rec;
			aux_rec = aux_rec->next;
		}
	}
}

/**
 * \brief Get pointer to template in template manager
 */
struct ipfix_template *tm_get_template(struct ipfix_template_mgr *tm, struct ipfix_template_key *key)
{
	struct ipfix_template_mgr_record *tmr = tm_record_lookup(tm, key);
	if (tmr == NULL) {
		return NULL;
	}
	return tm_record_get_template(tmr, key->tid);
}

/**
 * \brief Increment number of references to template
 */
void tm_template_reference_inc(struct ipfix_template *templ)
{
	/* This must be atomic */
	__sync_fetch_and_add(&(templ->references), 1);
}

/**
 * \brief Decrement number of references to template
 *
 * \param[in] templ Template
 */
void tm_template_reference_dec(struct ipfix_template *templ)
{
	if (templ->references > 0) {
		/* This must be atomic */
		__sync_fetch_and_sub(&(templ->references), 1);
	}
}

/**
 * \brief Determines whether specific template contains given field and returns
 * the field's offset.
 *
 * \param[in] templ Template
 * \param[in] field Field ID. In case of an enterprise-specific field, the
 * enterprise bit must be set to 1
 * \return Field offset on success, negative value otherwise
 */
int template_contains_field(struct ipfix_template *templ, uint16_t field)
{
	uint16_t fields = 0;
	uint8_t *p;
	uint8_t variable_length = 0;
	uint8_t hit = 0;

	if (!templ) {
		return -1;
	}

	if (templ->template_type == TM_OPTIONS_TEMPLATE) {
		p = (uint8_t *) ((struct ipfix_options_template_record*) templ)->fields;
	} else {
		p = (uint8_t *) templ->fields;
	}

	uint16_t ie_id;
	uint16_t field_length;
	uint16_t total_length = 0;

	while (fields < templ->field_count) {
		ie_id = *((uint16_t *) p);
		field_length = *((uint16_t *) (p + 2));

		if (ie_id == field) {
			hit = 1;
			break;
		}

		/* Count total length, if we don't find element with variable length */
		if (field_length == VAR_IE_LENGTH) {
			variable_length = 1;
		} else {
			total_length += field_length;
		}

		/* Move to next field (may be enterprise ID) */
		p += 4;

		/* Check whether field is enterprise-specific */
		if (ie_id & 0x8000) {
			/* Move to next field */
			p += 4;
		}

		fields += 1;
	}

	if (hit) {
		return (!variable_length) ? total_length : 0;
	}

	/* Field could not be found in specific template */
	return -1;
}

/**
 * \brief Determines whether specific template contains given field and returns
 * the field's offset.
 *
 * \param[in] templ Template
 * \param[in] eid Enterprise ID (zero in case the field is not enterprise-specific)
 * \param[in] fid Field ID
 * \return Field offset on success, negative value otherwise
 */
int template_get_field_offset(struct ipfix_template *templ, uint16_t eid, uint16_t fid)
{
	uint16_t fields = 0;
	uint8_t *p;
	uint8_t variable_length = 0;
	uint8_t hit = 0;

	if (!templ) {
		return -1;
	}

	if (templ->template_type == TM_OPTIONS_TEMPLATE) {
		p = (uint8_t *) ((struct ipfix_options_template_record*) templ)->fields;
	} else {
		p = (uint8_t *) templ->fields;
	}

	uint32_t enterprise_id;
	uint16_t ie_id;
	uint16_t field_length;
	uint16_t total_length = 0;

	while (fields < templ->field_count) {
		ie_id = *((uint16_t *) p);
		field_length = *((uint16_t *) (p + 2));

		if (ie_id == fid) {
			/* 
			 * In case we are not dealing with an enterprise-specific field, we
			 * can stop and return, since we found the field. Otherwise, we have
			 * to check whether the enterprise IDs match.
			 */
			if (eid == 0) {
				hit = 1;
				break;
			}
		}

		/* Count total length, if we don't find element with variable length */
		if (field_length == VAR_IE_LENGTH) {
			variable_length = 1;
		} else {
			total_length += field_length;
		}

		/* Move to next field (may be enterprise ID) */
		p += 4;

		/* Check whether field is enterprise-specific */
		if (eid > 0 && ie_id == fid) {
			enterprise_id = *((uint32_t *) p);

			if (enterprise_id == eid) {
				hit = 1;
				break;
			}

			/* Move to next field */
			p += 4;
		}

		fields += 1;
	}

	if (hit) {
		return (!variable_length) ? total_length : 0;
	}

	/* Field could not be found in specific template */
	return -1;
}

/**
 * \brief Make ipfix_template_key from ODID, crc and template id
 * 
 * @param odid Observation Domain ID
 * @param crc  CRC from source IP and source port
 * @param tid  Template ID
 * @return pointer to ipfix_template_key
 */
struct ipfix_template_key *tm_key_create(uint32_t odid, uint32_t crc, uint32_t tid)
{
	struct ipfix_template_key *key = malloc(sizeof(struct ipfix_template_key));
	if (key == NULL) {
		return NULL;
	}
	
	key->crc = crc;
	key->odid = odid;
	key->tid = tid;
	
	return key;
}

/**
 * \brief Change Template ID in template_key
 *
 * @param key Template identifier in Template Manager
 * @param tid New Template ID
 * @return pointer to changed ipfix_template_key
 */
struct ipfix_template_key *tm_key_change_template_id(struct ipfix_template_key *key, uint32_t tid)
{
	key->tid = tid;
	return key;
}

/**
 * \brief Destroy ipfix_template_key structure
 * 
 * @param key IPFIX template key
 */
void tm_key_destroy(struct ipfix_template_key *key)
{
	free(key);
	key = NULL;
}

/**
 * \brief Compare two template records
 *
 * \param[in] first First template record
 * \param[in] second Second template record
 * \return non-zero if templates are equal
 */
int tm_compare_template_records(struct ipfix_template_record *first, struct ipfix_template_record *second)
{
	/* Check valid pointers */
	if (first == NULL || second == NULL) {
		return 0;
	}

	/* Same pointers? */
	if (first == second) {
		return 1;
	}

	/* Check number of fields */
	if (first->count != second->count) {
		return 0;
	}
	uint16_t *field1 = (uint16_t *) first->fields;
	uint16_t *field2 = (uint16_t *) second->fields;

	/* Check each field ID */
	uint16_t i;
	for (i = 0; i < ntohs(first->count); ++i, field1 += 2, field2 += 2) {
		if (*field1 != *field2) {
			return 0;
		}
	}

	/* Template records are equal */
	return 1;
}
