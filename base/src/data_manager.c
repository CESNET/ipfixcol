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
#include <pthread.h>
#include <libxml/tree.h>
#include <sys/prctl.h>
#include <ipfixcol/verbose.h>

#include "data_manager.h"

/** Identifier to MSG_* macros */
static char *msg_module = "data manager";

/** Ring buffer size */
extern int ring_buffer_size;

/**
 * \brief
 *
 * Structure holding UDP specific template configuration
 */
struct udp_conf {
	uint16_t template_life_time;
	uint16_t template_life_packet;
	uint16_t options_template_life_time;
	uint16_t options_template_life_packet;
};

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

	/* set the thread name to reflect the configuration */
	snprintf(config->thread_name, 16, "ipfixcol DM %d", config->observation_domain_id);
	prctl(PR_SET_NAME, config->thread_name, 0, 0, 0);

	/* initialise UDP timeouts */
//	data_manager_udp_init((struct input_info_network*) config->input_info, &udp_conf);

	/* loop will break upon receiving NULL from buffer */
	while (1) {
        index = -1;

		/* read new data */
		msg = rbuffer_read (config->in_queue, &index);
		if (rbuffer_write (config->store_queue, msg, config->plugins_count) != 0) {
			MSG_WARNING(msg_module, "ODID %d: Unable to pass data into the Storage plugins' queue.",
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
        	MSG_NOTICE(msg_module, "ODID %d: No more data from IPFIX preprocessor.",
					config->observation_domain_id);
			break;
		}
	}
    
    /* close all storage plugins */
    while (aux_storage) {
        pthread_join(aux_storage->storage.thread_config->thread_id, NULL);
        aux_storage = aux_storage->next;
    }

	MSG_NOTICE(msg_module, "ODID %d: Closing Data manager's thread.",
			config->observation_domain_id);

	return (NULL);
}

static void* storage_plugin_thread (void* cfg)
{
    struct storage *config = (struct storage*) cfg; 
	struct ipfix_message* msg;
	unsigned int index = config->thread_config->queue->read_offset;
	int i;

	/* set the thread name to reflect the configuration */
	prctl(PR_SET_NAME, config->thread_name, 0, 0, 0);

    /* loop will break upon receiving NULL from buffer */
	while (1) {
		/* get next data */
		msg = rbuffer_read (config->thread_config->queue, &index);
		if (msg == NULL) {
			MSG_NOTICE("storage plugin thread", "No more data from Data manager.");
            break;
		}


		/* do the job */
		config->store (config->config, msg, config->thread_config->template_mgr);

		for (i=0; msg->data_couple[i].data_set != NULL && i<1023; i++) {
			if (msg->data_couple[i].data_template != NULL) {
				msg->data_couple[i].data_template->references--;
			}
		}

		/* all done, mark data as processed */
//		rbuffer_remove_reference(config->thread_config->queue, index, 1);

		/* move the index */
		index = (index + 1) % config->thread_config->queue->size;
	}

	MSG_NOTICE("storage plugin thread", "Closing storage plugin's thread.");
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
    rbuffer_write((*config)->in_queue, NULL, (*config)->plugins_count);
    pthread_join((*config)->thread_id, NULL);


    rbuffer_free((*config)->in_queue);
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
    struct storage_list* storage_plugins)
{
	xmlChar *plugin_params;
	int retval, oid_specific_plugins = 0;
	struct storage_list* aux_storage;
	struct storage_list *aux_storage_list;
	struct data_manager_config *config;
	struct storage_thread_conf* plugin_cfg;

