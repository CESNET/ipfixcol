/**
 * \file configurator.h
 * \author Michal Kozubik <kozubik@cesnet.cz>
 * \brief Configurator implementation.
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

#ifndef CONFIGURATOR_H
#define	CONFIGURATOR_H

#include "config.h"

/**
 * \addtogroup internalConfig
 * \ingroup internalAPIs
 *
 * These functions implements processing of configuration data of the
 * collector (mainly plugins (re)configuration etc.).
 *
 * @{
 */

/* Plugin type */
enum plugin_types {
    PLUGIN_INPUT,
    PLUGIN_INTER,
    PLUGIN_STORAGE
};

#define PLUGIN_ID_ALL 0

/* Plugin configuration */
struct plugin_config {
    union {
        struct input *input;        /**< Input plugin config */
        struct intermediate *inter; /**< Intermediate plugin config */
        struct storage *storage;    /**< Storage plugin config */
    };
    
    int type;                       /**< Plugin type */
    struct plugin_xml_conf conf;    /**< XML configuration */
};

/* parsed startup config */
typedef struct startup_config_s {
    struct plugin_config *input[8];     /**< Input plugins */
    struct plugin_config *inter[8];     /**< Intermediate plugins */
    struct plugin_config *storage[8];   /**< Storage plugins */
} startup_config;

typedef struct ipfix_config {
    const char *internal_file;      /**< path to internal configuration file */
    const char *startup_file;       /**< path to startup configuration file */
    xmlDoc *act_doc;                /**< Actual startup configuration */
    xmlDoc *new_doc;                /**< New startup configuration */
    xmlNode *collector_node;        /**< Collector node in startup.xml */
    struct input input;             /**< Input plugin */
    startup_config *startup;        /**< parser startup file */
    char process_name[16];          /**< process name */
    int proc_id;                    /**< process ID */
    int ip_id;                      /**< Internal process ID */
    int sp_id;                      /**< Storage plugin ID */
} configurator;

/**
 * \brief Initialize configurator
 * 
 * \param[in] internal path to internalcfg.xml
 * \param[in] startup  path to startup.xml
 * \return initialized configurator
 */
configurator *config_init(const char *internal, const char *startup);

/**
 * \brief Reconfigure collector
 * 
 * \param[in] config configurator structure
 * \return 0 on success
 */
int config_reconf(configurator *config);

/**
 * \brief Stop all intermediate plugins and flush their buffers
 * 
 * \param[in] config configurator
 */
void config_stop_inter(configurator *config);

/**
 * \brief Destroy configurator
 * 
 * \param[in] config configurator structure
 */
void config_destroy(configurator *config);

/**@}*/

#endif	/* CONFIGURATOR_H */
