/**
 * \file filter.c
 * \author Michal Kozubik <kozubik@cesnet.cz>
 * \brief Intermediate plugin for IPFIX data filtering
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
#include <libxml/parser.h>
#include <libxml/tree.h>

#include <ipfixcol.h>

#include "../../intermediate_process.h"

static const char *msg_module = "filter";

struct filter_source {
	uint32_t id;
	struct filter_source *next;
};

struct filter_profile {
	uint32_t new_odid;
	char *filter;
	struct filter_source *sources;
	struct filter_profile *next;
};

struct filter_config {
	void *ip_config;
	struct filter_profile *profiles;
	struct filter_profile *default_profile;
};

/**
 * \brief Initialize filter plugin
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
	struct filter_config *conf = NULL;
	struct filter_profile *aux_profile;
	xmlDoc *doc = NULL;
	xmlNode *root = NULL, *profile = NULL, *node = NULL;

	conf = (struct filter_config *) calloc(1, sizeof(struct filter_config));
	if (!conf) {
		MSG_ERROR(msg_module, "Unable to allocate memory (%s:%d)", __FILE__, __LINE__);
		goto cleanup_err;
	}

	if (!params) {
		MSG_ERROR(msg_module, "Missing plugin configuration!");
		goto cleanup_err;
	}

	doc = xmlParseDoc(BAD_CAST params);
	if (!doc) {
		MSG_ERROR(msg_module, "Cannot parse config xml!");
		goto cleanup_err;
	}

	root = xmlDocGetRootElement(doc);
	if (!root) {
		MSG_ERROR(msg_module, "Cannot get document root element!");
		goto cleanup_err;
	}
	xmlChar *aux_char;

	struct filter_source *aux_src = NULL;

	/* Iterate throught all profiles */
	for (profile = root->children; profile; profile = profile->next) {
		/* Allocate space for profile */
		aux_profile = calloc(1, sizeof(struct filter_profile));
		if (!aux_profile) {
			MSG_ERROR(msg_module, "Unable to allocate memory (%s:%d)", __FILE__, __LINE__);
			goto cleanup_err;
		}
		/* Set new ODID */
		aux_profile->new_odid = atoi((char *) xmlGetProp(profile, (const xmlChar *) "to"));

		/* Get filter string and all sources */
		for (node = profile->children; node; node = node->next) {
			if (!xmlStrcmp(node->name, (const xmlChar *) "from")) {
				/* New source */
				aux_src = calloc(1, sizeof(struct filter_source));
				if (!aux_src) {
					MSG_ERROR(msg_module, "Unable to allocate memory (%s:%d)", __FILE__, __LINE__);
					free(aux_profile);
					goto cleanup_err;
				}
				aux_char = xmlNodeListGetString(doc, node->children, 1);
				aux_src->id = atoi((char *) aux_char);
				xmlFree(aux_char);

				/* Insert new source into list */
				if (!aux_profile->sources) {
					aux_profile->sources = aux_src;
				} else {
					aux_src->next = aux_profile->sources;
					aux_profile->sources = aux_src;
				}
			} else if (!xmlStrcmp(node->name, (const xmlChar *) "filterString")) {
				/* Filter string found */
				aux_profile->filter = (char *) xmlNodeListGetString(doc, node->children, 1);
			}
		}

		/* No filter string -> no profile */
		if (!aux_profile->filter) {
			free(aux_profile);
			continue;
		}

		/* This is default profile */
		if (!xmlStrcasecmp(profile->name, (const xmlChar *) "default")) {
			if (conf->default_profile) {
				MSG_WARNING(msg_module, "Multiple default profiles, using the first one!");
				free(aux_profile);
			} else {
				conf->default_profile = aux_profile;
			}

			continue;
		}


		/* Insert new profile into list */
		if (!conf->profiles) {
			conf->profiles = aux_profile;
		} else {
			aux_profile->next = conf->profiles;
			conf->profiles = aux_profile;
		}
	}

	conf->ip_config = ip_config;

	*config = conf;
	return 0;

cleanup_err:
	if (!conf) {
		return -1;
	}

	if (doc) {
		xmlFreeDoc(doc);
	}

	aux_profile = conf->profiles;

	while (aux_profile) {
		conf->profiles = conf->profiles->next;
		free(aux_profile);
		aux_profile = conf->profiles;
	}

	if (conf->default_profile) {
		free(conf->default_profile);
	}

	free(conf);
	return -1;
}

int filter_apply_profile(struct ipfix_message *msg, struct filter_profile *profile)
{
	return 0;
}

int process_message(void *config, void *message)
{
	struct ipfix_message *msg = (struct ipfix_message *) message;
	struct filter_config *conf = (struct filter_config *) config;
	struct filter_profile *aux_profile = NULL, *profile = NULL;
	struct filter_source *aux_src = NULL;
	int ret;
	uint32_t orig_odid = ntohl(msg->pkt_header->observation_domain_id);


	/* Go throught all profiles */
	for (aux_profile = conf->profiles; aux_profile && !profile; aux_profile = aux_profile->next) {
		/* Go throught all sources for this profile */
		for (aux_src = aux_profile->sources; aux_src; aux_src = aux_src->next) {
			if (aux_src->id == orig_odid) {
				profile = aux_profile;
				break;
			}
		}
	}

	if (!profile) {
		if (conf->default_profile) {
			/* Use default profile */
			profile = conf->default_profile;
		} else {
			/* No profile found for this ODID */
			pass_message(conf->ip_config, message);
			return 0;
		}
	}


	ret = filter_apply_profile(msg, profile);

	if (!ret) {
		return ret;
	}

	pass_message(conf->ip_config, message);
	return 0;
}

int intermediate_plugin_close(void *config)
{
	struct filter_config *conf = (struct filter_config *) config;
	struct filter_profile *aux_profile = conf->profiles;

	while (aux_profile) {
		conf->profiles = conf->profiles->next;
		free(aux_profile);
		aux_profile = conf->profiles;
	}

	free(conf);
	return 0;
}
