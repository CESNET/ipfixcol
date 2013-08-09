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

#include "data_manager.h"

/** Identifier to MSG_* macros */
static char *msg_module = "data manager";

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
 * \brief Process new template (only one)
 *
 * This function process templates as well as options templates. It takes
 * advantage of the fact that the structures are similar and elements
 * template_id and count are at the same position in memory.
 *
 * @param[in] template_mgr Template manager
 * @param[in] set ipfix template or options template set
 * @param[in] type Type of templates to process (TM_TEMPLATE, TM_OPTIONS_TEMPLATE)
 * @param[in] msg_counter counter of ipfix messages for UDP templates
 * @param[in] input_info input info structure from ipfix message
 * @return int size of processed template in bytes on success, 0 otherwise
 */
static int data_manager_process_one_template(struct ipfix_template_mgr *template_mgr, void *tmpl,
						int max_len, int type, uint32_t msg_counter, struct input_info *input_info) {
	struct ipfix_template_record *template_record;
	struct ipfix_template *template;
	int ret;

	template_record = (struct ipfix_template_record*) tmpl;

	/* check for withdraw all templates message */
	/* these templates are no longer used (checked in data_manager_withdraw_templates()) */
	if (input_info->type == SOURCE_TYPE_UDP && ntohs(template_record->count) == 0) {
		/* got withdrawal message with UDP -> this is wrong */
		MSG_WARNING(msg_module, "Got template withdraw message on UDP. Ignoring.");
		return TM_TEMPLATE_WITHDRAW_LEN;
	} else if ((ntohs(template_record->template_id) == IPFIX_TEMPLATE_FLOWSET_ID ||
				ntohs(template_record->template_id) == IPFIX_OPTION_FLOWSET_ID) &&
			ntohs(template_record->count) == 0) {
		/* withdraw template or option template */
		tm_remove_all_templates(template_mgr, type);
		/* don't try to parse the withdraw template */
		return TM_TEMPLATE_WITHDRAW_LEN;
		/* check for withdraw template message */
	} else if (ntohs(template_record->count) == 0) {
		ret = tm_remove_template(template_mgr, ntohs(template_record->template_id));
		/* Log error when removing unknown template */
		if (ret == 1) {
			MSG_WARNING(msg_module, "%s withdraw message received for unknown Template ID: %u",
					(type==TM_TEMPLATE)?"Template":"Options template", ntohs(template_record->template_id));
		}
		return TM_TEMPLATE_WITHDRAW_LEN;
		/* check whether template exists */
	} else if ((template = tm_get_template(template_mgr, ntohs(template_record->template_id))) == NULL) {
		/* add template */
		/* check that the template has valid ID ( < 256 ) */
		if (ntohs(template_record->template_id) < 256) {
			MSG_WARNING(msg_module, "%s ID %i is reserved and not valid for data set!", (type==TM_TEMPLATE)?"Template":"Options template", ntohs(template_record->template_id));
		} else {
			MSG_NOTICE(msg_module, "New %s ID %i", (type==TM_TEMPLATE)?"template":"options template", ntohs(template_record->template_id));
			template = tm_add_template(template_mgr, tmpl, max_len, type);
		}
	} else {
		/* template already exists */
		MSG_WARNING(msg_module, "%s ID %i already exists. Rewriting.",
				(type==TM_TEMPLATE)?"Template":"Options template", template->template_id);
		MSG_DEBUG(msg_module, "going to update template");
		template = tm_update_template(template_mgr, tmpl, max_len, type);
	}
	if (template == NULL) {
		MSG_WARNING(msg_module, "Cannot parse %s set, skipping to next set",
				(type==TM_TEMPLATE)?"template":"options template");
		return 0;
		/* update UDP timeouts */
	} else if (input_info->type == SOURCE_TYPE_UDP) {
		template->last_message = msg_counter;
		template->last_transmission = time(NULL);
	}

	/* return the length of the original template:
	 * = template length - template header length + template record header length */
	if (type == TM_TEMPLATE) {
		return template->template_length - sizeof(struct ipfix_template) + sizeof(struct ipfix_template_record);
	}
	return template->template_length - sizeof(struct ipfix_template) + sizeof(struct ipfix_options_template_record);
}

