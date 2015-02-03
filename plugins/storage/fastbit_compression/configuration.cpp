#include "configuration.h"
#include "ipfixcol_fastbit.h"

bool xml_get_bool(xmlDoc *doc, xmlNode *node)
{
	xmlChar *text;
	bool retval;

	text = xmlNodeListGetString(doc, node->xmlChildrenNode, 1);
	if (!xmlStrcmp(text, (const xmlChar *) "yes")) {
		retval = true;
	} else {
		retval = false;
	}
	xmlFree(text);

	return retval;
}

bool xml_get_uint(xmlDoc *doc, xmlNode *node, unsigned int *result)
{
	xmlChar *text;
	bool retval;

	text = xmlNodeListGetString(doc, node->xmlChildrenNode, 1);
	if (sscanf((const char *) text, "%u", result) < 1) {
		retval = false;
	}

	xmlFree(text);
	retval = true;
	return retval;
}

bool xml_get_text(xmlDoc *doc, xmlNode *node, char **result)
{
	xmlChar *text;
	char *tmp;

	if (result == NULL) {
		return false;
	}

	text = xmlNodeListGetString(doc, node->xmlChildrenNode, 1);
	tmp = strdup((const char *) text);
	xmlFree(text);

	if (tmp == NULL) {
		return false;
	}

	*result = tmp;

	return true;
}

bool load_config(struct fastbit_plugin_conf *conf, const char *params)
{
	xmlDoc *doc;
	xmlNode *cur, *cur2;
	xmlChar *text = NULL;
	
	conf->flags = 0;
	conf->global_compress = NULL;

	/* parse configuration */
	doc = xmlParseDoc((xmlChar *) params);
	if (doc == NULL) {
		MSG_ERROR(MSG_MODULE, "Parsing plugin configuration failed");
		goto err;
	}
	cur = xmlDocGetRootElement(doc);
	if (cur == NULL) {
		MSG_ERROR(MSG_MODULE, "Empty configuration");
		goto err;
	}
	if (xmlStrcmp(cur->name, (const xmlChar *) "fileWriter")) {
		MSG_ERROR(MSG_MODULE, "Invalid configuration");
		goto err;
	}
	cur = cur->xmlChildrenNode;
	while (cur != NULL) {
		if ((!xmlStrcmp(cur->name, (const xmlChar *) "fileFormat"))) {
		} else if ((!xmlStrcmp(cur->name, (const xmlChar *) "path"))) {
			xml_get_text(doc, cur, &conf->db_path);
		} else if ((!xmlStrcmp(cur->name, (const xmlChar *) "dumpInterval"))) {
			cur2 = cur->xmlChildrenNode;
			while (cur2 != NULL) {
				if (!xmlStrcmp(cur2->name, (const xmlChar *) "timeWindow")) {
					if (!xml_get_uint(doc, cur2, &conf->window_size)) {
						MSG_ERROR(MSG_MODULE, "invalid timeWindow value");
					}
				} else if (!xmlStrcmp(cur2->name, (const xmlChar *) "timeAlignment")) {
					if(xml_get_bool(doc, cur2)) {
						conf->flags |= CONF_TIME_ALIGN;
					}
				} else if (!xmlStrcmp(cur2->name, (const xmlChar *) "recordLimit")) {
					if(xml_get_bool(doc, cur2)) {
						conf->flags |= CONF_RECORD_LIMIT;
					}
				} else if (!xmlStrcmp(cur2->name, (const xmlChar *) "bufferSize")) {
					if(!xml_get_uint(doc, cur2, &conf->buffer_size)) {
						MSG_ERROR(MSG_MODULE, "invalid timeWindow value");
					}
				}
				cur2 = cur2->next;
			}
		} else if ((!xmlStrcmp(cur->name, (const xmlChar *) "namingStrategy"))) {
			cur2 = cur->xmlChildrenNode;
			while (cur2 != NULL) {
				if (!xmlStrcmp(cur2->name, (const xmlChar *) "type")) {
					text = xmlNodeListGetString(doc, cur2->xmlChildrenNode, 1);
					if (!xmlStrcmp(text, (const xmlChar *) "time")) {
						conf->naming = CONF_NAMING_TIME;
					} else if (!xmlStrcmp(text, (const xmlChar *) "incremental")) {
						conf->naming = CONF_NAMING_INC;
					} else if (!xmlStrcmp(text, (const xmlChar *) "prefix")) {
						conf->naming = CONF_NAMING_PREFIX;
					} else {
						MSG_ERROR(MSG_MODULE, "invalid namingStrategy type");
					}
					xmlFree(text);
					text = NULL;
				} else if (!xmlStrcmp(cur2->name, (const xmlChar *) "prefix")) {
					if(!xml_get_text(doc, cur2, &conf->prefix)) {
						MSG_ERROR(MSG_MODULE, "couldn't get prefix");
					}
				}
				cur2 = cur2->next;
			}
		} else if ((!xmlStrcmp(cur->name, (const xmlChar *) "onTheFlyIndexes"))) {
			if (xml_get_bool(doc, cur)) {
				conf->flags |= CONF_OTF_INDEXES;
			}
		} else if ((!xmlStrcmp(cur->name, (const xmlChar *) "reorder"))) {
			if (xml_get_bool(doc, cur)) {
				conf->flags |= CONF_REORDER;
			}
		} else if ((!xmlStrcmp(cur->name, (const xmlChar *) "indexes"))) {
			load_column_settings(conf, doc, cur, add_tmpl_indexes, add_element_indexes);
		} else if (!xmlStrcmp(cur->name, (const xmlChar *) "globalCompression")) {
			text = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			if (text) {
				conf->global_compress = add_compression(conf, (const char *) text);
				xmlFree(text);
			}
		} else if (!xmlStrcmp(cur->name, (const xmlChar *) "compress")) {
			load_column_settings(conf, doc, cur, add_tmpl_compression, add_element_compression);
		} else if (!xmlStrcmp(cur->name, (const xmlChar *) "compressOptions")) {
			cur2 = cur->xmlChildrenNode;
			while (cur2 != NULL) {
				column_writer *writer = add_compression(conf, (const char *) cur2->name);
				if (writer) {
					writer->conf_init(doc, cur2);
				}
				cur2 = cur2->next;
			}
		} else {
			MSG_WARNING(MSG_MODULE, "Unknown element %s", cur->name);
		}
		cur = cur->next;
	}

	xmlFreeDoc(doc);

	return true;
err:
	xmlFreeDoc(doc);
	if (text) {
		xmlFree(text);
	}
	return false;
}

