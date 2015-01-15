extern "C" {
#include <ipfixcol/storage.h>
#include <ipfixcol/verbose.h>
	
#include <libxml/parser.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <limits.h>
#include <errno.h>
}

#include "ipfixcol_fastbit.h"
#include "configuration.h"
#include "database.h"


const char *ipfix_type_table[NTYPES] = {
	NULL,
	"octetArray",
	"unsigned8",
	"unsigned16",
	"unsigned32",
	"unsigned64",
	"signed8",
	"signed16",
	"signed32",
	"signed64",
	"float32",
	"float64",
	"boolean",
	"macAddress",
	"string",
	"dateTimeSeconds",
	"dateTimeMilliseconds",
	"dateTimeMicroseconds",
	"dateTimeNanoseconds",
	"ipv4Address",
	"ipv6Address",
	"basicList",
	"subTemplateList",
	"subTemplateMultiList"
};

ibis::TYPE_T ipfix_to_fastbit_type(ipfix_type_t ipfix_type, size_t *size)
{
	ibis::TYPE_T result = ibis::UNKNOWN_TYPE;

	switch (ipfix_type) {
	case IPFIX_TYPE_boolean:
	case IPFIX_TYPE_unsigned8:
	case IPFIX_TYPE_unsigned16:
	case IPFIX_TYPE_unsigned32:
	case IPFIX_TYPE_dateTimeSeconds:
	case IPFIX_TYPE_ipv4Address:
	case IPFIX_TYPE_unsigned64:
	case IPFIX_TYPE_dateTimeMilliseconds:
	case IPFIX_TYPE_dateTimeMicroseconds:
	case IPFIX_TYPE_dateTimeNanoseconds:
	case IPFIX_TYPE_macAddress:
		if (*size == 1) {
			result = ibis::UBYTE;
		} else if (*size <= 2) {
			result = ibis::USHORT;
			*size = 2;
		} else if (*size <= 4) {
			result = ibis::UINT;
			*size = 4;
		} else if (*size <= 8) {
			*size = 8;
			result = ibis::ULONG;
		}
		break;

	case IPFIX_TYPE_signed8:
	case IPFIX_TYPE_signed16:
	case IPFIX_TYPE_signed32:
	case IPFIX_TYPE_signed64:
		if (*size == 1) {
			result = ibis::BYTE;
		} else if (*size <= 2) {
			result = ibis::SHORT;
		} else if (*size <= 4) {
			result = ibis::INT;
		} else if (*size <= 8) {
			result = ibis::LONG;
		}
		break;

	case IPFIX_TYPE_float32:
		result = ibis::FLOAT;
		*size = 4;
		break;
	case IPFIX_TYPE_float64:
		result = ibis::DOUBLE;
		*size = 8;
		break;

	case IPFIX_TYPE_ipv6Address:
		result = ibis::ULONG;
		*size = 8;
		break;

	case IPFIX_TYPE_octetArray:
		result = ibis::BLOB;
		*size = 0;
		break;
	case IPFIX_TYPE_string:
		result = ibis::TEXT;
		*size = 0;
		break;
	
	case IPFIX_TYPE_basicList:
	case IPFIX_TYPE_subTemplateList:
	case IPFIX_TYPE_subTemplateMultiList:
		result = ibis::BLOB;
		break;
	case IPFIX_TYPE_UNKNOWN:
		result = ibis::UNKNOWN_TYPE;
		break;
	default:
		result = ibis::UNKNOWN_TYPE;
		break;
	}
	return result;
}

const char *fastbit_type_str(ibis::TYPE_T type)
{
	switch (type) {
	case ibis::UNKNOWN_TYPE:
		return "UNKNOWN_TYPE";
	case ibis::OID:
		return "OID";
	case ibis::BYTE:
		return "BYTE";
	case ibis::UBYTE:
		return "UBYTE";
	case ibis::SHORT:
		return "SHORT";
	case ibis::USHORT:
		return "USHORT";
	case ibis::INT:
		return "INT";
	case ibis::UINT:
		return "UINT";
	case ibis::LONG:
		return "LONG";
	case ibis::ULONG:
		return "ULONG";
	case ibis::FLOAT:
		return "FLOAT";
	case ibis::DOUBLE:
		return "DOUBLE";
	case ibis::CATEGORY:
		return "CATEGORY";
	case ibis::TEXT:
		return "TEXT";
	case ibis::BLOB:
		return "BLOB";
	}
	return "UNKNOWN_TYPE";
}

