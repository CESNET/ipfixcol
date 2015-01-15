/** @file
 */
#ifndef IPFIXCOL_FASTBIT_H
#define IPFIXCOL_FASTBIT_H

extern "C" {
#include <time.h>
#include <pthread.h>
}

#include <fastbit/ibis.h>
#include <map>

#include "types.h"

#include "configuration.h"
#include "database.h"

#define STATS_FILE_NAME "flowsStats.txt"
#define IPFIX_ELEMENTS_FILE ipfix_elements

extern const char *ipfix_type_table[NTYPES];

/**
 * Structure containing plugin configuration and data that have to be persistent
 * betwen storage API calls.
 */
struct fastbit_plugin {
	struct fastbit_plugin_conf conf;
	time_t start_time;
	/* pointer to current dbslot object */
	std::map<uint32_t, dbslot> db;
};

/**
 * @brief Get time slot number for current time.
 * @param conf Fastbit plugin configuration structure.
 * @return Time slot number.
 */
int get_timeslot(struct fastbit_plugin_conf *conf, time_t start_time, time_t t);

/**
 * @brief Get a path where the data for a given timeslot should be stored.
 * @param conf Fastbit plugin configuration structure.
 * @param timeslot Number of time slot.
 * @param result Address where a pointer to the newly allocated string will be
 * stored. This string should be deallocated with free().
 * @return Success.
 */
bool get_db_path(struct fastbit_plugin_conf *conf, time_t start_time, int timeslot, uint32_t odid, char **result);

/**
 * @brief Return IPFIX type for given enterprise id and element id. Thej
 * ipfix-elements.xml file is searched.
 */
ipfix_type_t get_element_type(uint32_t enterprise_id, uint16_t element_id);

/**
 * @brief Same as @a get_element_type but first search in a cache.
 */
ipfix_type_t get_element_type_cached(uint32_t enterprise_id, uint16_t element_id, type_cache_t *type_cache);

ibis::TYPE_T ipfix_to_fastbit_type(ipfix_type_t ipfix_type, size_t *size);
const char *fastbit_type_str(ibis::TYPE_T);
ibis::TYPE_T fastbit_type_from_str(const char *str);


#define MSG_MODULE "fastbit output"

#endif

