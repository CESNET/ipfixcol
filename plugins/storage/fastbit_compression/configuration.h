/** @file
 */
#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include <set>
#include <map>
#include <string>

extern "C" {
#include <ipfixcol/storage.h>
#include <ipfixcol/verbose.h>
	
#include <libxml/parser.h>
}

#include "types.h"
#include "compression.h"

#define CONF_REORDER            0x01
#define CONF_OTF_INDEXES        0x02
#define CONF_RECORD_LIMIT       0x04
#define CONF_TIME_ALIGN         0x08

typedef enum {
	CONF_NAMING_TIME,
	CONF_NAMING_INC,
	CONF_NAMING_PREFIX
} naming_t;

/**
 * Structure to hold configuration options from xml configuration file
 */
struct fastbit_plugin_conf {
	char *db_path;
	int flags;
	naming_t naming;
	char *prefix;
	unsigned int window_size;
	unsigned int buffer_size;
	std::set<std::pair<uint32_t, uint16_t> > indexes;

	column_writer *global_compress;
	std::map<std::pair<uint32_t, uint16_t>, column_writer *> compress_element;
	std::map<uint16_t, column_writer *> compress_tmpl;

	std::map<std::string, column_writer *> writers;

	type_cache_t type_cache;
};

/**
 * @brief Free all dynamically allocated memory referenced by conf.
 * @param conf Pointer to configuration structure.
 */
void free_config(struct fastbit_plugin_conf *conf);

/**
 * @brief Get compression writer from config.
 * @param conf Pointer to configuration structure.
 * @param template_id Template id
 * @param enterprise Enterprise number
 * @param element_id Element id.
 * @return Column writer type for given element.
 */
column_writer *get_column_writer(const struct fastbit_plugin_conf *conf, uint16_t template_id, uint32_t enterprise, uint16_t element_id);

/**
 * @brief Whether to build index for given element.
 * @param conf Pointer to configuration structure.
 * @param template_id Template id
 * @param enterprise Enterprise number
 * @param element_id Element id.
 * @return True if index should be built for given element according to the configuration.
 */
bool get_build_index(const struct fastbit_plugin_conf *conf, uint16_t template_id, uint32_t enterprise, uint16_t element_id);

bool xml_get_bool(xmlDoc *doc, xmlNode *node);
bool xml_get_uint(xmlDoc *doc, xmlNode *node, unsigned int *result);
bool xml_get_text(xmlDoc *doc, xmlNode *node, char **result);
bool load_config(struct fastbit_plugin_conf *conf, const char *params);


void load_column_settings(struct fastbit_plugin_conf *conf, xmlDoc*doc, xmlNode *node,
		void (*tmpl_callback)(struct fastbit_plugin_conf *, uint16_t, const char *),
		void (*element_callback)(struct fastbit_plugin_conf *, uint32_t, uint16_t, const char *));

column_writer *add_compression(struct fastbit_plugin_conf *conf, const char *text);
void add_tmpl_indexes(struct fastbit_plugin_conf *conf, uint16_t template_id, const char *text);
void add_element_indexes(struct fastbit_plugin_conf *conf, uint32_t enterprise, uint16_t element_id, const char *text);
void add_tmpl_compression(struct fastbit_plugin_conf *conf, uint16_t template_id, const char *text);
void add_element_compression(struct fastbit_plugin_conf *conf, uint32_t enterprise, uint16_t element_id, const char *text);

#endif
