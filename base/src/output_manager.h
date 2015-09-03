/**
 * \file output_manager.h
 * \author Michal Kozubik <kozubik.michal@gmail.com>
 * \brief Output Manger's functions
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

#ifndef OUTPUT_MANAGER_H_
#define OUTPUT_MANAGER_H_

#include <stdint.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>

#include "config.h"
#include "queues.h"
#include "preprocessor.h"
#include "data_manager.h"
#include "configurator.h"

#define MAX_DIR_LEN 256

/* Thread structure (for statistics) */
struct stat_thread {
	unsigned long tid;
	uint64_t proc_time;
	struct stat_thread *next;
};

/* Statistics configuration */
struct stat_conf {
	char tasks_dir[MAX_DIR_LEN];
	long total_cpu;
	int cpus;
	struct stat_thread *threads;
	int done;
};

/**
 * \struct output_mm_config
 *
 * Contains all configuration of output managers' manager.
 */
struct output_manager_config {
	struct data_manager_config *data_managers;  /**< output managers */
	struct data_manager_config *last;           /**< last Output Manager in list */
	struct storage *storage_plugins[32];        /**< Storage plugins */
	struct ring_buffer *in_queue;               /**< Input queue */
	struct ring_buffer *new_in;                 /**< New input queue */
	int running;                                /**< Status of manager */
	bool odid_merge;                            /**< Enable common data manager */
	pthread_t thread_id;                        /**< Manager's thread ID */
	pthread_t stat_thread;                      /**< Stat's thread ID */
	int stat_interval;                          /**< Stat's interval */
	struct stat_conf stats;                     /**< Statistics */
	configurator *plugins_config;               /**< Plugins configurator */
	pthread_mutex_t in_q_mutex;
	pthread_cond_t  in_q_cond;
};

/**
 * \brief Create Output Manager
 *
 * @param[in] plugins_config plugins configurator
 * @param[in] stat_interval statistics printing interval
 * @param[out] config configuration structure
 * @return 0 on success, negative value otherwise
 */
int output_manager_create(configurator *plugins_config, int stat_interval, bool odid_merge, void **config);

/**
 * \brief Start data processing
 * 
 * @return 0 on success
 */
int output_manager_start();

/**
 * \brief Add new plugin to active storage plugins
 * 
 * @param plugin new plugin
 * @return 0 on success
 */
int output_manager_add_plugin(struct storage *plugin);

/**
 * \brief Remove plugin from active storage plugins
 * 
 * @param id Plugin id
 * @return 0 on success
 */
int output_manager_remove_plugin(int id);

/**
 * \brief Closes output manager specified by its configuration
 *
 * @param[in] config Configuration of the output manager to be closed
 */
void output_manager_close(void *config);

/**
 * \brief Set new input queue
 * 
 * @param in_queue input queue
 */
void output_manager_set_in_queue(struct ring_buffer *in_queue);

/**
 * \brief Get Output Manager's input queue
 * 
 * \return input queue
 */
struct ring_buffer *output_manager_get_in_queue();

#endif /* OUTPUT_MANAGER_H_ */
