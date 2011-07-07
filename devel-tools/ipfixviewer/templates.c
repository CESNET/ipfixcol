/**
 * \file templates.c
 * \author Michal Srb <michal.srb@cesnet.cz>
 * \brief Template manager implementation for ipfix-viewer tool.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <arpa/inet.h>
#include "ipfix.h"
#include "templates.h"


#define TEMPLATE_MANAGER_DEFAULT_SIZE 100


int tm_get_template_index(struct ipfix_template_mgr_t *tm, uint16_t template_id);

int tm_init(void **config)
{
	struct ipfix_template_mgr_t *conf;

	conf = (struct ipfix_template_mgr_t *) malloc(sizeof(*conf));
	if (!conf) {
		fprintf(stderr, "Out of memory... (%s:%d)\n", __FILE__, __LINE__);
		exit(EXIT_FAILURE);
	}
	memset(conf, 0, sizeof(*conf));

	/* initial length of the array for templates */
	conf->max_length = TEMPLATE_MANAGER_DEFAULT_SIZE;	

	conf->templates = (struct ipfix_template_t **) malloc(sizeof(struct ipfix_template_t *) * conf->max_length);
	if (!conf->templates) {
		fprintf(stderr, "Out of memory... (%s:%d)\n", __FILE__, __LINE__);
		exit(EXIT_FAILURE);
	}

	*config = conf;
	
	return 0;
}


int tm_exit(void *config)
{
	struct ipfix_template_mgr_t *conf;
	conf = (struct ipfix_template_mgr_t *) config;

	free(config);

	return 0;
}



struct ipfix_template_t *tm_add_template(struct ipfix_template_mgr_t *tm, void *template,
                    int type)
{
	struct ipfix_template_t *templ;
	struct ipfix_template_t *old_templ;
	struct ipfix_template_record *rec;
	struct ipfix_options_template_record *opt_rec;
	uint16_t count = 0;
	uint16_t offset = 0;	/* length of the template record */
	uint16_t index;
	size_t fields_offset;
	int templ_index;
	int i;

	rec = (struct ipfix_template_record *) template;
	
	offset += 4;            /* because of template header */
	index = count;

	/* find out length of the template */
	while (count != ntohs(rec->count)) {
		offset += 4;	/* one field has 4 bytes */
		/* check for enterprise number bit */
		if (ntohs(rec->fields[index].ie.id) >> 15) {
			/* Enterprise number follows */
			++index;
			offset += 4;
		}
		++index;
		++count;
	}

	/* size of the header: Template Record Header = 4, Opt. = 6*/
	offset -= (type == IPFIX_TEMPLATE_FLOWSET_ID) ? 4 : 6;
	/* variable 'offset' now contains exact length of the template */

	/* copy out template */
	fields_offset = offsetof(struct ipfix_template_t, fields);
	templ = (struct ipfix_template_t *) malloc(fields_offset + offset);
	if (!templ) {
		fprintf(stderr, "Out of memory... (%s:%d)\n", __FILE__, __LINE__);
		exit(EXIT_FAILURE);
	}
	memset(templ, 0, fields_offset + offset);
	memcpy(((uint8_t *)templ)+fields_offset, ((uint8_t *) template)+((type == 2) ? 4 : 6), offset);

	templ->fields_length = offset; 

	templ->template_type = type;
	templ->template_id = rec->template_id;
	templ->field_count = rec->count;
	
	if (type == IPFIX_OPTION_FLOWSET_ID) {
		opt_rec = (struct ipfix_options_template_record *) rec;
		templ->scope_field_count = opt_rec->scope_field_count;
	}

	/* check whether there is a place for new template */
	if (tm->counter >= tm->max_length) {
		/* no space, reallocate the array */
		tm->templates = realloc(tm->templates, tm->max_length * 2);
		if (tm->templates == NULL) {
			fprintf(stderr, "Out of memory... (%s:%d)\n", __FILE__, __LINE__);
			exit(EXIT_FAILURE);
		}
		tm->max_length = tm->max_length * 2;
	}

	/* do we have this template already? */
	old_templ = tm_get_template(tm, templ->template_id);
	if (old_templ) {
		/* yep, this template already exists. rewrite it */
		templ_index = tm_get_template_index(tm, templ->template_id);
		free(tm->templates[templ_index]);
		tm->templates[templ_index] = templ;
		return templ;
	}

	/* it's brand new template, find place for it */
	/* FIXME - this algorithm is not nice */
	for (i = 0; i < tm->max_length; i++) {
		if (tm->templates[i] == NULL) {
			/* free space found */
			break;
		}
	}

	tm->templates[i] = templ;
	tm->counter += 1;

	return templ;
}


struct ipfix_template_t *tm_get_template(struct ipfix_template_mgr_t *tm,
                                         uint16_t template_id)
{
	int i;

	struct ipfix_template_t *curr_template;

	if (tm->counter == 0) {
		/* there are no templates... */
		return NULL;
	}

	curr_template = NULL;
	for (i = 0; i < tm->counter; i++) {
		if (tm->templates[i]->template_id == template_id) {
			curr_template = tm->templates[i];
			break;
		}
	}

	return curr_template;	
}

/* get array index of the template */
int tm_get_template_index(struct ipfix_template_mgr_t *tm, uint16_t template_id)
{
	int middle;
	int i;

	struct ipfix_template_t *curr_template;

	if (tm->counter == 0) {
		/* there are no templates... */
		return 0;
	}

	curr_template = NULL;
	for (i = 0; i < tm->counter; i++) {
		if (tm->templates[i]->template_id == template_id) {
			curr_template = tm->templates[i];
			middle = i;
			break;
		}
	}

	return middle;
}



int tm_remove_template(struct ipfix_template_mgr_t *tm,
                       uint16_t template_id)
{
	int index;

	index = tm_get_template_index(tm, template_id);
	if (index == -1) {
		/* no such template */
		return -1;	
	}

	free(tm->templates[index]);
	tm->templates[index] = NULL;
	
	tm->counter -= 1;

	return 0;
}


int tm_remove_all_templates(struct ipfix_template_mgr_t *tm, int type)
{
	int i;
	
	if (tm->counter == 0) {
		return 0;
	}
	
	for (i = 0; i < tm->counter; i++) {
		if (tm->templates[i]->template_type == type) {
			free(tm->templates[i]);
			tm->templates[i] = NULL;
		}
	}

	tm->counter = 0;

	return 0;
}

