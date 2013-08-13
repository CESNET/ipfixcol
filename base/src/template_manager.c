/**
 * \file template_manager.c
 * \author Petr Velan <petr.velan@cesnet.cz>
 * \brief Data manager implementation.
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
#include <string.h>
#include <pthread.h>
#include <libxml/tree.h>

#include "ipfixcol.h"

/**TEMPLATE_FIELD_LEN length of standard template field */
#define TEMPLATE_FIELD_LEN 4
/**TEMPLATE_ENT_FIELD_LEN length of template enterprise number */
#define TEMPLATE_ENT_NUM_LEN 4

/** Identifier to MSG_* macros */
static char *msg_module = "template manager";

struct ipfix_template_mgr *tm_create() {
	struct ipfix_template_mgr *template_mgr;

	if ((template_mgr = malloc(sizeof(struct ipfix_template_mgr))) == NULL) {
		MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)", __FILE__, __LINE__);
	}
	template_mgr->counter = 0;
	template_mgr->max_length = 32;
	template_mgr->templates = calloc(template_mgr->max_length, sizeof(void *));

	return template_mgr;
}

void tm_destroy(struct ipfix_template_mgr *tm)
{
	tm_remove_all_templates(tm, TM_TEMPLATE);  /* Templates */
	tm_remove_all_templates(tm, TM_OPTIONS_TEMPLATE);  /* Options Templates */
	free(tm->templates);
	free(tm);
	return;
}

/**
 * \brief Copy ipfix_template fields and convert them to host byte order
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
		tm_copy_fields((uint8_t*)template->fields,
					(uint8_t*)tmpl->fields,
					template_length - sizeof(struct ipfix_template) + sizeof(template_ie));
	} else { /* option template */
		template->scope_field_count = ntohs(((struct ipfix_options_template_record*) template_record)->scope_field_count);
		if (template->scope_field_count == 0) {
			MSG_WARNING(msg_module, "Option template scope field count is 0");
			return 1;
		}
		tm_copy_fields((uint8_t*)template->fields,
							(uint8_t*)((struct ipfix_options_template_record*) template_record)->fields,
							template_length - sizeof(struct ipfix_template) + sizeof(template_ie));
	}
	template->references = 0;
	template->next = NULL;
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

struct ipfix_template *tm_add_template(struct ipfix_template_mgr *tm, void *template, int max_len, int type)
{
	struct ipfix_template *new_tmpl = NULL;
	struct ipfix_template **new_templates = NULL;
	uint32_t data_length = 0;
	uint32_t tmpl_length;
	int i;

	tmpl_length = tm_template_length(template, max_len, type, &data_length);
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

	if (tm_fill_template(new_tmpl, template, tmpl_length, data_length, type) == 1) {
		free(new_tmpl);
		return NULL;
	}

	/* check whether allocated memory is big enough */
	if (tm->counter == tm->max_length) {
		new_templates = realloc(tm->templates, tm->max_length*2*sizeof(void *));
		if (new_templates == NULL) {
			MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)", __FILE__, __LINE__);
			free(new_tmpl);
			return NULL;
		}
		tm->templates = new_templates;
		memset(tm->templates + tm->max_length, 0, tm->max_length * sizeof(void*));
		tm->max_length *= 2;
	}

	/* add template to manager (first position available) */
	for (i = 0; i < tm->max_length; i++) {
		if (tm->templates[i] == NULL) {
			tm->templates[i] = new_tmpl;
			/* increase the counter */
			tm->counter++;
			break;
		}
	}
	return new_tmpl;
}


struct ipfix_template *tm_update_template(struct ipfix_template_mgr *tm, void *template, int max_len, int type)
{
	uint32_t id = ntohs(((struct ipfix_template_record *) template)->template_id);
	int i, count=0;
	/* the array may have holes, thus the counter */
	for (i=0; i < tm->max_length && count < tm->counter; i++) {
		if (tm->templates[i] != NULL) {
			if (tm->templates[i]->template_id == id) {
				break;
			}
			count++;
		}
	}
	if (tm->templates[i]->references == 0) {
		if (tm->templates[i]->next == NULL) {
			/* No previous template */
			/* remove the old template */
//			MSG_DEBUG(msg_module, "No references and no previous template - removing, ID %d", id);
			if (tm_remove_template(tm, id) != 0) {
				MSG_WARNING(msg_module, "Cannot remove template %i.", ntohs(((struct ipfix_template_record*) template)->template_id));
			}
			/* create a new one */
			MSG_DEBUG(msg_module, "Creatign a new one");
			return tm_add_template(tm, template, max_len, type);
		} else {
			/* Has some previous template(s) */
			MSG_DEBUG(msg_module, "No references, but previous template found, ID (%d)", id);
			struct ipfix_template *new = tm->templates[i]->next;
			free(tm->templates[i]);
			tm->templates[i] = new;
		}
	} else {
		MSG_DEBUG(msg_module, "Template %d can't be removed (%d references), it will be marked as old.", id, tm->templates[i]->references);
	}
	/* Create new template and place it on beginnig of list */
	struct ipfix_template *new_templ= NULL;
	uint32_t data_length = 0;
	uint32_t tmpl_length;

	tmpl_length = tm_template_length(template, max_len, type, &data_length);
	if (tmpl_length == 0) {
		/* template->count probably points beyond current set area */
		MSG_WARNING(msg_module, "Template %d is malformed (bad template count), skipping.",
				ntohs(((struct ipfix_template_record *) template)->template_id));
		return NULL;
	}

	/* allocate memory for new template */
	if ((new_templ = malloc(tmpl_length)) == NULL) {
		MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)", __FILE__, __LINE__);
	}

	/* fill in new template */
	if (tm_fill_template(new_templ, template, tmpl_length, data_length, type) == 1) {
		free(new_templ);
		return NULL;
	}

	/* Inserting new template */
	new_templ->next = tm->templates[i];
	tm->templates[i] = new_templ;
	MSG_DEBUG(msg_module,"Template with id %d and index %d added to list", id, i);
	return tm->templates[i];
}


struct ipfix_template *tm_get_template(struct ipfix_template_mgr *tm, uint16_t template_id)
{
	int i, count=0;
	/* the array may have holes, thus the counter */
	for (i=0; i < tm->max_length && count < tm->counter; i++) {
		if (tm->templates[i] != NULL) {
			if (tm->templates[i]->template_id == template_id) {
				return tm->templates[i];
			}
			count++;
		}
	}
	/* template not found */
	return NULL;
}

int tm_remove_template(struct ipfix_template_mgr *tm, uint16_t template_id)
{
	int i;
	for (i=0; i < tm->max_length; i++) {
		if (tm->templates[i] != NULL && tm->templates[i]->template_id == template_id) {
			struct ipfix_template *next = tm->templates[i];
			while (next->next != NULL) {
				tm->templates[i] = next->next;
				free(next);
				next = tm->templates[i];
			}
			free(tm->templates[i]);
			tm->templates[i] = NULL;
			tm->counter--;
			return 0;
		}
	}
	/* template not found */
	return 1;
}

int tm_remove_all_templates(struct ipfix_template_mgr *tm, int type)
{
	MSG_DEBUG(msg_module, "Removing all templates");
	int i;
	for (i=0; i < tm->max_length; i++) {
		if ((tm->templates[i] != NULL) && (tm->templates[i]->template_type == type)) {
			struct ipfix_template *next = tm->templates[i];
			while (next->next != NULL) {
				tm->templates[i] = next->next;
				free(next);
				next = tm->templates[i];
			}
			free(tm->templates[i]);
			tm->templates[i] = NULL;
		}
	}
	return 0;
}
