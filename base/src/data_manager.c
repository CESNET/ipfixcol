/**
 * \file data_manager.c
 * \author Radek Krejci <rkrejci@cesnet.cz>
 * \author Michal Kozubik <kozubik@cesnet.cz>
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
#include <ipfixcol/storage.h>
#include "configurator.h"
#include "data_manager.h"

/** Identifier to MSG_* macros */
static char *msg_module = "data manager";

/** Ring buffer size */
extern int ring_buffer_size;

/**
 * \brief Deallocate Data manager's configuration structure.
 *
 * @param config Configuration structure to destroy.
 */
static inline void data_manager_free (struct data_manager_config* config)
{
	int i;
	
	if (config) {
		for (i = 0; i < config->plugins_count; ++i) {
			if (config->storage_plugins[i]) {
				/* Close & free plugin */
				if (config->storage_plugins[i]->dll_handler) {
					config->storage_plugins[i]->close(&(config->storage_plugins[i]->config));
				}
				
				if (config->storage_plugins[i]->thread_config) {
					free(config->storage_plugins[i]->thread_config);
				}
				
				free(config->storage_plugins[i]);
				config->storage_plugins[i] = NULL;
			}
		}
		
		/* Free store queue */
		if (config->store_queue) {
			rbuffer_free(config->store_queue);
		}
		
		/* Free DM config */
		free(config);
	}
}

/**
 * \brief Thread for storage plugin
 */
static void* storage_plugin_thread(void *cfg)
{
    struct storage *config = (struct storage*) cfg; 
	struct ipfix_message* msg;
	int can_read = 0, stop = 0;
	unsigned int index = config->thread_config->queue->read_offset;

	/* set the thread name to reflect the configuration */
	prctl(PR_SET_NAME, config->thread_name, 0, 0, 0);

    /* loop will break upon receiving NULL from buffer */
	while (!stop) {
		/* get next data */
		msg = rbuffer_read (config->thread_config->queue, &index);
		if (msg == NULL) {
			MSG_NOTICE("storage plugin thread", "[%u] No more data from Data Manager", config->odid);
            break;
		}
		
		/* Decode message type */
		switch (msg->plugin_status) {
		case PLUGIN_STOP: /* Stop working */
			if (msg->plugin_id == config->id) {
				stop = 1;
			}
			rbuffer_remove_reference(config->thread_config->queue, index, 1);
			break;
		case PLUGIN_START: /* Start reading */
			if (msg->plugin_id == config->id) {
				can_read = 1;
				rbuffer_remove_reference(config->thread_config->queue, index, 1);
			}
			break;
		default: /* DATA */
			if (can_read) {
				config->store (config->config, msg, config->thread_config->template_mgr);
				rbuffer_remove_reference(config->thread_config->queue, index, 1);
			}
			break;
		}
		
		/* move the index */
		index = (index + 1) % config->thread_config->queue->size;
	}

	MSG_NOTICE("storage plugin thread", "[%u] Closing storage plugin thread", config->odid);
	return (NULL);
}

/**
 * \brief Add storage plugin instance
 */
int data_manager_add_plugin(struct data_manager_config *config, struct storage *plugin)
{
	int retval = 0, name_len;
	xmlChar *plugin_params;
	
	/* Check ODID */
	if ((plugin->xml_conf->observation_domain_id != NULL && /* OID set and does not match */
		atol(plugin->xml_conf->observation_domain_id) != config->observation_domain_id) ||
		(plugin->xml_conf->observation_domain_id == NULL && /* OID not set, but specific plugin(s) found*/
		config->oid_specific_plugins > 0)) {
			
		/* skip storage plugin */
		return 0;
	}

	/* Allocate space */
	if (!config->storage_plugins[config->plugins_count]) {
		config->storage_plugins[config->plugins_count] = calloc(1, sizeof(struct storage));
		if (!config->storage_plugins[config->plugins_count]) {
			MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)", __FILE__, __LINE__);
			return 1;
		}
	}
	
	/* Copy plugin data */
	memcpy(config->storage_plugins[config->plugins_count], plugin, sizeof(struct storage));
	plugin = config->storage_plugins[config->plugins_count];
	
	/* Initiate storage plugin */
	xmlDocDumpMemory (plugin->xml_conf->xmldata, &plugin_params, NULL);
	retval = plugin->init ((char*) plugin_params, &(plugin->config));
	xmlFree(plugin_params);
	
	if (retval != 0) {
		MSG_WARNING(msg_module, "[%u] Storage plugin initialization failed", config->observation_domain_id);
		return 0;
	}
	
	/* Create storage plugin thread */
	struct storage_thread_conf *plugin_cfg = calloc (1, sizeof(struct storage_thread_conf));
	if (!plugin_cfg) {
		MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)", __FILE__, __LINE__);
		plugin->close (&(plugin->config));
		return 0;
	}
	
	/* Set plugin's input queue */
	plugin_cfg->queue = config->store_queue;
