/**
 * \file hooks.c
 * \author Michal Kozubik <kozubik@cesnet.cz>
 * \brief Intermediate Process that is able to exec hooks for events
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
 * \defgroup hooksInter Hooks Intermediate Process
 * \ingroup intermediatePlugins
 *
 * This plugin calls user-specified operations when some event occures
 * (e.g. new exporter connected)
 *
 * @{
 */


#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

#include <ipfixcol.h>
#include <libxml/tree.h>

/*
 * HOWTO add new hook type
 * 1) add hook name at the end of array "hook_names"
 * 2) add new hook type into "enum hooks" right before "HOOK_NONE"
 * 3) in process_message, call "hook_do_operations(conf->hooks[NEW_HOOK_TYPE])" when your hook is triggered
 */

/* API version constant */
IPFIXCOL_API_VERSION;

/* module name for MSG_* */
static const char *msg_module = "hooks";

/* hook names in startup configuration */
static const char *hook_names[] = {
	"exporterConnected",
	"exporterDisconnected"
};

/* hook types */
enum hooks {
	EXPORTER_NEW,
	EXPORTER_CLOSED,
	HOOK_NONE,
};

/* operations list */
struct operation_s {
	char *operation;
	struct operation_s *next;
};

/* plugin's configuration structure */
struct hooks_ip_config {
	void *ip_config;
	struct operation_s *hooks[HOOK_NONE];  
	/*
	 * hooks[EXPORTER_NEW]    - operations executed when new exporter is connected
	 * hooks[EXPORTER_CLOSED] - operations executed when exporter is disconnected
	 */
};

/**
 * \brief Decode hook name into type
 * 
 * @param hook hook name
 * @return hook type
 */
int hooks_decode_name(char *hook)
{
	int i;
	for (i = 0; i < HOOK_NONE; ++i) {
		if (!strcasecmp(hook, hook_names[i])) {
			break;
		}
	}
	
	return i;
}

/**
 * \brief Add new hook into configuration
 * 
 * @param conf configuration
 * @param type hook type
 * @param op operation
 */
void hooks_add_hook(struct hooks_ip_config *conf, int type, struct operation_s *op)
{
	if (conf->hooks[type]) {
		op->next = conf->hooks[type];
	}
	
	conf->hooks[type] = op;
}

/**
 * \brief Initialize plugin
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
	(void) ip_id;
	(void) template_mgr;
	struct hooks_ip_config *conf;
	conf = (struct hooks_ip_config *) calloc(1, sizeof(*conf));
	if (!conf) {
		MSG_ERROR(msg_module, "Unable to allocate memory (%s:%d)", __FILE__, __LINE__);
		return -1;
	}

	int type, len;
	xmlDoc *doc = NULL;
	xmlNode *root = NULL, *hook = NULL, *operation = NULL;
	xmlChar *aux_char = NULL;;
	struct operation_s *aux_op = NULL;
	
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

	/* Parse xml */
	for (hook = root->children; hook; hook = hook->next) {
		aux_char = xmlGetProp(hook, (const xmlChar *) "name");
		if (!aux_char) {
			MSG_ERROR(msg_module, "Hook name not specified, skipping");
			continue;
		}
		
		/* decode hook name */
		type = hooks_decode_name((char *) aux_char);
		if (type == HOOK_NONE) {
			MSG_ERROR(msg_module, "Unknown hook \"%s\", skipping", (char *) aux_char);
			xmlFree(aux_char);
			continue;
		}
		xmlFree(aux_char);
		
		/* get hook's operations */
		for (operation = hook->children; operation; operation = operation->next) {
			aux_op = calloc(1, sizeof(struct operation_s));
			if (!aux_op) {
				MSG_ERROR(msg_module, "Unable to allocate memory (%s:%d)", __FILE__, __LINE__);
				goto cleanup_err;
			}
			
			/* get operation */
			aux_char = xmlNodeListGetString(doc, operation->children, 1);
			len = strlen((char *) aux_char);
			
			aux_op->operation = calloc(1, len + 2);
			if (!aux_op->operation) {
				MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)", __FILE__, __LINE__);
				goto cleanup_err;
			}

			strncpy_safe(aux_op->operation, (char *) aux_char, len + 1);
			
			/* add & to the end (if not present) */
			if (aux_op->operation[len - 1] != '&') {
				aux_op->operation[len] = '&';
				aux_op->operation[len + 1] = '\0';
			}
			
			hooks_add_hook(conf, type, aux_op);
			xmlFree(aux_char);
		}
	}
	
	/* Print active hooks */
	for (type = 0; type < HOOK_NONE; ++type) {
		if (conf->hooks[type]) {
			MSG_DEBUG(msg_module, "Operations for hook \"%s\":", hook_names[type]);
			for (aux_op = conf->hooks[type]; aux_op; aux_op = aux_op->next) {
				MSG_DEBUG(msg_module, "%s", aux_op->operation);
			}
		}
	}
	
	xmlFreeDoc(doc);
	
	conf->ip_config = ip_config;
	*config = conf;
	MSG_NOTICE(msg_module, "Successfully initialized");
	return 0;
	
cleanup_err:
	if (doc) {
		xmlFreeDoc(doc);
	}

	if (aux_op) {
		if (aux_op->operation) {
			free(aux_op->operation);
		}

		free(aux_op);
	}

	free(conf);
	return -1;
}

/**
 * \brief Proccess all operations in list
 * 
 * @param op operations
 */
void hooks_do_operations(struct operation_s *op)
{
	struct operation_s *aux_op;
	
	for (aux_op = op; aux_op; aux_op = aux_op->next) {
		if (system(aux_op->operation) == -1) {
			MSG_ERROR(msg_module, "Error when running \"%s\"", aux_op->operation);
		}
	}
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
	struct hooks_ip_config *conf = (struct hooks_ip_config *) config;
	struct ipfix_message  *msg  = (struct ipfix_message  *) message;
	
	/* Check source status for exporter (dis)connect hooks */
	switch (msg->source_status) {
	case SOURCE_STATUS_NEW:
		hooks_do_operations(conf->hooks[EXPORTER_NEW]);
		break;
	case SOURCE_STATUS_CLOSED:
		hooks_do_operations(conf->hooks[EXPORTER_CLOSED]);
		break;
	default:
		/* no hook */
		break;
	}
	
	pass_message(conf->ip_config, (void *) msg);
	return 0;
}

/**
 * \brief Free operation structure
 * 
 * @param op operation
 */
void hooks_free_operation(struct operation_s *op)
{
	free(op->operation);
	free(op);
}

/**
 * \brief Close intermediate plugin
 * 
 * @param config plugin configuration
 * @return 0 on success
 */
int intermediate_close(void *config)
{
	struct hooks_ip_config *conf;
	struct operation_s *aux_op;
	int type;
	
	conf = (struct hooks_ip_config *) config;
	
	/* free all hooks */
	for (type = 0; type < HOOK_NONE; ++type) {
		while (conf->hooks[type]) {
			aux_op = conf->hooks[type];
			conf->hooks[type] = aux_op->next;
			hooks_free_operation(aux_op);
		}
	}
	
	free(conf);

	return 0;
}

/**@}*/
