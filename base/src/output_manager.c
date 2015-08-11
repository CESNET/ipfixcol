/**
 * \file output_manager.c
 * \author Michal Kozubik <kozubik.michal@gmail.com>
 * \brief Distribution of IPFIX message to Data Managers
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

#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <libxml/tree.h>
#include <sys/prctl.h>
#include <ipfixcol.h>
#include <signal.h>
#include <errno.h>
#include <dirent.h>
#include <inttypes.h>
#include <glob.h>
#include <libgen.h>

#include "configurator.h"
#include "data_manager.h"
#include "output_manager.h"

/* MSG_ macros identifiers */
static const char *msg_module = "output manager";
static const char *stat_module = "stat";

/* Output Manager configuration - singleton */
struct output_manager_config *conf = NULL;

/* These nodes are used to keep references to the various input_info data
 * structures used by IPFIX messages that pass this Output Manager
 */
struct input_info_node {
	struct input_info *input_info;
	struct input_info_node *next;

	/* Counter to be used by statistics thread */
	uint64_t last_packets;
	uint64_t last_data_records;
	uint64_t last_lost_data_records;
};

/* List of input_info_node structures */
struct input_info_node *input_info_list = NULL; /* Pointer to first node in list */

/**
 * \brief Add input_info as a node to input_info_list
 *
 * \param[in] node input_info object to add to list
 */
void add_input_info(struct input_info *node) {
	struct input_info_node *aux_node = input_info_list;

	/* Find last position in linked list */
	while (aux_node && aux_node->input_info) {
		aux_node = aux_node->next;
	}

	struct input_info_node *new_node = calloc(1, sizeof(struct input_info_node));
	new_node->input_info = node;

	if (input_info_list) {
		new_node->next = input_info_list;
		input_info_list = new_node;
	} else {
		/* input_info_list is empty */
		new_node->next = NULL;
		input_info_list = new_node;
	}
}

/**
 * \brief Retrieve input_info_node (containing input_info structure) from
 * input_info_list, based on provided ODID
 *
 * \param[in] odid ODID to retrieve input_info for
 */
struct input_info_node *get_input_info_node(uint32_t odid) {
	struct input_info_node *aux_node = input_info_list;
	while (aux_node) {
		if (aux_node->input_info->odid == odid) {
			return aux_node;
		}

		aux_node = aux_node->next;
	}

	return NULL;
}

/**
 * \brief Dummy SIGUSR1 signal handler
 */
void sig_handler(int s) 
{
	(void) s;
}

/**
 * \brief Search for Data manager handling specified Observation Domain ID
 *
 * \todo: improve search e.g. by some kind of sorting data_managers
 *
 * \param[in] id Observation domain ID of wanted Data manager.
 * \return Desired Data Manager configuration structure if exists, NULL if
 * there is no Data manager for specified Observation domain ID
 */
static struct data_manager_config *get_data_mngmt_config(uint32_t id, struct data_manager_config *data_mngmts)
{
	struct data_manager_config *aux_cfg = data_mngmts;

	for (aux_cfg = data_mngmts; aux_cfg; aux_cfg = aux_cfg->next) {
		if (aux_cfg->observation_domain_id == id) {
			break;
		}
	}

	return aux_cfg;
}

/**
 * \brief Insert new Data manager into list
 * 
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
 * 
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
	tm_remove_all_odid_templates(template_mgr, odid);
}

/**
 * \brief Get input queue
 */
inline struct ring_buffer *output_manager_get_in_queue()
{
	return conf->in_queue;
}

/**
 * \brief Set new input queue
 */
void output_manager_set_in_queue(struct ring_buffer *in_queue)
{
	if (conf->in_queue == in_queue) {
		return;
	}
	
	if (conf->running) {
		/* If already running, control message must be sent */
		pthread_mutex_lock(&conf->in_q_mutex);
		
		conf->new_in = in_queue;
		rbuffer_write(conf->in_queue, NULL, 1);
		
		/* Wait for change */
		while (conf->in_queue != in_queue) {
			pthread_cond_wait(&conf->in_q_cond, &conf->in_q_mutex);
		}
		
		pthread_mutex_unlock(&conf->in_q_mutex);
	} else {
		conf->in_queue = in_queue;
	}
}