//		plugin_cfg->template_mgr = config->template_mgr;
	plugin->thread_config = plugin_cfg;
	plugin->odid = config->observation_domain_id;
	
	/* Set thread name */
	name_len = strlen(plugin->thread_name);
	snprintf(plugin->thread_name + name_len, 16 - name_len, " %d", config->observation_domain_id);
	
	/* Create thread */
	if (pthread_create(&(plugin_cfg->thread_id), NULL, &storage_plugin_thread, (void*) plugin) != 0) {
		MSG_ERROR(msg_module, "Unable to create storage plugin thread");
		plugin->close(&(plugin->config));
		free(plugin_cfg);
		plugin->thread_config = NULL;
		return 0;
	}
	
	/* Create START message */
	struct ipfix_message *msg = calloc(1, sizeof(struct ipfix_message));
	msg->plugin_status = PLUGIN_START;
	msg->plugin_id = plugin->id;

	/* Start plugin */
	config->plugins_count++;
	rbuffer_write(config->store_queue, msg, 1);
	
	return 0;
}

/**
 * \brief Remove plugin from data manager
 */
int data_manager_remove_plugin(struct data_manager_config* config, int id)
{
	int i;

	struct storage *plugin = NULL;
	
	/* Find plugin */
	for (i = 0; i < config->plugins_count; ++i) {
		if (config->storage_plugins[i] && config->storage_plugins[i]->id == id) {
			/* Remove from array */
			plugin = config->storage_plugins[i];
			config->storage_plugins[i] = NULL;
			break;
		}
	}
	
	if (plugin) {
		/* Create STOP message */
		struct ipfix_message *msg = calloc(1, sizeof(struct ipfix_message));
		msg->plugin_status = PLUGIN_STOP;
		msg->plugin_id = plugin->id;
		
		/* Wait for plugin termination */
		rbuffer_write(config->store_queue, msg, config->plugins_count);
		pthread_join(plugin->thread_config->thread_id, NULL);
		config->plugins_count--;
	}
	
	return 0;
}

/**
 * \brief Close Data manager specified by its configuration
 *
 * @param config Configuration structure of the manager
 */
void data_manager_close (struct data_manager_config **config)
{
	int i;

	/* close all storage plugins */
	rbuffer_write ((*config)->store_queue, NULL, (*config)->plugins_count);
	for (i = 0; i < (*config)->plugins_count; ++i) {
		if ((*config)->storage_plugins[i]) {
			pthread_join((*config)->storage_plugins[i]->thread_config->thread_id, NULL);
		}
	}

	/* deallocate config structure */
    data_manager_free(*config);
    *config = NULL;
}

/**
 * \brief Initiate Data manager's config structure and create a thread executing Data manager's code.
 */
struct data_manager_config *data_manager_create(uint32_t observation_domain_id, struct storage *storage_plugins[])
{
	int i;
	struct data_manager_config *config = NULL;

	/* prepare Data manager's config structure */
	config = (struct data_manager_config*) calloc(1, sizeof(struct data_manager_config));
	if (config == NULL) {
		MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)", __FILE__, __LINE__);
		return (NULL);
	}

	/* initiate queue to communicate with storage plugins' threads */
	config->store_queue = rbuffer_init(ring_buffer_size);
	if (config->store_queue == NULL) {
		MSG_ERROR(msg_module, "Unable to initiate queue for communication with storage plugins");
		goto err;
	}

	config->observation_domain_id = observation_domain_id;

	/* check whether there is OID specific plugin for this OID */
	for (i = 0; storage_plugins[i]; ++i) {
		if (storage_plugins[i]->xml_conf->observation_domain_id &&
			atol(storage_plugins[i]->xml_conf->observation_domain_id) == config->observation_domain_id) {
			config->oid_specific_plugins++;
		}
	}

	/* initiate all storage plugins */
	for (i = 0; storage_plugins[i]; ++i) {
		data_manager_add_plugin(config, storage_plugins[i]);
	}

	/* check if at least one storage plugin initiated */
	if (config->plugins_count == 0) {
		MSG_WARNING(msg_module, "[%u] No storage plugin for the Data Manager initiated", config->observation_domain_id);
		goto err;
	}
	
	return (config);
	
err:
	if (config) {
		data_manager_free(config);
	}
	return NULL;
}