/**
 * \brief Process templates
 *
 * Currently template management does not conform to RFC 5101 in following:
 * - If template is reused without previous withdrawal or timeout (UDP),
 *   only warning is logged and template is updated (template MUST be of
 *   the same length).
 * - If template is not found, data is not coupled with template,
 *   i.e. data_set[x]->template == NULL
 * - When template is malformed and cannot be added to template manager,
 *   rest of the template set is discarded (template length cannot be
 *   determined)
 *
 * @param[in,out] template_mgr	Template manager
 * @param[in] msg IPFIX			message
 * @param[in] udp_conf			UDP template configuration
 * @return uint32_t Number of received data records
 */
static uint32_t data_manager_process_templates(struct ipfix_template_mgr *template_mgr, struct ipfix_message *msg,
												struct udp_conf *udp_conf, uint32_t msg_counter)
{
	uint8_t *ptr;
	uint32_t records_count = 0;
	int i, ret;
	int max_len;     /* length to the end of the set = max length of the template */
	uint16_t count;
	uint16_t length = 0;
	uint16_t offset = 0;
	uint8_t var_len = 0;
	struct ipfix_template *template;
	uint16_t min_data_length;
	uint16_t data_length;
	/* check for new templates */
	for (i=0; msg->templ_set[i] != NULL && i<1024; i++) {
		ptr = (uint8_t*) &msg->templ_set[i]->first_record;
		while (ptr < (uint8_t*) msg->templ_set[i] + ntohs(msg->templ_set[i]->header.length)) {
			max_len = ((uint8_t *) msg->templ_set[i] + ntohs(msg->templ_set[i]->header.length)) - ptr;
			ret = data_manager_process_one_template(template_mgr, ptr, max_len, TM_TEMPLATE, msg_counter, msg->input_info);
			if (ret == 0) {
				break;
			} else {
				ptr += ret;
			}
		}
	}

	/* check for new option templates */
	for (i=0; msg->opt_templ_set[i] != NULL && i<1024; i++) {
		ptr = (uint8_t*) &msg->opt_templ_set[i]->first_record;
		max_len = ((uint8_t *) msg->opt_templ_set[i] + ntohs(msg->opt_templ_set[i]->header.length)) - ptr;
		while (ptr < (uint8_t*) msg->opt_templ_set[i] + ntohs(msg->opt_templ_set[i]->header.length)) {
			ret = data_manager_process_one_template(template_mgr, ptr, max_len, TM_OPTIONS_TEMPLATE, msg_counter, msg->input_info);
			if (ret == 0) {
				break;
			} else {
				ptr += ret;
			}
		}
	}

