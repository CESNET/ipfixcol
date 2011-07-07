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

static int tm_fill_template(struct ipfix_template *template, void *template_record, int type)
{
	struct ipfix_template_record *tmpl = (struct ipfix_template_record*) template_record;

	/* set attributes common to both template types */
	template->template_type = type;
	template->last_transmission = time(NULL);
	template->field_count = tmpl->count;
	template->template_id = tmpl->template_id;

	/* set type specific attributes */
	if (type == 0) { /* template */
		template->scope_field_count = 0;
		memcpy(template->fields, tmpl->fields, template->field_count * sizeof(template_ie));
	} else { /* option template */
		template->scope_field_count = ((struct ipfix_options_template_record*) template_record)->scope_field_count;
		if (template->scope_field_count == 0) {
			VERBOSE (CL_VERBOSE_OFF, "Option template scope field count is 0");
			return 1;
		}
		/* \todo check whether copied filed count is correct */
		memcpy(&template->fields[0], ((struct ipfix_options_template_record*) template_record)->fields,
				template->field_count * sizeof(template_ie));
	}
	return 0;
}

int tm_add_template(struct ipfix_template_mgr *tm, void *template, int type)
{
	struct ipfix_template *new_tmpl = NULL;
	struct ipfix_template **new_templates = NULL;
	int i;

	/* allocate memory for new template */
	if ((new_tmpl = malloc(sizeof(struct ipfix_template))) == NULL) {
		VERBOSE (CL_VERBOSE_OFF, "Memory allocation failed (%s:%d)", __FILE__, __LINE__);
	}

	if (tm_fill_template(new_tmpl, template, type) == 1) {
		free(new_tmpl);
		return 1;
	}

	/* check whether allocated memory is big enough */
	if (tm->counter == tm->max_length) {
		new_templates = realloc(tm->templates, tm->max_length*2);
		if (new_templates == NULL) {
			VERBOSE (CL_VERBOSE_OFF, "Memory allocation failed (%s:%d)", __FILE__, __LINE__);
			free(new_tmpl);
			return 1;
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

	return 0;
}


int tm_update_template(struct ipfix_template_mgr *tm, void *template, int type)
{
	struct ipfix_template *tmpl;

	tmpl = tm_get_template(tm, ((struct ipfix_template_record*) template)->template_id);
	return tm_fill_template(tmpl, template, type);
}


struct ipfix_template *tm_get_template(struct ipfix_template_mgr *tm, uint16_t template_id)
{
	int i;

	for (i=0; i < tm->counter; i++) {
		if (tm->templates[i] != NULL && tm->templates[i]->template_id == template_id) {
			return tm->templates[i];
		}
	}
	/* template not found */
	return NULL;
}

int tm_remove_template(struct ipfix_template_mgr *tm, uint16_t template_id)
{
	int i;

	for (i=0; i < tm->counter; i++) {
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

int tm_remove_all_templates(struct ipfix_template_mgr *tm)
{
	int i;

	for (i=0; i < tm->counter; i++) {
		if (tm->templates[i] != NULL) {
			free(tm->templates[i]);
			tm->templates[i] = NULL;
		}
	}
	return 0;
}