ibis::TYPE_T fastbit_type_from_str(const char *str)
{
	if (str == NULL) {
		return ibis::UNKNOWN_TYPE;
	} else if (!strcmp(str, "OID")) {
		return ibis::OID;
	} else if (!strcmp(str, "BYTE")) {
		return ibis::BYTE;
	} else if (!strcmp(str, "UBYTE")) {
		return ibis::UBYTE;
	} else if (!strcmp(str, "SHORT")) {
		return ibis::SHORT;
	} else if (!strcmp(str, "USHORT")) {
		return ibis::USHORT;
	} else if (!strcmp(str, "INT")) {
		return ibis::INT;
	} else if (!strcmp(str, "UINT")) {
		return ibis::UINT;
	} else if (!strcmp(str, "LONG")) {
		return ibis::LONG;
	} else if (!strcmp(str, "ULONG")) {
		return ibis::ULONG;
	} else if (!strcmp(str, "FLOAT")) {
		return ibis::FLOAT;
	} else if (!strcmp(str, "DOUBLE")) {
		return ibis::DOUBLE;
	} else if (!strcmp(str, "CATEGORY")) {
		return ibis::CATEGORY;
	} else if (!strcmp(str, "TEXT")) {
		return ibis::TEXT;
	} else if (!strcmp(str, "BLOB")) {
		return ibis::BLOB;
	} else {
		return ibis::UNKNOWN_TYPE;
	}
}

int get_timeslot(struct fastbit_plugin_conf *conf, time_t start_time, time_t t)
{
	int timeslot;

	timeslot = difftime(t, start_time) / conf->window_size;

	return timeslot;
}

/**
 * @brief Replace tokens in databese path from config file.
 * @param format String that may contain conversion specifications for strftime
 * with additon of %o which is replaced by @a oid
 * @param result Address where a newly allocated string with result will be stored.
 * @param timeinfo Broken down time.
 * @param oid Observation domain id; replaces '%o' occurrences in format string
 * @return Success
 */
bool path_format(const char *format, char **result, struct tm *timeinfo, uint32_t oid)
{
	char result_tmp[NAME_MAX+1];
	char oid_str[22];

	if (!format) {
		return false;
	}

	if (!result) {
		return false;
	}

	sprintf(oid_str, "%d", oid);

	strftime(result_tmp, sizeof(result_tmp), format, timeinfo);

	*result = (char *) malloc(strlen(result_tmp) + sizeof(oid_str) + 1);
	*result[0] = 0;
	
	size_t i;
	size_t last = 0;
	for (i = 0; result_tmp[i]; i++) {
		if (result_tmp[i] == '%' && result_tmp[i+1] == 'o') {
			result_tmp[i] = 0;
			strcat(*result, &result_tmp[last]);
			strcat(*result, oid_str);
			i++;
			last = i+1;
		}
	}
	strcat(*result, &result_tmp[last]);

	return true;
}

