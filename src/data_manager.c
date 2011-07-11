/**
 * \file data_manager.c
 * \author Radek Krejci <rkrejci@cesnet.cz>
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
#include "data_manager.h"

/**
 * \brief Deallocate Data manager's configuration structure.
 *
 * @param config Configuration structure to destroy.
 */
static inline void data_manager_free (struct data_manager_config* config)
{
	struct storage_list *aux_storage;

	if (config != NULL) {
        /* free struct storage  */
		while (config->storage_plugins) {
			aux_storage = config->storage_plugins;
			config->storage_plugins = config->storage_plugins->next;
            /* close storage plugin */
			if (aux_storage->storage.dll_handler) {
                aux_storage->storage.close (&(aux_storage->storage.config));
            }
            /* free thread_config (tread should aready exited)*/
            if (aux_storage->storage.thread_config != NULL) {
                free(aux_storage->storage.thread_config);
            }
            /* pointers in storage were copied and should be closed elsewhere */
            free (aux_storage);
	    }

		if (config->in_queue != NULL) {
			rbuffer_free(config->in_queue);
		}
		if (config->store_queue != NULL) {
			rbuffer_free(config->store_queue);
		}


		if (config->template_mgr != NULL) {
			tm_destroy(config->template_mgr);
		}
		free (config);
	}
}

/**
 * \brief Process templates
 *
 * Currently template management does not conform to RFC 5101 in following:
 * - If template is reused without prewious withdrawal or timeout (UDP),
 *   only warning is logged and template is updated
 * - If template is not found, data is removed from preprocessed array,
 *   but is passed in msg to storage plugin
 * - When template is malformed and cannot be added to template manager,
 *   rest of the template set is discarded
 */
static void data_manager_process_templates(struct ipfix_template_mgr *template_mgr, struct ipfix_message *msg)
{
	struct ipfix_template *template;
	struct ipfix_template_record *template_record;
	struct ipfix_template_record *options_template_record;
	uint8_t *ptr;
	int i;

	/** \todo do templates management */

	/* check for new templates */
	for (i=0; msg->templ_set[i] != NULL && i<1024; i++) {
		ptr = (uint8_t*) &msg->templ_set[i]->first_record;
		while (ptr < (uint8_t*) msg->templ_set[i] + ntohs(msg->templ_set[i]->header.length)) {
			template_record = (struct ipfix_template_record*) ptr;
			/* check for withdraw all templates message */
			if (ntohs(template_record->template_id) == IPFIX_TEMPLATE_FLOWSET_ID &&
					ntohs(template_record->count) == 0) {
				tm_remove_all_templates(template_mgr, TM_TEMPLATE);
				/* check for withdraw template message */
			} else if (ntohs(template_record->count) == 0) {
				tm_remove_template(template_mgr, ntohs(template_record->template_id));
				/* check whether template exists */
			} else if ((template = tm_get_template(template_mgr, ntohs(template_record->template_id))) == NULL) {
				/* add template */
				MSG(0, "New template ID %i", ntohs(template_record->template_id));
				template = tm_add_template(template_mgr, ptr, TM_TEMPLATE);
				/* template already exits */
			} else {
				VERBOSE(CL_VERBOSE_BASIC, "Template ID %i already exists. Rewriting.", template->template_id);
				template = tm_update_template(template_mgr, ptr, TM_TEMPLATE);
			}
			if (template == NULL) {
				VERBOSE(CL_VERBOSE_BASIC, "Cannot parse template set, skipping to next set");
				break;
			}
			ptr += template->template_length - sizeof(struct ipfix_template) + sizeof(struct ipfix_template_record);
		}
	}

	/* check for new option templates */
	for (i=0; msg->opt_templ_set[i] != NULL && i<1024; i++) {
		ptr = (uint8_t*) &msg->opt_templ_set[i]->first_record;
		while (ptr < (uint8_t*) msg->opt_templ_set[i] + ntohs(msg->opt_templ_set[i]->header.length)) {
			options_template_record = (struct ipfix_template_record*) ptr;
			/* check for withdraw all option templates message */
			if (ntohs(options_template_record->template_id) == IPFIX_OPTION_FLOWSET_ID &&
					ntohs(options_template_record->count) == 0) {
				tm_remove_all_templates(template_mgr, TM_OPTIONS_TEMPLATE);
				/* check for withdraw option template message */
			} else if (ntohs(options_template_record->count) == 0) {
				tm_remove_template(template_mgr, ntohs(options_template_record->template_id));
				/* check whether option template exists */
			} else if ((template = tm_get_template(template_mgr, ntohs(options_template_record->template_id))) == NULL) {
				/* add template */
				MSG(0, "New option template ID %i", ntohs(options_template_record->template_id));
				template = tm_add_template(template_mgr, ptr, TM_OPTIONS_TEMPLATE);
				/* option template already exits */
			} else {
				VERBOSE(CL_VERBOSE_BASIC, "Option template ID %i already exists. Rewriting.", template->template_id);
				template = tm_update_template(template_mgr, ptr, TM_OPTIONS_TEMPLATE);
			}
			if (template == NULL) {
				VERBOSE(CL_VERBOSE_BASIC, "Cannot parse option template set, skipping to next set");
				break;
			}
			ptr += template->template_length - sizeof(struct ipfix_template) + sizeof(struct ipfix_options_template_record);
		}
	}


	/* add template to message data_couples */
	for (i=0; msg->data_set[i].data_set != NULL && i<1023; i++) {
		msg->data_set[i].template = tm_get_template(template_mgr, ntohs(msg->data_set[i].data_set->header.flowset_id));
		if (msg->data_set[i].template == NULL) {
			VERBOSE(CL_VERBOSE_OFF, "Data template with ID %i not found!", ntohs(msg->data_set[i].data_set->header.flowset_id));
			msg->data_set[i].data_set = NULL;
		} else {
			/* check UDP template timeout \todo  should be configurable */
			if ((msg->input_info->type == SOURCE_TYPE_UDP) && (time(NULL) - msg->data_set[i].template->last_transmission > 300)) {
				VERBOSE(CL_VERBOSE_BASIC, "Data template ID %i expired! Using old template.", msg->data_set[i].template->template_id);
			}
		}
	}
	return;
}