/**
 * \brief Add new storage plugin
 */
int output_manager_add_plugin(struct storage *plugin)
{
	int i;
	struct data_manager_config *data_mgr = NULL;
	
	/* Find a place for plugin in array */
	for (i = 0; conf->storage_plugins[i]; ++i) {}
	conf->storage_plugins[i] = plugin;
	
	if (plugin->xml_conf->observation_domain_id) {
		/* Plugin for one specific ODID */
		data_mgr = get_data_mngmt_config(atol(plugin->xml_conf->observation_domain_id), conf->data_managers);
		
		if (data_mgr) {
			/* Update existing Data Manager */
			data_manager_add_plugin(data_mgr, plugin);
		}
	} else {
		/* Update all existing Data Managers */
		for (data_mgr = conf->data_managers; data_mgr; data_mgr = data_mgr->next) {
			/* Add plugin to the Data Manager */
			data_manager_add_plugin(data_mgr, plugin);
		}
	}
	
	return 0;
}

/**
 * \brief Remove storage plugin
 */
int output_manager_remove_plugin(int id)
{
	int i;
	struct data_manager_config *data_mgr = NULL;
	struct storage *plugin = NULL;
	
	/* Find plugin with given id */
	for (i = 0; conf->storage_plugins[i]; ++i) {
		if (conf->storage_plugins[i]->id == id) {
			/* Remove it from array */
			plugin = conf->storage_plugins[i];
			conf->storage_plugins[i] = NULL;
			break;
		}
	}
	
	if (!plugin) {
		/* Plugin not found */
		return 0;
	}
	
	/* Kill all its instances */
	if (plugin->xml_conf->observation_domain_id) {
		/* Has ODID - max. 1 instance */
		data_mgr = get_data_mngmt_config(atol(plugin->xml_conf->observation_domain_id), conf->data_managers);
		
		if (data_mgr) {
			/* Kill plugin */
			data_manager_remove_plugin(data_mgr, id);
		}
	} else {
		/* Multiple instances */
		for (data_mgr = conf->data_managers; data_mgr; data_mgr = data_mgr->next) {
			/* Kill plugin */
			data_manager_remove_plugin(data_mgr, id);
		}
	}
	
	return 0;
}

/**
 * \brief Output Manager thread
 *
 * @param[in] config configuration structure
 * @return NULL
 */