void add_tmpl_indexes(struct fastbit_plugin_conf *conf, uint16_t template_id, const char *text)
{
	MSG_WARNING(MSG_MODULE, "enabling indexes per template not allowed");
}

void add_element_indexes(struct fastbit_plugin_conf *conf, uint32_t enterprise, uint16_t element_id, const char *text)
{
	conf->indexes.insert(std::make_pair(enterprise, element_id));
}

column_writer *add_compression(struct fastbit_plugin_conf *conf, const char *text)
{
	std::map<std::string, column_writer *>::iterator iter;
	column_writer *writer;

	iter = (conf->writers).find(std::string(text));
	if (iter == conf->writers.end()) {
		writer = column_writer::create(text, NULL, NULL);
		if (writer) {
			iter = conf->writers.insert(std::make_pair(std::string(text), writer)).first;
		} else {
			return NULL;
		}
	}
	return iter->second;
}

void add_tmpl_compression(struct fastbit_plugin_conf *conf, uint16_t template_id, const char *text)
{
	column_writer *writer;

	if (text == NULL) {
		return;
	}

	writer = add_compression(conf, text);

	if (writer == NULL) {
		return;
	}

	conf->compress_tmpl[template_id] = writer;
}

void add_element_compression(struct fastbit_plugin_conf *conf, uint32_t enterprise, uint16_t element_id, const char *text)
{
	column_writer *writer;

	if (text == NULL) {
		return;
	}

	writer = add_compression(conf, text);

	if (writer == NULL) {
		return;
	}

	conf->compress_element[std::make_pair(enterprise, element_id)] = writer;
}