/**
 * \brief Thread routine for new Data manager (new Observation Domain ID).
 *
 * @param[in] config Data manager configuration (is internally typecasted to
 * struct data_manager_config)
 */
static void* data_manager_thread (void* cfg)
{
	struct data_manager_config *config = (struct data_manager_config*) cfg;
    struct storage_list *aux_storage = config->storage_plugins;
	struct ipfix_message *msg;
	unsigned int index;

	/* loop will break upon receiving NULL from buffer */
	while (1) {
        index = -1;

		/* read new data */
		msg = rbuffer_read (config->in_queue, &index);

		/* process templates */
		if (msg != NULL) {
			data_manager_process_templates(config->template_mgr, msg);
		}

		/* pass data into the storage plugins */
		if (rbuffer_write (config->store_queue, msg, config->plugins_count) != 0) {
			VERBOSE (CL_VERBOSE_BASIC, "ODID %d: Unable to pass data into the Storage plugins' queue.",
					config->observation_domain_id);
			rbuffer_remove_reference (config->in_queue, index, 1);
			continue;
		}

		/*
		 * data are now in store_queue, so we can remove it from in_queue, but
		 * we cannot deallocate data - it will be done in store_queue
		 */
		rbuffer_remove_reference (config->in_queue, index, 0);

        /* passing NULL message means closing */
        if (msg == NULL) {
			VERBOSE (CL_VERBOSE_ADVANCED, "ODID %d: No more data from IPFIX preprocessor.exit(1);",
					config->observation_domain_id);
			break;
		}
	}
    
    /* close all storage plugins */
    while (aux_storage) {
        pthread_join(aux_storage->storage.thread_config->thread_id, NULL);
        aux_storage = aux_storage->next;
    }

	VERBOSE (CL_VERBOSE_ADVANCED, "ODID %d: Closing Data manager's thread.",
			config->observation_domain_id);

	return (NULL);
}


static void* storage_plugin_thread (void* cfg)
{
    struct storage *config = (struct storage*) cfg; 
	struct ipfix_message* msg;
	unsigned int index = config->thread_config->queue->read_offset;

    /* loop will break upon receiving NULL from buffer */
	while (1) {
		/* get next data */
		msg = rbuffer_read (config->thread_config->queue, &index);
		if (msg == NULL) {
			VERBOSE (CL_VERBOSE_BASIC, "No more data from Data manager.");
            break;
		}

		/* do the job */
		config->store (config->config, msg, config->thread_config->template_mgr);

		/* all done, mark data as processed */
		rbuffer_remove_reference(config->thread_config->queue, index, 1);

		/* move the index */
		index = (index + 1) % config->thread_config->queue->size;
	}

	VERBOSE (CL_VERBOSE_ADVANCED, "Closing storage plugin's thread.");
	return (NULL);
}

/**
 * \brief Close Data manager specified by its configuration
 *
 * @param config Configuration structure of the manager
 */
void data_manager_close (struct data_manager_config **config)
{
    /* close data manager thread - write NULL  */
    rbuffer_write((*config)->in_queue, NULL, 1);
    pthread_join((*config)->thread_id, NULL);
    /* deallocate config structure */
    data_manager_free(*config);
    *config = NULL;

    return;
}