static void *output_manager_plugin_thread(void* config)
{
	struct data_manager_config *data_config = NULL;
	struct ipfix_message* msg = NULL;
	unsigned int index;

	conf = (struct output_manager_config *) config;
	index = conf->in_queue->read_offset;

	/* Set thread name to reflect the configuration */
	prctl(PR_SET_NAME, "ipfixcol OM", 0, 0, 0);

	/* loop will break upon receiving NULL from buffer */
	while (1) {
		/* get next data */
		index = -1;
		msg = rbuffer_read(conf->in_queue, &index);

		if (!msg) {
			rbuffer_remove_reference(conf->in_queue, index, 1);
			if (conf->new_in) {
				/* Set new input queue */
				conf->in_queue = (struct ring_buffer *) conf->new_in;
				conf->new_in = NULL;
				pthread_cond_signal(&conf->in_q_cond);
				continue;
			}
			
			/* Stop manager */
			break;
		}

		/* Check whether we have a pointer to the input_info structure based on ODID */
		struct input_info_node *input_info_node = get_input_info_node(msg->input_info->odid);
		if (input_info_node == NULL) {
			/* No reference to input_info structure for this ODID; add it */
			add_input_info(msg->input_info);
		}

		/* Get appropriate data Manager config according to ODID */
		data_config = get_data_mngmt_config(msg->input_info->odid, conf->data_managers);
		if (data_config == NULL) {
			/*
			 * No data manager config for this observation domain ID found -
			 * we have a new observation domain ID, so create new data manager for
			 * it
			 */
			data_config = data_manager_create(msg->input_info->odid, conf->storage_plugins);
			if (data_config == NULL) {
				MSG_WARNING(msg_module, "[%u] Unable to create Data Manager; skipping data...",
						msg->input_info->odid);
				rbuffer_remove_reference(conf->in_queue, index, 1);
				continue;
			}

			/* Add config to data_mngmts structure */
			output_manager_insert(conf, data_config);

			MSG_NOTICE(msg_module, "[%u] Data Manager created", msg->input_info->odid);
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
				MSG_DEBUG(msg_module, "[%u] No source; releasing templates...", data_config->observation_domain_id);
				output_manager_remove(conf, data_config);
			}

			rbuffer_remove_reference(conf->in_queue, index, 1);
			continue;
		}
		
		/* Write data into input queue of Storage Plugins */
		if (rbuffer_write(data_config->store_queue, msg, data_config->plugins_count) != 0) {
			MSG_WARNING(msg_module, "[%u] Unable to write into Data Manager input queue; skipping data...", data_config->observation_domain_id);
			rbuffer_remove_reference(conf->in_queue, index, 1);
			free(msg);
			continue;
		}
		
		/* Remove data from queue (without memory deallocation) */
		rbuffer_remove_reference(conf->in_queue, index, 0);
	}

	MSG_NOTICE(msg_module, "Closing Output Manager thread");

	return (void *) 0;
}

/**
 * \brief Get total cpu time
 * 
 * @return total cpu time
 */
uint64_t statistics_total_cpu()
{
	FILE *stat = fopen("/proc/stat", "r");
	if (!stat) {
		MSG_WARNING(stat_module, "Cannot open file '%s'", "/proc/stat");
		return 0;
	}
	
	uint32_t user = 0, nice = 0, sys = 0, idle = 0;
	if (fscanf(stat, "%*s %" SCNu32 "%" SCNu32 "%" SCNu32 "%" SCNu32, &user, &nice, &sys, &idle) == EOF) {
		MSG_ERROR(stat_module, "Error while reading /proc/stat: %s", strerror(errno));
	}
	
	fclose(stat);
	
	return ((uint64_t) user + (uint64_t) nice + (uint64_t) sys + (uint64_t) idle);
}

/**
 * \brief Search thread statistics
 * 
 * @param conf statiscics conf
 * @param tid thread id
 * @return thread
 */
struct stat_thread *statistics_get_thread(struct stat_conf *conf, unsigned long tid)
{
	struct stat_thread *aux_thread;
	for (aux_thread = conf->threads; aux_thread; aux_thread = aux_thread->next) {
		if (aux_thread->tid == tid) {
			break;
		}
	}
	
	return aux_thread;
}

/**
 * \brief Add thread to list
 * 
 * @param conf statistics conf
 * @param tid thread id
 * @return thread
 */
struct stat_thread *statistics_add_thread(struct stat_conf *conf, long tid)
{
	struct stat_thread *thread = calloc(1, sizeof(struct stat_thread));
	if (!thread) {
		MSG_ERROR(stat_module, "Memory allocation failed (%s:%d)", __FILE__, __LINE__);
		return NULL;
	}
	
	thread->tid = tid;
	thread->next = conf->threads;
	conf->threads = thread;
	
	return thread;
}

/**
 * \brief Print cpu usage for each collector's thread
 * 
 * @param conf statistics conf
 */