bool get_db_path(struct fastbit_plugin_conf *conf, time_t start_time, int timeslot, uint32_t odid, char **result)
{
	char *dir;
	char name[256];
	time_t slot_time;
	struct tm tm;

	if (result == NULL) {
		return false;
	}

	slot_time = start_time + timeslot * conf->window_size;
	if (conf->flags & CONF_TIME_ALIGN) {
		slot_time -= start_time % conf->window_size;
	}
	localtime_r(&slot_time, &tm);

	if (!path_format(conf->db_path, &dir, &tm, odid)) {
		return false;
	}

	switch (conf->naming) {
	case CONF_NAMING_TIME:
		sprintf(name, "%.4d%.2d%.2d%.2d%.2d%.2d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
		break;
	case CONF_NAMING_INC:
		sprintf(name, "%.12d", timeslot);
		break;
	case CONF_NAMING_PREFIX:
		strcpy(name, "");
		break;
	default:
		MSG_ERROR(MSG_MODULE, "unknown naming strategy type");
		return false;
	}

	*result = (char *) malloc(strlen(dir) + 1 + strlen(name) + strlen(conf->prefix) + 1);
	if (*result == NULL) {
		return false;
	}
	strcpy(*result, dir);
	strcat(*result, "/");
	strcat(*result, conf->prefix);
	strcat(*result, name);
	free(dir);

	return true;
}

ipfix_type_t ipfix_type_from_string(const char *type_name)
{
	for (size_t i = 1; i < NTYPES; i++) {
		if (!strcmp(type_name, ipfix_type_table[i])) {
			return (ipfix_type_t) i;
		}
	}
	MSG_DEBUG(MSG_MODULE, "unknown type %s", type_name);
	return IPFIX_TYPE_UNKNOWN;
}

ipfix_type_t get_element_type(uint32_t enterprise_id, uint16_t element_id)
{
	xmlDoc *doc;
	xmlNode *cur, *cur2;
	xmlChar *text = NULL;
	unsigned int enterprise, id;
	ipfix_type_t type = IPFIX_TYPE_UNKNOWN;

	doc = xmlParseFile(IPFIX_ELEMENTS_FILE);
	if (doc == NULL) {
		MSG_ERROR(MSG_MODULE, "Parsing ipfix elements file failed");
		goto err;
	}
	cur = xmlDocGetRootElement(doc);
	if (cur == NULL) {
		MSG_ERROR(MSG_MODULE, "Empty file");
		goto err;
	}
	if (xmlStrcmp(cur->name, (const xmlChar *) "ipfix-elements")) {
		MSG_ERROR(MSG_MODULE, "Invalid configuration");
		goto err;
	}
	cur = cur->xmlChildrenNode;
	while (cur != NULL) {
		if ((!xmlStrcmp(cur->name, (const xmlChar *) "element"))) {
			cur2 = cur->xmlChildrenNode;
			while (cur2 != NULL) {
				if ((!xmlStrcmp(cur2->name, (const xmlChar *) "enterprise"))) {
					xml_get_uint(doc, cur2, &enterprise);
				} else if ((!xmlStrcmp(cur2->name, (const xmlChar *) "id"))) {
					xml_get_uint(doc, cur2, &id);
				} else if ((!xmlStrcmp(cur2->name, (const xmlChar *) "dataType"))) {
					if (text) {
						xmlFree(text);
					}
					text = xmlNodeListGetString(doc, cur2->xmlChildrenNode, 1);
				} else if ((!xmlStrcmp(cur2->name, (const xmlChar *) "semantic"))) {
				}
				cur2 = cur2->next;
			}
			if (enterprise == enterprise_id && id == element_id) {
				type = ipfix_type_from_string((char *) text);
				break;
			}
		}
		cur = cur->next;
	}

	xmlFreeDoc(doc);

	if (text) {
		xmlFree(text);
	}

	if (type == IPFIX_TYPE_UNKNOWN) {
		MSG_WARNING(MSG_MODULE, "element %d, enterprise %d not found in file '%s'", element_id, enterprise_id, IPFIX_ELEMENTS_FILE);
	}

	return type;
err:
	xmlFreeDoc(doc);

	if (text) {
		xmlFree(text);
	}
	return IPFIX_TYPE_UNKNOWN;
}

ipfix_type_t get_element_type_cached(uint32_t enterprise_id, uint16_t element_id, type_cache_t *type_cache)
{
	ipfix_type_t ipfix_type = IPFIX_TYPE_UNKNOWN;

	if (type_cache) {
		type_cache_t::iterator iter;
		iter = type_cache->find(std::make_pair(enterprise_id, element_id));
		if (iter != type_cache->end()) {
			ipfix_type = iter->second;
		}
	}
	if (ipfix_type == IPFIX_TYPE_UNKNOWN) {
		ipfix_type = get_element_type(enterprise_id, element_id);
		if (type_cache) {
			(*type_cache)[std::make_pair(enterprise_id, element_id)] = ipfix_type;
		}
	}

	return ipfix_type;
}

/* Storage API implementation */

extern "C"
int storage_init(char *params, void **config)
{
	struct fastbit_plugin *core;

	core = new struct fastbit_plugin;

	time(&core->start_time);
	if (!load_config(&core->conf, params)) {
		*config = NULL;
		delete core;
		return 1;
	}

	*config = core;

	MSG_DEBUG(MSG_MODULE, "module started");
	MSG_DEBUG(MSG_MODULE, "database path: %s", core->conf.db_path);
	MSG_DEBUG(MSG_MODULE, "naming strategy: %d", core->conf.naming);
	MSG_DEBUG(MSG_MODULE, "prefix: %s", core->conf.prefix);
	MSG_DEBUG(MSG_MODULE, "flags: %d", core->conf.flags);
	MSG_DEBUG(MSG_MODULE, "start time: %d", core->start_time);

	return 0;
}

extern "C"
int store_packet(void *config, const struct ipfix_message *ipfix_msg,
        const struct ipfix_template_mgr *template_mgr)
{
	struct fastbit_plugin *core = (struct fastbit_plugin *) config;
	time_t t;
	int timeslot;
	std::map<uint32_t, dbslot>::iterator db_iter;
	char *db_dir;
	uint32_t odid = ntohl(ipfix_msg->pkt_header->observation_domain_id);
	uint32_t sequence_number = ntohl(ipfix_msg->pkt_header->sequence_number);

	time(&t);

	timeslot = get_timeslot(&core->conf, core->start_time, t);

	db_iter = core->db.find(odid);
	if (db_iter == core->db.end()) {
		get_db_path(&core->conf, core->start_time, timeslot, odid, &db_dir);
		db_iter = core->db.insert(std::make_pair(odid, dbslot(timeslot, db_dir))).first;
		free(db_dir);
		MSG_DEBUG(MSG_MODULE, "new slot created");
	}

	db_iter->second.exported_flows += (sequence_number - db_iter->second.seq_last);
	db_iter->second.seq_last = sequence_number;
	
	if (timeslot > db_iter->second.get_timeslot()) {
		get_db_path(&core->conf, core->start_time, timeslot, odid, &db_dir);
		db_iter->second.set_timeslot(timeslot);
		db_iter->second.change_dir(db_dir);
		free(db_dir);
	} else if (timeslot < db_iter->second.get_timeslot()) {
		MSG_WARNING(MSG_MODULE, "missed timeslot");
		return 1;
	}

	for (int i = 0; i < 1023; i++) {
		struct ipfix_data_set *data_set = ipfix_msg->data_couple[i].data_set;
		struct ipfix_template *data_template = ipfix_msg->data_couple[i].data_template;

		if (ipfix_msg->data_couple[i].data_set == NULL) {
			break;
		}

		if (ipfix_msg->data_couple[i].data_template == NULL) {
			continue;
		}

		db_iter->second.store_set(data_template, data_set, &core->conf);
	}

	return 0;
}

extern "C"
int store_now(const void *config)
{
	struct fastbit_plugin *core = (struct fastbit_plugin *) config;
	std::map<uint32_t, dbslot>::iterator db_iter;

	for (db_iter = core->db.begin(); db_iter != core->db.end(); db_iter++) {
		db_iter->second.flush();
	}

	return 0;
}

extern "C"
int storage_close(void **config)
{
	struct fastbit_plugin *core = (struct fastbit_plugin *) *config;
	std::map<uint32_t, dbslot>::iterator db_iter;

	if (core == NULL) {
		return 1;
	}

	for (db_iter = core->db.begin(); db_iter != core->db.end(); db_iter++) {
		db_iter->second.flush();
	}

	free_config(&core->conf);
	delete core;

	return 0;
}