	/* prepare Data manager's config structure */
	config = (struct data_manager_config*) calloc (1, sizeof(struct data_manager_config));
	if (config == NULL) {
		MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)", __FILE__, __LINE__);
		return (NULL);
	}

	/* initiate queue to communicate with IPFIX preprocessor */
	config->in_queue = rbuffer_init(ring_buffer_size);
	if (config->in_queue == NULL) {
		MSG_ERROR(msg_module, "Unable to initiate queue for communication with IPFIX preprocessor.");
		data_manager_free (config);
		return (NULL);
	}
	/* initiate queue to communicate with storage plugins' threads */
	config->store_queue = rbuffer_init(ring_buffer_size);
	if (config->store_queue == NULL) {
		MSG_ERROR(msg_module, "Unable to initiate queue for communication with Storage plugins.");
		data_manager_free (config);
		return (NULL);
	}

	config->observation_domain_id = observation_domain_id;
	config->storage_plugins = NULL;
	config->plugins_count = 0;
//	config->input_info = input_info;
//	config->template_mgr = tm_create();

	/* check whether there is OID specific plugin for this OID */
	for (aux_storage = storage_plugins; aux_storage != NULL; aux_storage = aux_storage->next) {
		if (storage_plugins->storage.xml_conf->observation_domain_id != NULL &&
			atol(storage_plugins->storage.xml_conf->observation_domain_id) == config->observation_domain_id) {
			oid_specific_plugins++;
		}
	}

	/* initiate all storage plugins */
	aux_storage_list = storage_plugins;
	while (aux_storage_list) {

		/* check whether storage plugin is ment for this OID */
		if ((aux_storage_list->storage.xml_conf->observation_domain_id != NULL && /* OID set and does not match */
			atol(aux_storage_list->storage.xml_conf->observation_domain_id) != config->observation_domain_id) ||
			(aux_storage_list->storage.xml_conf->observation_domain_id == NULL && /* OID not set, but specific plugin(s) found*/
			oid_specific_plugins > 0)) {
			/* skip to next storage plugin */
			aux_storage_list = aux_storage_list->next;
			continue;
		}

		/* allocate memory for copy of storage structure for description of storage plugin */
		aux_storage = (struct storage_list*) malloc (sizeof(struct storage_list));
		if (aux_storage == NULL) {
			MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)", __FILE__, __LINE__);
			aux_storage_list = aux_storage_list->next;
			continue;
		}

		/* copy the original storage_structure */
		memcpy (aux_storage, aux_storage_list, sizeof(struct storage_list));

		/* initiate storage plugin */
		xmlDocDumpMemory (aux_storage->storage.xml_conf->xmldata, &plugin_params, NULL);
		retval = aux_storage->storage.init ((char*) plugin_params, &(aux_storage->storage.config));
		if (retval != 0) {
			MSG_WARNING(msg_module, "Initiating storage plugin failed.");
			xmlFree (plugin_params);
			aux_storage_list = aux_storage_list->next;
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
			MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)", __FILE__, __LINE__);
			aux_storage->storage.close (&(aux_storage->storage.config));
			aux_storage_list = aux_storage_list->next;
			continue;
		}
		plugin_cfg->queue = config->store_queue;
//		plugin_cfg->template_mgr = config->template_mgr;
		aux_storage->storage.thread_config = plugin_cfg;
		if (pthread_create(&(plugin_cfg->thread_id), NULL, &storage_plugin_thread, (void*) &aux_storage->storage) != 0) {
			MSG_ERROR(msg_module, "Unable to create storage plugin thread.");
			aux_storage->storage.close (&(aux_storage->storage.config));
			free (plugin_cfg);
			aux_storage->storage.thread_config = NULL;
			aux_storage_list = aux_storage_list->next;
			continue;
		}

		/* store initiated plugin record into the list of storage plugins of this data manager */
		config->storage_plugins = aux_storage;
		config->plugins_count++;

		/* continue on the following storage plugin */
		aux_storage_list = aux_storage_list->next;
	}

	/* check if at least one storage plugin initiated */
	if (config->plugins_count == 0) {
		MSG_WARNING(msg_module, "No storage plugin for the Data manager initiated.");
		data_manager_free (config);
		return (NULL);
	}

	/* create new thread of data manager */
	if (pthread_create(&(config->thread_id), NULL, &data_manager_thread, (void*)config) != 0) {
		MSG_ERROR(msg_module, "Unable to create data manager thread.");
		data_manager_free (config);
		return (NULL);
	}

	return (config);
}