static void statistics_print_cpu(struct stat_conf *conf, FILE *stat_out_file)
{
	if (stat_out_file) {
		/* Add contents here */
	} else {
		DIR *dir = opendir(conf->tasks_dir);
		if (!dir) {
			MSG_WARNING(stat_module, "Cannot open directory '%s'", conf->tasks_dir);
			return;
		}
		
		FILE *stat;
		struct dirent *entry;
		char stat_path[MAX_DIR_LEN], thread_name[MAX_DIR_LEN], state;
		int tid = 0;
		unsigned long utime, systime;
		uint64_t proc_time;
		uint64_t total_cpu = statistics_total_cpu();
		float usage;
		
		MSG_INFO(stat_module, "");
		MSG_INFO(stat_module, "%10s %7s %10s %15s", "TID", "state", "cpu usage", "thread name");
		while ((entry = readdir(dir)) != NULL) {
			if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) {
				continue;
			}
			
			/* Parse stat file */
			snprintf(stat_path, MAX_DIR_LEN, "%s/%s/stat", conf->tasks_dir, entry->d_name);
			stat = fopen(stat_path, "r");
			if (!stat) {
				MSG_WARNING(stat_module, "Cannot open file '%s'", stat_path);
				continue;
			}
			
			/* Read thread info */
			uint8_t format_str_len = 62 + sizeof(MAX_DIR_LEN) + 1; // 62 is size of the raw format string, without MAX_DIR_LEN; +1 is null-terminating char
			char format_str[format_str_len];
			snprintf(format_str, format_str_len, "%%d (%%%d[^)]) %%c %%*d %%*d %%*d %%*d %%*d %%*u %%*u %%*u %%*u %%*u %%lu %%lu", MAX_DIR_LEN);
			if (fscanf(stat, format_str, &tid, thread_name, &state, &utime, &systime) == EOF) {
				MSG_ERROR(stat_module, "Error while reading %s: %s", stat_path, strerror(errno));
			}
			fclose(stat);
			
			/* Count thread CPU time */
			proc_time = utime + systime;
			
			struct stat_thread *thread = statistics_get_thread(conf, tid);
			if (!thread) {
				thread = statistics_add_thread(conf, tid);
				if (!thread) {
					continue;
				}
			}
			
			/* Count thread CPU utilization (%) */
			if (thread->proc_time && total_cpu - conf->total_cpu > 0) {
				usage = conf->cpus * (proc_time - thread->proc_time) * 100 / (float) (total_cpu - conf->total_cpu);
			} else {
				usage = 0.0;
			}
			
			/* Print statistics */
			MSG_INFO(stat_module, "%10d %7c %8.2f %% %15s", tid, state, usage, thread_name);
			
			/* Update stats */
			thread->proc_time = proc_time;
		}

		MSG_INFO(stat_module, "");
		closedir(dir);

		/* Update stats */
		conf->total_cpu = total_cpu;
	}
}

/**
 * \brief Print queue usage
 * 
 * @param conf Output Manager config
 * @param stat_out_file Output file for statistics
 */
void statistics_print_buffers(struct output_manager_config *conf, FILE *stat_out_file)
{
	if (stat_out_file) {
		/* Add contents here */
	} else {
		/* Print info about preprocessor's output queue */
		MSG_INFO(stat_module, "Queue utilization:");
		
		struct ring_buffer *prep_buffer = get_preprocessor_output_queue();
		MSG_INFO(stat_module, "     Preprocessor output queue: %u / %u", prep_buffer->count, prep_buffer->size);

		/* Print info about Output Manager queues */
		struct data_manager_config *dm = conf->data_managers;
		if (dm) {
			MSG_INFO(stat_module, "     Output Manager output queues:");
			MSG_INFO(stat_module, "         %.4s | %.10s / %.10s", "ODID", "waiting", "total size");
			
			while (dm) {
				MSG_INFO(stat_module, "   %10u %9u / %u", dm->observation_domain_id, dm->store_queue->count, dm->store_queue->size);
				dm = dm->next;
			}
		}
	}
}

/**
 * \brief Periodically prints statistics about proccessing speed
 * 
 * @param config Output Manager configuration
 * @return 0;
 */