void load_column_settings(struct fastbit_plugin_conf *conf, xmlDoc*doc, xmlNode *node,
		void (*tmpl_callback)(struct fastbit_plugin_conf *, uint16_t, const char *),
		void (*element_callback)(struct fastbit_plugin_conf *, uint32_t, uint16_t, const char *))
{
	xmlNode *cur;
	xmlChar *text = NULL;
	uint16_t template_id;
	uint32_t enterprise;
	uint16_t element;

	for (cur = node->xmlChildrenNode; cur != NULL; cur = cur->next) {
		if (cur->type != XML_ELEMENT_NODE) {
			continue;
		}
		if (!xmlStrcmp(cur->name, (const xmlChar *) "template")) {
			text = xmlGetProp(cur, (const xmlChar *) "id");
			if (text == NULL) {
				MSG_WARNING(MSG_MODULE, "missing template id");
				continue;
			}
			if (sscanf((const char *) text, "%hu", &template_id) < 1) {
				MSG_WARNING(MSG_MODULE, "invalid template id: %s", text);
				xmlFree(text);
				continue;
			}
			xmlFree(text);
			text = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			tmpl_callback(conf, template_id, (const char *) text);
			if (text) {
				xmlFree(text);
			}
		} else if (!xmlStrcmp(cur->name, (const xmlChar *) "element")) {
			text = xmlGetProp(cur, (const xmlChar *) "enterprise");
			if (text == NULL) {
				enterprise = 0;
			} else if (sscanf((const char *) text, "%u", &enterprise) < 1) {
				MSG_WARNING(MSG_MODULE, "invalid enterprise number: %s", text);
				xmlFree(text);
				continue;
			} else {
				xmlFree(text);
			}
			text = xmlGetProp(cur, (const xmlChar *) "id");
			if (text == NULL) {
				MSG_WARNING(MSG_MODULE, "missing element number");
				continue;
			}
			if (sscanf((const char *) text, "%hu", &element) < 1) {
				MSG_WARNING(MSG_MODULE, "invalid element number: %s", text);
				xmlFree(text);
				continue;
			}
			xmlFree(text);
			text = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			element_callback(conf, enterprise, element, (const char *) text);
			if (text) {
				xmlFree(text);
			}
		} else {
			MSG_WARNING(MSG_MODULE, "unknown element %s", cur->name);
		}
	}
}

column_writer *get_column_writer(const struct fastbit_plugin_conf *conf, uint16_t template_id, uint32_t enterprise, uint16_t element_id)
{
	std::map<std::pair<uint32_t, uint16_t>, column_writer *>::const_iterator it_element;
	std::map<uint16_t, column_writer *>::const_iterator it_tmpl;

	it_element = conf->compress_element.find(std::make_pair(enterprise, element_id));
	if (it_element != conf->compress_element.end()) {
		if (it_element->second != NULL) {
			return it_element->second;
		}
	}

	it_tmpl = conf->compress_tmpl.find(template_id);
	if (it_tmpl != conf->compress_tmpl.end()) {
		if (it_tmpl->second != NULL) {
			return it_tmpl->second;
		}
	}
	return conf->global_compress;
}

bool get_build_index(const struct fastbit_plugin_conf *conf, uint16_t template_id, uint32_t enterprise, uint16_t element_id)
{
	if (conf->flags & CONF_OTF_INDEXES) {
		return true;
	}
	return (conf->indexes.find(std::make_pair(enterprise, element_id)) != conf->indexes.end());
}

void free_config(struct fastbit_plugin_conf *conf)
{
	std::map<std::string, column_writer *>::iterator writers_it;

	for (writers_it = conf->writers.begin(); writers_it != conf->writers.end(); writers_it++) {
		if (writers_it->second) {
			delete writers_it->second;
		}
	}

	if (conf == NULL) {
		return;
	}

	if (conf->db_path) {
		free(conf->db_path);
	}
	if (conf->prefix) {
		free(conf->prefix);
	}
}
