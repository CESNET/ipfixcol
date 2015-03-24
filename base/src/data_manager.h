/**
 * \file data_manager.h
 * \author Radek Krejci <rkrejci@cesnet.cz>
 * \author Michal Kozubik <kozubik@cesnet.cz>
 * \brief Data Manager's functions
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

#ifndef DATA_MANAGER_H_
#define DATA_MANAGER_H_

#include <stdint.h>
#include <pthread.h>

#include "ipfixcol.h"
#include "config.h"
#include "queues.h"
#include "preprocessor.h"

/**
 * \brief Data manager configuration
 *
 * Contains all configuration of data manager. Works as list of configurations.
 * List of data manager configurations is mantained by preprocessor which
 * decides what data manager should get the message.
 */
struct data_manager_config {
	uint32_t observation_domain_id;     /**< DM accepts messages from this ODID */
	uint32_t references;                /**< Number of data sources working with this DM */
	unsigned int plugins_count;         /**< Number of running storage plugins */
	struct ring_buffer *store_queue;    /**< Input queue for storage plugins */
	struct storage *storage_plugins[8]; /**< Storage plugins */
	struct data_manager_config *next;   /**< Next DM */
	int oid_specific_plugins;           /**< Number of ODID specific plugins */
};

/**
 * \brief Creates new data manager
 *
 * @param[in] observation_domain_id Observation domain id that should be handled by this data manager
 * @param[in] storage_plugins List of storage plugins that should be opened by this data manager
 * @return data_manager_config Configuration strucutre on success, NULL otherwise
 */
struct data_manager_config* data_manager_create(uint32_t observation_domain_id, struct storage* storage_plugins[]);

/**
 * \brief Closes data manager specified by its configuration
 *
 * @param[in] config Configuration of the data manager to be closed
 * @return void
 */
void data_manager_close (struct data_manager_config **config);

/**
 * \brief Add new storage plugin
 * 
 * @param config Data Manager's config
 * @param plugin Plugin's configuration
 * @return 0 on success
 */
int data_manager_add_plugin(struct data_manager_config *config, struct storage *plugin);

/**
 * \brief Remove storage plugin
 * 
 * @param config Data Manager's config
 * @param id Plugin's id
 * @return 0 on success
 */
int data_manager_remove_plugin(struct data_manager_config *config, int id);

#endif /* DATA_MANAGER_H_ */
