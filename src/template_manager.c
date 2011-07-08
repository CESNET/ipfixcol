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
#include <commlbr.h>
#include <pthread.h>
#include <libxml/tree.h>

#include "../ipfixcol.h"

/**TEMPLATE_FIELD_LEN length of standard template field */
#define TEMPLATE_FIELD_LEN 4
/**TEMPLATE_ENT_FIELD_LEN length of template enterprise number */
#define TEMPLATE_ENT_NUM_LEN 4

struct ipfix_template_mgr *tm_create() {
	struct ipfix_template_mgr *template_mgr;

	if ((template_mgr = malloc(sizeof(struct ipfix_template_mgr))) == NULL) {
		VERBOSE (CL_VERBOSE_OFF, "Memory allocation failed (%s:%d)", __FILE__, __LINE__);
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
 * \brief Copy ipfix_template fields and convert then to host byte order
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
		if (*(template_ptr+offset) & 0x8000) { /* enterprise element has first bit set to 1*/
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
static int tm_fill_template(struct ipfix_template *template, void *template_record, uint16_t template_length, int type)
{
	struct ipfix_template_record *tmpl = (struct ipfix_template_record*) template_record;

	/* set attributes common to both template types */
	template->template_type = type;
	template->last_transmission = time(NULL);
	template->field_count = ntohs(tmpl->count);
	template->template_id = ntohs(tmpl->template_id);
	template->template_length = template_length;

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
			VERBOSE (CL_VERBOSE_OFF, "Option template scope field count is 0");
			return 1;
		}
		tm_copy_fields((uint8_t*)template->fields,
							(uint8_t*)((struct ipfix_options_template_record*) template_record)->fields,
							template_length - sizeof(struct ipfix_template) + sizeof(template_ie));
	}
	return 0;
}

/**
 * \brief Calculates ipfix_template length based on (options_)template_record
 */
static uint16_t tm_template_length(struct ipfix_template_record *template, int type)
{
	uint8_t *fields;
	int count;
	uint16_t fields_length=0;
	uint16_t tmpl_length = sizeof(struct ipfix_template) - sizeof(template_ie);

	if (type == TM_TEMPLATE) { /* template */
		fields = (uint8_t *) template->fields;
	} else { /* options template */
		fields = (uint8_t *) ((struct ipfix_options_template_record*) template)->fields;
	}

	for (count=0; count < ntohs(template->count); count++) {
		fields_length += TEMPLATE_FIELD_LEN;
		if (ntohs(fields[fields_length]) & 0x80) { /* enterprise element has first bit set to 1*/
			fields_length += TEMPLATE_ENT_NUM_LEN;
		}
	}

	/* length is in octets, fields_length is 16bit */
	tmpl_length += fields_length;
	return tmpl_length;
}

struct ipfix_template *tm_add_template(struct ipfix_template_mgr *tm, void *template, int type)
{
	struct ipfix_template *new_tmpl = NULL;
	struct ipfix_template **new_templates = NULL;
	uint32_t tmpl_length =  tm_template_length(template, type);
	int i;

	/* allocate memory for new template */
	if ((new_tmpl = malloc(tmpl_length)) == NULL) {
		VERBOSE (CL_VERBOSE_OFF, "Memory allocation failed (%s:%d)", __FILE__, __LINE__);
	}

	if (tm_fill_template(new_tmpl, template, tmpl_length, type) == 1) {
		free(new_tmpl);
		return NULL;
	}

	/* check whether allocated memory is big enough */
	if (tm->counter == tm->max_length) {
		new_templates = realloc(tm->templates, tm->max_length*2);
		if (new_templates == NULL) {
			VERBOSE (CL_VERBOSE_OFF, "Memory allocation failed (%s:%d)", __FILE__, __LINE__);
			free(new_tmpl);
			return NULL;
		}
		tm->templates = new_templates;
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


struct ipfix_template *tm_update_template(struct ipfix_template_mgr *tm, void *template, int type)
{
	struct ipfix_template *tmpl;

	tmpl = tm_get_template(tm, ntohs(((struct ipfix_template_record*) template)->template_id));
	if (tm_fill_template(tmpl, template, tmpl->template_length, type) != 0) {
		return NULL;
	}
	return tmpl;
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
	int i;

	for (i=0; i < tm->max_length; i++) {
		if ((tm->templates[i] != NULL) && (tm->templates[i]->template_type == type)) {
			free(tm->templates[i]);
			tm->templates[i] = NULL;
		}
	}
	return 0;
}
