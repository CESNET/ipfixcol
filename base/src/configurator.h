/* 
 * File:   configurator.h
 * Author: michal
 *
 * Created on 2. prosinec 2014, 9:33
 */

#ifndef CONFIGURER_H
#define	CONFIGURER_H

#include "config.h"

/* Plugin type */
enum plugin_types {
    PLUGIN_INPUT,
    PLUGIN_INTER,
    PLUGIN_STORAGE,
    PLUGIN_NONE
};

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
    xmlNode *collector_node;        /**< Collector node in startup.xml */
    struct input input;             /**< Input plugin */
    startup_config *startup;        /**< parser startup file */
    char process_name[16];          /**< process name */
    int proc_id;                    /**< process ID */
} configurator;


/* Public: */
/**
 * \brief Initialize configurator
 * 
 * \param internal path to internalcfg.xml
 * \param startup  path to startup.xml
 * \return initialized configurator
 */
configurator *config_init(const char *internal, const char *startup);

/**
 * \brief Reconfigure collector
 * 
 * \param config configurator structure
 * \return 0 on success
 */
int config_reconf(configurator *config);

/**
 * \brief Destroy configurator
 * 
 * \param config configurator structure
 */
void config_destroy(configurator *config);

#endif	/* CONFIGURER_H */