static void *statistics_thread(void* config)
{
	struct output_manager_config *conf = (struct output_manager_config *) config;
	time_t begin = time(NULL), time_now, runtime;
	
	/* Create statistics config */
	conf->stats.total_cpu = 0;
	conf->stats.threads = NULL;
	conf->stats.cpus = sysconf(_SC_NPROCESSORS_ONLN);
	snprintf(conf->stats.tasks_dir, MAX_DIR_LEN, "/proc/%d/task/", getpid());

	/* Create file handle in case statistics file has been specified in config */
	FILE *stat_out_file = NULL;
	xmlNode *node = conf->plugins_config->collector_node;
	while (node != NULL) {
		/* Skip processing this node in case it's a comment */
		if (node->type == XML_COMMENT_NODE) {
			node = node->next;
			continue;
		}

		/* Jump to collectingProcess tree, since 'statisticsFile' belongs to it (if present) */
		if (xmlStrcmp(node->name, (const xmlChar *) "collectingProcess") == 0) {
			node = node->xmlChildrenNode;
		}

		if (xmlStrcmp(node->name, (const xmlChar *) "statisticsFile") == 0) {
			char *stat_out_file_path = (char *) xmlNodeGetContent(node->xmlChildrenNode);
			if (stat_out_file_path && strlen(stat_out_file_path) > 0) {
				/* Determine statistics file path, i.e., full path minus basename */
				char stat_out_path[strlen(stat_out_file_path) + 1];
				strncpy_safe(stat_out_path, stat_out_file_path, strlen(stat_out_file_path) - strlen(basename(stat_out_file_path)));

				/* Check whether statistics file path exists */
				DIR *stat_out_dir;
				if ((stat_out_dir = opendir(stat_out_path)) == NULL) {
					MSG_ERROR(msg_module, "Statistics file directory '%s' does not exist", stat_out_path);
					xmlFree(stat_out_file_path);

					/* Skip to next XML node in configuration */
					node = node->next;
					continue;
				} else {
					closedir(stat_out_dir);
				}

				/* Clean up previous statistics files */
				char glob_path[strlen(stat_out_file_path) + 2]; // +2 is for wildcard (*) and null-terminating char
				strncpy_safe(glob_path, stat_out_file_path, strlen(stat_out_file_path) + 1);
				glob_path[strlen(stat_out_file_path)] = '*';
				glob_path[strlen(stat_out_file_path) + 1] = '\0';

				/* Search for files using glob */
				int glob_ret, glob_flags = 0;
				glob_t glob_results;
				if ((glob_ret = glob(glob_path, glob_flags, NULL, &glob_results)) == 0) {
					uint16_t i, stat_files_deleted = 0;
					for (i = 0; i < glob_results.gl_pathc; ++i) {
						if ((remove(glob_results.gl_pathv[i])) == 0) {
							++stat_files_deleted;
						} else {
							MSG_ERROR(msg_module, "An error occurred while deleting statistics file '%s'", glob_results.gl_pathv[i]);
						}
					}

					MSG_INFO(msg_module, "Cleaned up %u old statistics file(s)", stat_files_deleted);
					globfree(&glob_results);
				} else if (glob_ret == GLOB_NOMATCH) {
					/* No matching files/directories found; do nothing */
				} else {
					/* Another glob error occurred; do nothing */
				}

				/* Consists of the following elements:
				 *      strlen(stat_out_file_path) - length of supplied path
				 *      1 - dot (.)
				 *      5 - we assume that a PID has a maximum length of 5 digits
				 *      1 - null-terminating character
				 */
				int max_len = strlen(stat_out_file_path) + 1 + 5 + 1;
				char full_stat_out_file_path[max_len];

				/* snprintf ensures null-termination if (max_len != 0), which is always true */
				snprintf(full_stat_out_file_path, max_len, "%s.%d", stat_out_file_path, getpid());
				stat_out_file = fopen(full_stat_out_file_path, "w");
			} else {
				MSG_ERROR(msg_module, "Configuration error: 'statisticsFile' node has no value");
			}

			xmlFree(stat_out_file_path);

			/* No need to continue tree traversal, because 'statisticsFile' is only node to look for */
			break;
		}

		node = node->next;
	}
	
	/* Catch SIGUSR1 */
	signal(SIGUSR1, sig_handler);
	
	/* Set thread name */
	prctl(PR_SET_NAME, "ipfixcol:stats", 0, 0, 0);
	
	while (conf->stat_interval) {
		sleep(conf->stat_interval);
		
		/* Killed by Output Manager */
		if (conf->stats.done) {
			break;
		}
		
		/* Compute time */
		time_now = time(NULL);
		runtime = time_now - begin;

		/* Print info */
		if (stat_out_file) {
			rewind(stat_out_file); /* Move to beginning of file */
			fprintf(stat_out_file, "%s=%lu\n", "TIME", time_now);
			fprintf(stat_out_file, "%s=%lu\n", "RUNTIME", runtime);
		} else {
			MSG_INFO(stat_module, "");
			MSG_INFO(stat_module, "Time: %lu, runtime: %lu", time_now, runtime);
			MSG_INFO(stat_module, "%10s %15s %15s %15s %15s %15s %20s", "ODID", "packets", "data rec.", "lost data rec.", "packets/s", "data records/s", "lost data records/s");
		}

		/* Variables for calculating sums */
		uint64_t packets_total = 0;
		uint64_t data_records_total = 0;
		uint64_t lost_data_records_total = 0;

		/* Variables for calculating delta (to previous measurement) */
		uint32_t delta_packets;
		uint32_t delta_data_records;
		uint32_t delta_lost_data_records;

		struct input_info_node *aux_node = input_info_list;
		uint8_t aux_node_count = 0;
		while (aux_node) {
			delta_packets = aux_node->input_info->packets - aux_node->last_packets;
			delta_data_records = aux_node->input_info->data_records - aux_node->last_data_records;
			delta_lost_data_records = aux_node->input_info->sequence_number - aux_node->input_info->data_records - aux_node->last_lost_data_records;

			if (stat_out_file) {
				fprintf(stat_out_file, "%s%u=%lu\n", "PACKETS",
						aux_node->input_info->odid, aux_node->input_info->packets);
				fprintf(stat_out_file, "%s%u=%lu\n", "DATA_REC",
						aux_node->input_info->odid, aux_node->input_info->data_records);
				fprintf(stat_out_file, "%s%u=%lu\n", "LOST_DATA_REC",
						aux_node->input_info->odid, aux_node->input_info->sequence_number - aux_node->input_info->data_records);
				fprintf(stat_out_file, "%s%u=%u\n", "PACKETS_SEC",
						aux_node->input_info->odid, delta_packets / conf->stat_interval);
				fprintf(stat_out_file, "%s%u=%u\n", "DATA_REC_SEC",
						aux_node->input_info->odid, delta_data_records / conf->stat_interval);
				fprintf(stat_out_file, "%s%u=%u\n", "LOST_DATA_REC_SEC",
						aux_node->input_info->odid, delta_lost_data_records / conf->stat_interval);
			} else {
				MSG_INFO(stat_module, "%10u %15lu %15lu %15lu %15u %15u %20u",
						aux_node->input_info->odid,
						aux_node->input_info->packets,
						aux_node->input_info->data_records,
						aux_node->input_info->sequence_number - aux_node->input_info->data_records,
						delta_packets / conf->stat_interval,
						delta_data_records / conf->stat_interval,
						delta_lost_data_records / conf->stat_interval);
			}

			/* Add values per ODID to sum */
			packets_total += aux_node->input_info->packets;
			data_records_total += aux_node->input_info->data_records;
			lost_data_records_total += aux_node->input_info->sequence_number - aux_node->input_info->data_records;

			/* Update 'last' values */
			aux_node->last_packets = aux_node->input_info->packets;
			aux_node->last_data_records = aux_node->input_info->data_records;
			aux_node->last_lost_data_records = aux_node->input_info->sequence_number - aux_node->input_info->data_records;

			aux_node = aux_node->next;
			++aux_node_count;
		}

		if (stat_out_file) {
			fflush(stat_out_file);
		} else if (aux_node_count > 1) {
			/* Print totals row, but only in case there is more than one ODID */
			MSG_INFO(stat_module, "%s", "----------------------------------------------------------");
			MSG_INFO(stat_module, "%10s %15lu %15lu %15lu", "Total:", packets_total, data_records_total, lost_data_records_total);
		}
		
		/* Print CPU usage by threads */
		statistics_print_cpu(&(conf->stats), stat_out_file);
		
		/* Print buffer usage */
		statistics_print_buffers(conf, stat_out_file);
	}

	if (stat_out_file) {
		fclose(stat_out_file);
	}
	
	return NULL;
}

