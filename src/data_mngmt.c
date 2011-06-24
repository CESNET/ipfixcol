/**
 * \file data_mngmt.c
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
#include "config.h"
#include "data_mngmt.h"

extern volatile int done;

/**
 * \brief Deallocate Data manager's configuration structure.
 *
 * @param config Configuration structure to destroy.
 */
static inline void data_manager_config_free (struct data_manager_config* config)
{
	struct storage* aux_storage;

	if (config != NULL) {
		while (config->plugins) {
			aux_storage = config->plugins;
			config->plugins = config->plugins->next;
			aux_storage->close (&(aux_storage->config));
            if (aux_storage->thread_config != NULL) {
                free(aux_storage->thread_config);
            }
			free (aux_storage);
		}
		if (config->in_queue != NULL) {
			rbuffer_free(config->in_queue);
		}
		if (config->store_queue != NULL) {
			rbuffer_free(config->store_queue);
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
	struct data_manager_config* config = (struct data_manager_config*) cfg;
    struct storage *aux_storage = config->plugins;
	struct ipfix_message* msg;
	unsigned int index = -1;

	while (!done) {
		/* read new data */
		msg = rbuffer_read (config->in_queue, &index);

		/** \todo do templates management */
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
			VERBOSE (CL_VERBOSE_ADVANCED, "ODID %d: No more data from IPFIX preprocessor.",
					config->observation_domain_id);
			break;
		}


        /* TODO check whether we should not always use -1 */
        index = (index+1)  % config->in_queue->size;
	}
    
    /* close all storage plugins */
    while (aux_storage) {
        pthread_join(aux_storage->thread_config->thread_id, NULL);
        aux_storage = aux_storage->next;
    }

	VERBOSE (CL_VERBOSE_ADVANCED, "ODID %d: Closing Data manager's thread.",
			config->observation_domain_id);

    /* free data manager config structure */
    data_manager_config_free(config);

	return (NULL);
}


static void* storage_plugin_thread (void* cfg)
{
    struct storage *config = (struct storage*) cfg; 
	struct ipfix_message* msg;
	unsigned int index = config->thread_config->queue->read_offset;

    /* loop will be ended by pthread_cancel() which should be accepted 
        at pthread_cond_wait() */
	while (1) {
		/* get next data */
		msg = rbuffer_read (config->thread_config->queue, &index);
		if (msg == NULL) {
			VERBOSE (CL_VERBOSE_BASIC, "Error on reading data from Data manager.");
			//continue;
            break;
		}

		/* do the job */
		/**
		 *  \todo Last parameter is supposed to be ipfix_template_t* but I
		 * thing that it is not enough - we need to pass ipfix_template_mgr_t*
		 */
		config->store (config->config, msg, NULL);

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
    /* TODO close data manager thread here  */  
    rbuffer_write((*config)->in_queue, NULL, 1);
    pthread_join((*config)->thread_id, NULL);
    *config = NULL;
    /* deallocate config structure */
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
struct data_manager_config* create_data_manager (
    uint32_t observation_domain_id,
    struct storage* storage_plugins,
    struct input_info *input_info)
{
	xmlChar *plugin_params;
	int retval;
	struct storage* aux_storage;
	struct data_manager_config *config;
	struct storage_plugin_thread_cfg* plugin_cfg;

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
		data_manager_config_free (config);
		return (NULL);
	}
	/* initiate queue to communicate with storage plugins' threads */
	config->store_queue = rbuffer_init(RING_BUFFER_SIZE);
	if (config->store_queue == NULL) {
		VERBOSE(CL_VERBOSE_OFF, "Unable to initiate queue for communication with Storage plugins.");
		data_manager_config_free (config);
		return (NULL);
	}

	config->observation_domain_id = observation_domain_id;
	config->plugins = NULL;
	config->plugins_count = 0;
    config->input_info = input_info;


	/* initiate all storage plugins */
	while (storage_plugins) {
		/* allocate memory for copy of storage structure for description of storage plugin */
		aux_storage = (struct storage*) malloc (sizeof(struct storage));
		if (aux_storage == NULL) {
			VERBOSE (CL_VERBOSE_OFF, "Memory allocation failed (%s:%d)", __FILE__, __LINE__);
			storage_plugins = storage_plugins->next;
			continue;
		}

		/* copy the original storage_structure */
		memcpy (aux_storage, storage_plugins, sizeof(struct storage));

		/* initiate storage plugin */
		xmlDocDumpMemory (aux_storage->plugin->xmldata, &plugin_params, NULL);
		retval = aux_storage->init ((char*) plugin_params, &(aux_storage->config));
		if (retval != 0) {
			VERBOSE(CL_VERBOSE_OFF, "Initiating storage plugin failed.");
			xmlFree (plugin_params);
			storage_plugins = storage_plugins->next;
			continue;
		}
		storage_plugins->config = aux_storage->config;
		xmlFree (plugin_params);

		/* check the links in list of plugins available from data manager's config */
		if (config->plugins) {
			aux_storage->next = config->plugins;
		} else {
			aux_storage->next = NULL;
		}

		/* create storage plugin thread */
		plugin_cfg = (struct storage_plugin_thread_cfg*) malloc (sizeof (struct storage_plugin_thread_cfg));
		if (plugin_cfg == NULL) {
			VERBOSE (CL_VERBOSE_OFF, "Memory allocation failed (%s:%d)", __FILE__, __LINE__);
			aux_storage->close (&(aux_storage->config));
			storage_plugins = storage_plugins->next;
			continue;
		}
		plugin_cfg->queue = config->store_queue;
        aux_storage->thread_config = plugin_cfg;
		if (pthread_create(&(plugin_cfg->thread_id), NULL, &storage_plugin_thread, (void*) aux_storage) != 0) {
			VERBOSE(CL_VERBOSE_OFF, "Unable to create storage plugin thread.");
			aux_storage->close (&(aux_storage->config));
			free (plugin_cfg);
            aux_storage->thread_config = NULL;
			storage_plugins = storage_plugins->next;
			continue;
		}

		/* store initiated plugin record into the list of storage plugins of this data manager */
		config->plugins = aux_storage;
		config->plugins_count++;

		/* continue on the following storage plugin */
		storage_plugins = storage_plugins->next;
	}

	/* check if at least one storage plugin initiated */
	if (config->plugins_count == 0) {
		VERBOSE(CL_VERBOSE_OFF, "No storage plugin for the Data manager initiated.");
		data_manager_config_free (config);
		return (NULL);
	}

	/* create new thread of data manager */
	if (pthread_create(&(config->thread_id), NULL, &data_manager_thread, (void*)config) != 0) {
		VERBOSE(CL_VERBOSE_OFF, "Unable to create data manager thread.");
		data_manager_config_free (config);
		return (NULL);
	}

	return (config);
}