/**
 * \brief Initiate Data manager's config structure and create a thread executing
 * Data manager's code.
 *
 * @param observation_domain_id Observation Domain ID handled by this Data
 * manager.
 * @param storage_plugins List of storage plugins for this Data manager.
 * @return Configuration structure of created Data manager.
 */
struct data_manager_config* data_manager_create (
    uint32_t observation_domain_id,
    struct storage_list* storage_plugins,
    struct input_info *input_info)
{
	xmlChar *plugin_params;
	int retval;
	struct storage_list* aux_storage;
	struct data_manager_config *config;
	struct storage_thread_conf* plugin_cfg;

	/* prepare Data manager's config structure */
	config = (struct data_manager_config*) calloc (1, sizeof(struct data_manager_config));
	if (config == NULL) {
		VERBOSE (CL_VERBOSE_OFF, "Memory allocation failed (%s:%d)", __FILE__, __LINE__);
		return (NULL);
	}

	/* initiate queue to communicate with IPFIX preprocessor */
	config->in_queue = rbuffer_init(RING_BUFFER_SIZE);
	if (config->in_queue == NULL) {
		VERBOSE(CL_VERBOSE_OFF, "Unable to initiate queue for communication with IPFIX preprocessor.");
		data_manager_free (config);
		return (NULL);
	}
	/* initiate queue to communicate with storage plugins' threads */
	config->store_queue = rbuffer_init(RING_BUFFER_SIZE);
	if (config->store_queue == NULL) {
		VERBOSE(CL_VERBOSE_OFF, "Unable to initiate queue for communication with Storage plugins.");
		data_manager_free (config);
		return (NULL);
	}

	config->observation_domain_id = observation_domain_id;
	config->storage_plugins = NULL;
	config->plugins_count = 0;
    config->input_info = input_info;
    config->template_mgr = tm_create();

	/* initiate all storage plugins */
	while (storage_plugins) {
		/* allocate memory for copy of storage structure for description of storage plugin */
		aux_storage = (struct storage_list*) malloc (sizeof(struct storage_list));
		if (aux_storage == NULL) {
			VERBOSE (CL_VERBOSE_OFF, "Memory allocation failed (%s:%d)", __FILE__, __LINE__);
			storage_plugins = storage_plugins->next;
			continue;
		}

		/* copy the original storage_structure */
		memcpy (aux_storage, storage_plugins, sizeof(struct storage_list));

		/* initiate storage plugin */
		xmlDocDumpMemory (aux_storage->storage.xml_conf->xmldata, &plugin_params, NULL);
		retval = aux_storage->storage.init ((char*) plugin_params, &(aux_storage->storage.config));
		if (retval != 0) {
			VERBOSE(CL_VERBOSE_OFF, "Initiating storage plugin failed.");
			xmlFree (plugin_params);
			storage_plugins = storage_plugins->next;
			continue;
		}
		xmlFree (plugin_params);

		/* check the links in list of plugins available from data manager's config */
		if (config->storage_plugins) {
			aux_storage->next = config->storage_plugins;
		} else {
			aux_storage->next = NULL;
		}

		/* create storage plugin thread */
		plugin_cfg = (struct storage_thread_conf*) malloc (sizeof (struct storage_thread_conf));
		if (plugin_cfg == NULL) {
			VERBOSE (CL_VERBOSE_OFF, "Memory allocation failed (%s:%d)", __FILE__, __LINE__);
			aux_storage->storage.close (&(aux_storage->storage.config));
			storage_plugins = storage_plugins->next;
			continue;
		}
		plugin_cfg->queue = config->store_queue;
		plugin_cfg->template_mgr = config->template_mgr;
        aux_storage->storage.thread_config = plugin_cfg;
		if (pthread_create(&(plugin_cfg->thread_id), NULL, &storage_plugin_thread, (void*) &aux_storage->storage) != 0) {
			VERBOSE(CL_VERBOSE_OFF, "Unable to create storage plugin thread.");
			aux_storage->storage.close (&(aux_storage->storage.config));
			free (plugin_cfg);
            aux_storage->storage.thread_config = NULL;
			storage_plugins = storage_plugins->next;
			continue;
		}

		/* store initiated plugin record into the list of storage plugins of this data manager */
		config->storage_plugins = aux_storage;
		config->plugins_count++;

		/* continue on the following storage plugin */
		storage_plugins = storage_plugins->next;
	}

	/* check if at least one storage plugin initiated */
	if (config->plugins_count == 0) {
		VERBOSE(CL_VERBOSE_OFF, "No storage plugin for the Data manager initiated.");
		data_manager_free (config);
		return (NULL);
	}

	/* create new thread of data manager */
	if (pthread_create(&(config->thread_id), NULL, &data_manager_thread, (void*)config) != 0) {
		VERBOSE(CL_VERBOSE_OFF, "Unable to create data manager thread.");
		data_manager_free (config);
		return (NULL);
	}

	return (config);
}