/**
 * \brief Creates new Output Manager
 *
 * @param[in] plugins_config plugins configurator
 * @param[in] stat_interval statistics printing interval
 * @param[out] config configuration structure
 * @return 0 on success, negative value otherwise
 */
int output_manager_create(configurator *plugins_config, int stat_interval, void **config)
{
	conf = calloc(1, sizeof(struct output_manager_config));
	if (!conf) {
		MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)", __FILE__, __LINE__);
		return -1;
	}
	
	conf->stat_interval = stat_interval;
	conf->plugins_config = plugins_config;
	
	*config = conf;
	return 0;
}

/**
 * \brief Start Output Manager thread(s)
 * 
 * @return 0 on success
 */
int output_manager_start()
{
	int retval;

	/* Create Output Manager thread */
	retval = pthread_create(&(conf->thread_id), NULL, &output_manager_plugin_thread, (void *) conf);
	if (retval != 0) {
		MSG_ERROR(msg_module, "Unable to create Output Manager thread");
		free(conf);
		return -1;
	}
	
	conf->running = 1;

	if (conf->stat_interval > 0) {
		retval = pthread_create(&(conf->stat_thread), NULL, &statistics_thread, (void *) conf);
		if (retval != 0) {
			MSG_ERROR(msg_module, "Unable to create statistics thread");
			free(conf);
			return -1;
		}
	}
	
	return 0;
}