	/* add template to message data_couples */
	for (i=0; msg->data_couple[i].data_set != NULL && i<1023; i++) {
		msg->data_couple[i].data_template = tm_get_template(template_mgr, ntohs(msg->data_couple[i].data_set->header.flowset_id));
		if (msg->data_couple[i].data_template == NULL) {
			MSG_WARNING(msg_module, "Data template with ID %i not found!", ntohs(msg->data_couple[i].data_set->header.flowset_id));
		} else {
			if ((msg->input_info->type == SOURCE_TYPE_UDP) && /* source UDP */
					((time(NULL) - msg->data_couple[i].data_template->last_transmission > udp_conf->template_life_time) || /* lifetime expired */
					(udp_conf->template_life_packet > 0 && /* life packet should be checked */
					(uint32_t) (msg_counter - msg->data_couple[i].data_template->last_message) > udp_conf->template_life_packet))) {
				MSG_WARNING(msg_module, "Data template ID %i expired! Using old template.",
				                                               msg->data_couple[i].data_template->template_id);
			}

			/* compute sequence number */
			if (msg->data_couple[i].data_template->data_length & 0x80000000) {
				/* damn... there is a Information Element with variable length. we have to
				 * compute number of the Data Records in current Set by hand */

				/* template for current Data Set */
				template = msg->data_couple[i].data_template;
				/* every Data Record has to be at least this long */
				min_data_length = (uint16_t) msg->data_couple[i].data_template->data_length;

				data_length = ntohs(msg->data_couple[i].data_set->header.length) - sizeof(struct ipfix_set_header);


				length = 0;   /* position in Data Record */
				while (data_length - length >= min_data_length) {
					offset = 0;
					for (count = 0; count < template->field_count; count++) {
						if (template->fields[offset].ie.length == 0xffff) {
							/* this element has variable length. read first byte from actual data record to determine
							 * real length of the field */
							var_len = msg->data_couple[i].data_set->records[length];
							if (var_len < 255) {
								/* field length is var_len */
								length += var_len;
								length += 1; /* first 1 byte contains information about field length */
							} else {
								/* field length is more than 255, actual length is stored in next two bytes */
								length += ntohs(*((uint16_t *) (msg->data_couple[i].data_set->records + length + 1)));
								length += 3; /* first 3 bytes contain information about field length */
							}
						} else {
							/* length from template */
							length += template->fields[offset].ie.length;
						}

						if (template->fields[offset].ie.id & 0x8000) {
							/* do not forget on Enterprise Number */
							offset += 1;
						}
						offset += 1;
					}

					/* data record found */
					records_count += 1;
				}
			} else {
				/* no elements with variable length */
				records_count += ntohs(msg->data_couple[i].data_set->header.length) / msg->data_couple[i].data_template->data_length;
			}

			/* Checking Template Set expiration */
			MSG_DEBUG(msg_module, "Checking references and followers");
			if ((--(msg->data_couple[i].data_template->references)) == 0) {
				if (msg->data_couple[i].data_template->next == NULL) {
					/* If there is no newer Template with the same ID, remove it from template manager */
					MSG_DEBUG(msg_module, "Zero references and no follower, going to remove");
					tm_remove_template(template_mgr, msg->data_couple[i].data_template->template_id);
				} else {
					/* Replace this Template with next one (with the same ID) */
					MSG_DEBUG(msg_module, "No references, but has follower(s)");
					int cnt = 1;
					struct ipfix_template *last = msg->data_couple[i].data_template;
					while (last->next != NULL) {
						last = last->next;
						cnt++;
					}
					int j, count=0;
					/* the array may have holes, thus the counter */
					for (j=0; j < template_mgr->max_length && count < template_mgr->counter; j++) {
						if (template_mgr->templates[j] != NULL) {
							if (template_mgr->templates[j]->template_id == msg->data_couple[i].data_template->template_id) {
								break;
							}
							count++;
						}
					}
					/* Set the follower as new "main" template on this index */
					MSG_DEBUG(msg_module, "Setting new template (follower of the old one) with ID %d on index %d",
							msg->data_couple[i].data_template->template_id, j);
					struct ipfix_template *old = msg->data_couple[i].data_template;
					msg->data_couple[i].data_template = msg->data_couple[i].data_template->next;
					MSG_DEBUG(msg_module, "free(old)");
					free(old);
					template_mgr->templates[j] = msg->data_couple[i].data_template;
				}
			} else {
				MSG_DEBUG(msg_module, "Has some reference(s), keep it");
			}
		}
	}
	int j, cnt=0;
	/* the array may have holes, thus the counter */
	for (j=0; j < template_mgr->max_length && cnt < template_mgr->counter; j++) {
		if (template_mgr->templates[j] != NULL) {
			MSG_DEBUG(msg_module, "template %d id: %d", j, template_mgr->templates[j]->template_id);
			cnt++;
		}
	}
	/* return number of data records */
	return records_count;
}


