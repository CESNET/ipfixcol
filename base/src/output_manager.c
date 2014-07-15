/**
 * \file output_manager.c
 * \author Michal Kozubik <kozubik.michal@gmail.com>
 * \brief Distribution of IPFIX message to Data Managers
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
#include <string.h>
#include <pthread.h>
#include <libxml/tree.h>
#include <sys/prctl.h>

#include <ipfixcol.h>

#include "data_manager.h"
#include "output_manager.h"

static const char *msg_module = "output manager";
extern struct ipfix_template_mgr *tm;

/**
 * \brief Search for Data manager handling specified Observation Domain ID
 *
 * \todo: improve search e.g. by some kind of sorting data_managers
 *
 * @param[in] id Observation domain ID of wanted Data manager.
 * @return Desired Data manager's configuration structure if exists, NULL if
 * there is no Data manager for specified Observation domain ID
 */
static struct data_manager_config *get_data_mngmt_config (uint32_t id, struct data_manager_config *data_mngmts)
{
	struct data_manager_config *aux_cfg = data_mngmts;

	while (aux_cfg) {
		if (aux_cfg->observation_domain_id == id) {
			break;
		}
		aux_cfg = aux_cfg->next;
	}

	return (aux_cfg);
}

/**
 * \brief Insert new Data manager into list
 * \param[in] output_manager Output Manager structure
 * \param[in] new_manager New Data manager
 */
void output_manager_insert(struct output_manager_config *output_manager, struct data_manager_config *new_manager)
{
	new_manager->next = NULL;
	if (output_manager->last == NULL) {
		output_manager->data_managers = new_manager;
	} else {
		output_manager->last->next = new_manager;
	}
	output_manager->last = new_manager;
}

/**
 * \brief Remove data manager from list, close it and free templates
 * \param[in] output_manager Output Manager structure
 * \param[in] old_manager Data Manager to remove and close
 */
void output_manager_remove(struct output_manager_config *output_manager, struct data_manager_config *old_manager)
{
	struct data_manager_config *aux_conf = output_manager->data_managers;

	if (aux_conf == old_manager) {
		output_manager->data_managers = old_manager->next;
	}

	while (aux_conf->next) {
		if (aux_conf->next == old_manager) {
			aux_conf->next = old_manager->next;
			if (output_manager->last == old_manager) {
				output_manager->last = aux_conf;
			}
			break;
		} else {
			aux_conf = aux_conf->next;
		}
	}

	if (output_manager->data_managers == NULL) {
		output_manager->last = NULL;
	}
	uint32_t odid = old_manager->observation_domain_id;
	data_manager_close(&old_manager);
	tm_remove_all_odid_templates(tm, odid);
}

/**
 * \brief Output Managers thread
 *
 * @param[in] config configuration structure
 * @return NULL
 */
static void *output_manager_plugin_thread(void* config)
{
	struct output_manager_config *conf = NULL;
	struct data_manager_config *data_config = NULL;
	struct ipfix_message* msg = NULL;
	unsigned int index;

	conf = (struct output_manager_config *) config;
	index = conf->in_queue->read_offset;

	/* set the thread name to reflect the configuration */
	prctl(PR_SET_NAME, "ipfixcol OM", 0, 0, 0);      /* output managers' manager */


    /* loop will break upon receiving NULL from buffer */
	while (1) {
		/* get next data */
		index = -1;
		
		msg = rbuffer_read(conf->in_queue, &index);

		if (!msg) {
			MSG_NOTICE(msg_module, "No more data from core.");
			break;
		}

		/* get appropriate data manager's config according to Observation domain ID */
		data_config = get_data_mngmt_config (msg->input_info->odid, conf->data_managers);
		if (data_config == NULL) {
			/*
			 * no data manager config for this observation domain ID found -
			 * we have a new observation domain ID, so create new data manager for
			 * it
			 */
			data_config = data_manager_create(msg->input_info->odid, conf->storage_plugins);
			if (data_config == NULL) {
				MSG_WARNING(msg_module, "[%u] Unable to create data manager, skipping data.",
						msg->input_info->odid);
				free (msg);
				rbuffer_remove_reference(conf->in_queue, index, 1);
				continue;
			}

		    /* add config to data_mngmts structure */
	    	output_manager_insert(conf, data_config);

	        MSG_NOTICE(msg_module, "[%u] Created new Data manager", msg->input_info->odid);
		}

		if (msg->source_status == SOURCE_STATUS_NEW) {
			/* New source, increment reference counter */
			MSG_DEBUG(msg_module, "[%u] New source", data_config->observation_domain_id);
			data_config->references++;
		} else if (msg->source_status == SOURCE_STATUS_CLOSED) {
			/* Source closed, decrement reference counter */
			MSG_DEBUG(msg_module, "[%u] Closed source", data_config->observation_domain_id);
			data_config->references--;

			if (data_config->references == 0) {
				/* No reference for this ODID, close DM */
				MSG_DEBUG(msg_module, "[%u] No source, releasing templates.", data_config->observation_domain_id);
				output_manager_remove(conf, data_config);
			}
			rbuffer_remove_reference(conf->in_queue, index, 1);
			continue;

		}

		/* Write data into input queue of Storage Plugins */
		if (rbuffer_write(data_config->store_queue, msg, data_config->plugins_count) != 0) {
			MSG_WARNING(msg_module, "[%u] Unable to write into Data manager's input queue, skipping data.", data_config->observation_domain_id);
			rbuffer_remove_reference(conf->in_queue, index, 1);
			free(msg);
			continue;
		}
		
		/* Remove data from queue (without deallocating) */
		rbuffer_remove_reference(conf->in_queue, index, 0);
	}

	MSG_NOTICE(msg_module, "Closing Output Manager's thread.");

	return (void *) 0;
}


/**
 * \brief Creates new Output Manager
 *
 * @param[in] storages list of storage plugin
 * @param[in] in_queue manager's input queue
 * @param[out] config configuration structure
 * @return 0 on success, negative value otherwise
 */
int output_manager_create(struct storage_list *storages, struct ring_buffer *in_queue, void **config) {

	struct output_manager_config *conf;
	int retval;

	/* Allocate new Output Manager's configuration */
	conf = (struct output_manager_config *) calloc(1, sizeof(*conf));
	if (!conf) {
		MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)", __FILE__, __LINE__);
		return -1;
	}

	conf->storage_plugins = storages;
	conf->in_queue = in_queue;

	/* Create Output Manager's thread */
	retval = pthread_create(&(conf->thread_id), NULL, &output_manager_plugin_thread, (void *) conf);
	if (retval != 0) {
		MSG_ERROR(msg_module, "Unable to create output manager's thread.");
		free(conf);
		return -1;
	}

	*config = conf;
	return 0;
}

/**
 * \brief Close Ouput Manager and all Data Managers
 * 
 * @param config Output Manager's configuration
 */
void output_manager_close(void *config) {
	struct output_manager_config *manager = (struct output_manager_config *) config;
	struct data_manager_config *aux_config = NULL, *tmp = NULL;

	/* Stop Output Manager's thread and free input buffer */
	rbuffer_write(manager->in_queue, NULL, 1);
	pthread_join(manager->thread_id, NULL);
	rbuffer_free(manager->in_queue);

	aux_config = manager->data_managers;
	/* Close all data managers */
	while (aux_config) {
		tmp = aux_config;
		aux_config = aux_config->next;
		data_manager_close(&tmp);
	}

	free(manager);
}