/**
 * \brief Close Ouput Manager and all Data Managers
 */
void output_manager_close(void *config)
{
	struct output_manager_config *manager = (struct output_manager_config *) config;
	struct data_manager_config *aux_config = NULL, *tmp = NULL;
	struct stat_thread *aux_thread = NULL, *tmp_thread = NULL;

	/* Stop Output Manager thread and free input buffer */
	if (manager->running) {
		rbuffer_write(manager->in_queue, NULL, 1);
		pthread_join(manager->thread_id, NULL);
		rbuffer_free(manager->in_queue);

		/* Close statistics thread */
		if (manager->stat_interval > 0) {
			manager->stats.done = 1;
			pthread_kill(manager->stat_thread, SIGUSR1);
			pthread_join(manager->stat_thread, NULL);
		}

		aux_config = manager->data_managers;

		/* Close all data managers */
		while (aux_config) {
			tmp = aux_config;
			aux_config = aux_config->next;
			data_manager_close(&tmp);
		}

		/* Free input_info_list */
		if (input_info_list) {
			struct input_info_node *aux_node = input_info_list;
			while (aux_node) {
				struct input_info_node *aux_node_rm = aux_node;
				aux_node = aux_node->next;
				free(aux_node_rm);
			}
		}

		/* Free all thread structures for statistics */
		aux_thread = manager->stats.threads;
		while (aux_thread) {
			tmp_thread = aux_thread;
			aux_thread = aux_thread->next;
			free(tmp_thread);
		}
	}
	
	free(manager);
}