/**
 * \brief Check whether there is at least one template withdraw
 *
 * @param[in] msg IPFIX message
 * @return int Return 1 when template withdraw message found, 0 otherwise
 */
static int data_manager_withdraw_templates(struct ipfix_message *msg) {
	struct ipfix_template_record *template_record;
	struct ipfix_options_template_record *options_template_record;
	int i;

	/* check for template withdraw messagess (no other templates should be in the set) */
	for (i=0; msg->templ_set[i] != NULL && i<1024; i++) {
		template_record = &msg->templ_set[i]->first_record;
		if (ntohs(template_record->count) == 0) {
			return 1;
		}
	}

	/* check for options template withdraw messagess */
	for (i=0; msg->opt_templ_set[i] != NULL && i<1024; i++) {
			options_template_record = &msg->opt_templ_set[i]->first_record;
			if (ntohs(options_template_record->count) == 0) {
				return 1;
			}
	}
	return 0;
}

/**
 * \brief Fill in udp_info structure when managing UDP input
 *
 * @param[in] input_info 	Input information from input plugin
 * @param[in,out] udp_conf 	UDP template configuration structure to be filled in
 * @return void
 */
static void data_manager_udp_init (struct input_info_network *input_info, struct udp_conf *udp_conf) {
	if (input_info->type == SOURCE_TYPE_UDP) {
		if (((struct input_info_network*) input_info)->template_life_time != NULL) {
			udp_conf->template_life_time = atoi(((struct input_info_network*) input_info)->template_life_time);
		} else {
			udp_conf->template_life_time = TM_UDP_TIMEOUT;
		}
		if (input_info->template_life_packet != NULL) {
			udp_conf->template_life_packet = atoi(input_info->template_life_packet);
		} else {
			udp_conf->template_life_packet = 0;
		}
		if (input_info->options_template_life_time != NULL) {
			udp_conf->options_template_life_time = atoi(((struct input_info_network*) input_info)->options_template_life_time);
		} else {
			udp_conf->options_template_life_time = TM_UDP_TIMEOUT;
		}
		if (input_info->options_template_life_packet != NULL) {
			udp_conf->options_template_life_packet = atoi(input_info->options_template_life_packet);
		} else {
			udp_conf->options_template_life_packet = 0;
		}
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
	struct udp_conf udp_conf;
	unsigned int index;
	uint32_t sequence_number = 0, msg_counter = 0;
	/* set the thread name to reflect the configuration */
	snprintf(config->thread_name, 16, "ipfixcol DM %d", config->observation_domain_id);
	prctl(PR_SET_NAME, config->thread_name, 0, 0, 0);

	/* initialise UDP timeouts */
	data_manager_udp_init((struct input_info_network*) config->input_info, &udp_conf);

	/* loop will break upon receiving NULL from buffer */
	while (1) {
        index = -1;

		/* read new data */
		msg = rbuffer_read (config->in_queue, &index);
		if (msg != NULL) {
			msg_counter++;

			/* check sequence number */
			/* \todo handle out of order messages */
			if (sequence_number != ntohl(msg->pkt_header->sequence_number)) {
				MSG_WARNING(msg_module, "Sequence number does not match: expected %u, got %u",
						sequence_number, ntohl(msg->pkt_header->sequence_number));
				sequence_number = ntohl(msg->pkt_header->sequence_number);
			}
			/* Check whether there are withdraw templates (not for UDP) */
			if (config->input_info->type != SOURCE_TYPE_UDP &&  data_manager_withdraw_templates(msg)) {
				MSG_NOTICE(msg_module, "Got template withdrawal message. Waiting for storage packets.");
				/* wait for storage plugins to consume all pending messages */
				while (rbuffer_wait_empty(config->store_queue));
			}

			/* process templates */
			sequence_number += data_manager_process_templates(config->template_mgr, msg, &udp_conf, msg_counter);
		}

		/* pass data into the storage plugins */
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

		/* all done, mark data as processed */
		rbuffer_remove_reference(config->thread_config->queue, index, 1);

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
	int retval, oid_specific_plugins = 0;
	struct storage_list* aux_storage;
	struct data_manager_config *config;
	struct storage_thread_conf* plugin_cfg;
	/* prepare Data manager's config structure */
	config = (struct data_manager_config*) calloc (1, sizeof(struct data_manager_config));
	if (config == NULL) {
		MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)", __FILE__, __LINE__);
		return (NULL);
	}

	/* initiate queue to communicate with IPFIX preprocessor */
	config->in_queue = rbuffer_init(RING_BUFFER_SIZE);
	if (config->in_queue == NULL) {
		MSG_ERROR(msg_module, "Unable to initiate queue for communication with IPFIX preprocessor.");
		data_manager_free (config);
		return (NULL);
	}
	/* initiate queue to communicate with storage plugins' threads */
	config->store_queue = rbuffer_init(RING_BUFFER_SIZE);
	if (config->store_queue == NULL) {
		MSG_ERROR(msg_module, "Unable to initiate queue for communication with Storage plugins.");
		data_manager_free (config);
		return (NULL);
	}

	config->observation_domain_id = observation_domain_id;
	config->storage_plugins = NULL;
	config->plugins_count = 0;
	config->input_info = input_info;
	config->template_mgr = tm_create(atoi(((struct input_info_network *) input_info)->template_life_packet));


	/* check whether there is OID specific plugin for this OID */
	for (aux_storage = storage_plugins; aux_storage != NULL; aux_storage = aux_storage->next) {
		if (storage_plugins->storage.xml_conf->observation_domain_id != NULL &&
			atol(storage_plugins->storage.xml_conf->observation_domain_id) == config->observation_domain_id) {
			oid_specific_plugins++;
		}
	}

	/* initiate all storage plugins */
	while (storage_plugins) {

		/* check whether storage plugin is ment for this OID */
		if ((storage_plugins->storage.xml_conf->observation_domain_id != NULL && /* OID set and does not match */
			atol(storage_plugins->storage.xml_conf->observation_domain_id) != config->observation_domain_id) ||
			(storage_plugins->storage.xml_conf->observation_domain_id == NULL && /* OID not set, but specific plugin(s) found*/
			oid_specific_plugins > 0)) {
			/* skip to next storage plugin */
			storage_plugins = storage_plugins->next;
			continue;
		}

		/* allocate memory for copy of storage structure for description of storage plugin */
		aux_storage = (struct storage_list*) malloc (sizeof(struct storage_list));
		if (aux_storage == NULL) {
			MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)", __FILE__, __LINE__);
			storage_plugins = storage_plugins->next;
			continue;
		}

		/* copy the original storage_structure */
		memcpy (aux_storage, storage_plugins, sizeof(struct storage_list));

		/* initiate storage plugin */
		xmlDocDumpMemory (aux_storage->storage.xml_conf->xmldata, &plugin_params, NULL);
		retval = aux_storage->storage.init ((char*) plugin_params, &(aux_storage->storage.config));
		if (retval != 0) {
			MSG_WARNING(msg_module, "Initiating storage plugin failed.");
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
			MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)", __FILE__, __LINE__);
			aux_storage->storage.close (&(aux_storage->storage.config));
			storage_plugins = storage_plugins->next;
			continue;
		}
		plugin_cfg->queue = config->store_queue;
		plugin_cfg->template_mgr = config->template_mgr;
		aux_storage->storage.thread_config = plugin_cfg;
		if (pthread_create(&(plugin_cfg->thread_id), NULL, &storage_plugin_thread, (void*) &aux_storage->storage) != 0) {
			MSG_ERROR(msg_module, "Unable to create storage plugin thread.");
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
