/**
 * \file fastbit.cpp
 * \author Petr Kramolis <kramolis@cesnet.cz>
 * \brief ipficol storage plugin based on fastbit
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

extern "C" {
	#include <ipfixcol/storage.h>
	#include <ipfixcol/verbose.h>

	/* API version constant */
	IPFIXCOL_API_VERSION;
}

#include <pthread.h>
#include <semaphore.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <map>
#include <iostream>
#include <iomanip>
#include <string>
#include <fstream>
#include <iostream>

#include <fastbit/ibis.h>

#include "pugixml.hpp"
#include "fastbit.h"
#include "fastbit_table.h"
#include "fastbit_element.h"
#include "config_struct.h"

void ipv6_addr_non_canonical(char *str, const struct in6_addr *addr)
{
	sprintf(str, "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
			addr->s6_addr[0], addr->s6_addr[1],
			addr->s6_addr[2], addr->s6_addr[3],
			addr->s6_addr[4], addr->s6_addr[5],
			addr->s6_addr[6], addr->s6_addr[7],
			addr->s6_addr[8], addr->s6_addr[9],
			addr->s6_addr[10], addr->s6_addr[11],
			addr->s6_addr[12], addr->s6_addr[13],
			addr->s6_addr[14], addr->s6_addr[15]);
}

/**
 * \brief Load elements types from xml to configure structure
 *
 * This function reads ipfix-elements.xml
 * and stores elements data type in configuration structure
 *
 * @param conf fastbit storage plug-in configuration structure
 */
int load_types_from_xml(struct fastbit_config *conf)
{
	pugi::xml_document doc;
	pugi::xml_parse_result result;
	uint32_t en;
	uint16_t id;
	int len;
	enum store_type type;
	std::string str_value;

	result = doc.load_file(ipfix_elements);

	/* Check for errors */
	if (!result) {
		MSG_ERROR(msg_module, "Error while parsing '%s': %s", ipfix_elements, result.description());
		return -1;
	}

	pugi::xpath_node_set elements = doc.select_nodes("/ipfix-elements/element");
	for (pugi::xpath_node_set::const_iterator it = elements.begin(); it != elements.end(); ++it)
	{
		str_value = it->node().child_value("enterprise");
		en = strtoul(str_value.c_str(), NULL, 0);
		str_value = it->node().child_value("id");
		id = strtoul(str_value.c_str(), NULL, 0);
		str_value = it->node().child_value("dataType");

		/* Obtain field type */
		if (str_value == "boolean" \
				or str_value == "dateTimeSeconds" \
				or str_value == "dateTimeMicroseconds" \
				or str_value == "dateTimeMilliseconds" \
				or str_value == "dateTimeNanoseconds" \
				or str_value == "ipv4Address" \
				or str_value == "macAddress" \
				or str_value == "unsigned8" \
				or str_value == "unsigned16" \
				or str_value == "unsigned32" \
				or str_value == "unsigned64") {
			type = UINT;
		} else if (str_value == "signed8" \
				or str_value == "signed16" \
				or str_value == "signed32" \
				or str_value == "signed64") {
			type = INT;
		} else if (str_value == "ipv6Address") {
			type = IPV6;
		} else if (str_value == "float32" \
			or str_value == "float64") {
			type = FLOAT;
		} else if (str_value == "string") {
			type = TEXT;
		} else if (str_value == "octetArray" \
				or str_value == "basicList" \
				or str_value == "subTemplateList" \
				or str_value== "subTemplateMultiList") {
			type = BLOB;
		} else {
			type = UNKNOWN;
		}

		(*conf->elements_types)[en][id] = type;

		/* Obtain field length */
		if (str_value == "unsigned64" \
				or str_value == "dateTimeMilliseconds" \
				or str_value == "dateTimeMicroseconds" \
				or str_value == "dateTimeNanoseconds" \
				or str_value == "macAddress" \
				or str_value == "float64" \
				or str_value == "signed64") {
			len = 8;
		} else if (str_value == "unsigned32" \
				or str_value == "dateTimeSeconds" \
				or str_value == "ipv4Address" \
				or str_value == "boolean" \
				or str_value == "float32" \
				or str_value == "signed32") {
			len = 4;
		} else if (str_value == "unsigned16" \
				or str_value == "signed16") {
			len = 2;
		} else if (str_value == "unsigned8" \
				or str_value == "signed8") {
			len = 1;
		} else if (str_value == "ipv6Address" \
				or str_value == "string" \
				or str_value == "octetArray" \
				or str_value == "basicList" \
				or str_value == "subTemplateList" \
				or str_value == "subTemplateMultiList") {
			len = -1;
		}

		(*conf->elements_lengths)[en][id] = len;
	}

	return 0;
}

/**
 * \brief Search for element type in configure structure
 *
 * @param conf fastbit storage plug-in configuration structure
 * @param en Enterprise number of element
 * @param id ID of information element
 * @return element type
 */
enum store_type get_type_from_xml(struct fastbit_config *conf, uint32_t en, uint16_t id)
{
	/* Check whether a type has been determined for the specified element */
	if ((*conf->elements_types).count(en) == 0 || (*conf->elements_types)[en].count(id) == 0) {
		if (en == NFv9_CONVERSION_ENTERPRISE_NUMBER) {
			MSG_WARNING(msg_module, "No specification for e%uid%u found in '%s' (enterprise number converted from NFv9)", en, id, ipfix_elements);
		} else {
			MSG_WARNING(msg_module, "No specification for e%uid%u found in '%s'", en, id, ipfix_elements);
		}

		return UNKNOWN;
	}

	return (*conf->elements_types)[en][id];
}

void *reorder_index(void *config)
{
	struct fastbit_config *conf = static_cast<struct fastbit_config*>(config);
	ibis::table *index_table;
	std::string dir;
	ibis::part *reorder_part;
	ibis::table::stringArray ibis_columns;

	sem_wait(&(conf->sem));

	for (unsigned int i = 0; i < conf->dirs->size(); i++) {
		dir = (*conf->dirs)[i];
		/* Reorder partitions */
		if (conf->reorder) {
			MSG_DEBUG(msg_module, "Reordering: %s",dir.c_str());
			reorder_part = new ibis::part(dir.c_str(), NULL, false);
			reorder_part->reorder(); /* TODO return value */
			delete reorder_part;
		}

		/* Build indexes */
		if (conf->indexes == 1) { /* Build all indexes */
			MSG_DEBUG(msg_module, "Creating indexes: %s", dir.c_str());
			index_table = ibis::table::create(dir.c_str());
			index_table->buildIndexes(NULL);
			delete index_table;
		} else if (conf->indexes == 2) { /* Build selected indexes */
			index_table = ibis::table::create(dir.c_str());
			ibis_columns = index_table->columnNames();
			for (unsigned int i = 0; i < conf->index_en_id->size(); i++) {
				for (unsigned int j = 0; j < ibis_columns.size(); j++) {
					if ((*conf->index_en_id)[i] == std::string(ibis_columns[j])) {
						MSG_DEBUG(msg_module, "Creating indexes: %s%s", dir.c_str(), (*conf->index_en_id)[i].c_str());
						index_table->buildIndex(ibis_columns[j]);
					}
				}
			}
			delete index_table;
		}

		ibis::fileManager::instance().flushDir(dir.c_str());
	}

	sem_post(&(conf->sem));
	return NULL;
}

std::string generate_path(struct fastbit_config *config, std::string exporter_ip_addr, uint32_t odid)
{
	struct tm *timeinfo = localtime(&(config->last_flush));
	const int ft_size = 1000;
	char formated_time[ft_size];
	std::string path;

	std::stringstream ss;
	ss << odid;
	std::string odid_str = ss.str();

	strftime(formated_time, ft_size, (config->sys_dir).c_str(), timeinfo);
	path = std::string(formated_time);

	size_t pos = 0;
	while ((pos = path.find("%E", pos)) != std::string::npos) {
		path.replace(pos, 2, exporter_ip_addr);
	}

	pos = 0;
	while ((pos = path.find("%o", pos)) != std::string::npos) {
		path.replace(pos, 2, odid_str);
	}

	path += config->window_dir;
	return path;
}

void update_window_name(struct fastbit_config *conf)
{
	static int flushed = 1;

	/* Change window directory name */
	if (conf->dump_name == PREFIX) {
		conf->window_dir = conf->prefix + "/";
	} else if (conf->dump_name == INCREMENTAL) {
		std::stringstream ss;
		ss << std::setw(12) << std::setfill('0') << flushed;
		conf->window_dir = conf->prefix + ss.str() + "/";
		ss.str("");
		flushed++;
	} else {
		char formated_time[17];
		struct tm *timeinfo = localtime(&(conf->last_flush));
		strftime(formated_time, 17, "%Y%m%d%H%M%S", timeinfo);
		conf->window_dir = conf->prefix + std::string(formated_time) + "/";
	}
}

/**
 * \brief Flushes the data for the specified exporter and ODID
 *
 * @param conf Plugin configuration data structure
 * @param exporter_ip_addr_crc CRC32 of exporter IP address
 * @param odid Observation domain ID
 * @param close Indicates whether plugin/thread should be closed after flushing all data
 */
void flush_data(struct fastbit_config *conf, uint32_t exporter_ip_addr_crc, uint32_t odid,
		std::map<uint16_t,template_table*> *templates, bool close)
{
	std::map<uint16_t, template_table*>::iterator table;
	int s;
	std::stringstream ss;
	pthread_t index_thread;

	std::map<uint32_t, std::map<uint32_t, od_info>*>::iterator exporter_it;
	std::map<uint32_t, od_info>::iterator odid_it;

	/* Check whether exporter is listed in data structure */
	if ((exporter_it = conf->od_infos->find(exporter_ip_addr_crc)) == conf->od_infos->end()) {
		MSG_ERROR(msg_module, "Could not find exporter in od_infos; aborting...");
		return;
	}

	/* Check whether ODID is listed in data structure (under exporter) */
	if ((odid_it = exporter_it->second->find(odid)) == exporter_it->second->end()) {
		MSG_ERROR(msg_module, "Could not find ODID %u in od_infos; aborting...", odid);
		return;
	}

	std::string path = odid_it->second.path;

	sem_wait(&(conf->sem));
	{
		conf->dirs->clear();

		MSG_DEBUG(msg_module, "Flushing data to disk (exporter: %s, ODID: %u)",
				odid_it->second.exporter_ip_addr.c_str(), odid);
		MSG_DEBUG(msg_module, "    > Exported: %u", odid_it->second.flow_watch.received_flows());
		MSG_DEBUG(msg_module, "    > Received: %u", odid_it->second.flow_watch.exported_flows());

		for (table = templates->begin(); table != templates->end(); table++) {
			conf->dirs->push_back(path + ((*table).second)->name() + "/");
			(*table).second->flush(path);
			(*table).second->reset_rows();
		}

		if (odid_it->second.flow_watch.write(path) == -1) {
			MSG_ERROR(msg_module, "Unable to write flow statistics: %s", path.c_str());
		}

		odid_it->second.flow_watch.reset_state();
	}
	sem_post(&(conf->sem));

	if ((s = pthread_create(&index_thread, NULL, reorder_index, conf)) != 0) {
		MSG_ERROR(msg_module, "pthread_create");
	}

	if (close) {
		if ((s = pthread_join(index_thread, NULL)) != 0) {
			MSG_ERROR(msg_module, "pthread_join");
		}
	} else {
		if ((s = pthread_detach(index_thread)) != 0) {
			MSG_ERROR(msg_module, "pthread_detach");
		}
	}
}

/**
 * \brief Flushes the data for *all* exporters and ODIDs
 *
 * @param conf Plugin configuration data structure
 * @param close Indicates whether plugin/thread should be closed after flushing all data
 */
void flush_all_data(struct fastbit_config *conf, bool close)
{
	std::map<uint32_t, std::map<uint32_t, od_info>*> *od_infos = conf->od_infos;
	std::map<uint32_t, std::map<uint32_t, od_info>*>::iterator exporter_it;
	std::map<uint32_t, od_info>::iterator odid_it;

	/* Iterate over all exporters and ODIDs and flush data */
	for (exporter_it = od_infos->begin(); exporter_it != od_infos->end(); ++exporter_it) {
		for (odid_it = exporter_it->second->begin(); odid_it != exporter_it->second->end(); ++odid_it) {
			flush_data(conf, exporter_it->first, odid_it->first, &(odid_it->second.template_info), close);
		}
	}
}

int process_startup_xml(char *params, struct fastbit_config* c)
{
	struct tm *timeinfo;
	char formated_time[17];
	std::string path, time_window, record_limit, name_type, name_prefix,
			indexes, reorder, create_sp_files, test, template_field_lengths, time_alignment;
	pugi::xml_document doc;
	doc.load(params);

	if (doc) {
		/* Load element types from XML */
		if (load_types_from_xml(c) != 0) {
			return 1;
		}

		pugi::xpath_node ie = doc.select_single_node("fileWriter");
		path = ie.node().child_value("path");

		/* Make sure path ends with '/' character */
		if (path.at(path.size() - 1) != '/') {
			c->sys_dir = path + "/";
		} else {
			c->sys_dir = path;
		}

		indexes = ie.node().child_value("onTheFlyIndexes");
		c->indexes = (indexes == "yes");

		create_sp_files = ie.node().child_value("createSpFiles");
		c->create_sp_files = (create_sp_files == "yes");

		reorder = ie.node().child_value("reorder");
		c->reorder = (reorder == "yes");

		template_field_lengths = ie.node().child_value("useTemplateFieldLengths");
		c->use_template_field_lengths =
				(!ie.node().child("useTemplateFieldLengths") || template_field_lengths == "yes");

		pugi::xpath_node_set index_e = doc.select_nodes("fileWriter/indexes/element");
		for (pugi::xpath_node_set::const_iterator it = index_e.begin(); it != index_e.end(); ++it) {
			pugi::xpath_node node = *it;
			std::string en = "0";
			std::string id = "0";
			for (pugi::xml_attribute_iterator ait = node.node().attributes_begin();
					ait != node.node().attributes_end(); ++ait) {
				if (std::string(ait->name()) == "enterprise") {
					en = ait->value();
				} else if (std::string(ait->name()) == "id") {
					id = ait->value();
				}
			}

			/* Make sure IPv6 elements are indexed */
			if (IPV6 == get_type_from_xml(c, strtoul(en.c_str(), NULL, 0), strtoul(id.c_str(), NULL, 0))) {
				c->index_en_id->push_back("e" + en + "id" + id + "p0");
				c->index_en_id->push_back("e" + en + "id" + id + "p1");
			} else {
				c->index_en_id->push_back("e" + en + "id" + id);
			}
		}

		if (c->index_en_id->size() > 0 && c->indexes) {
			c->indexes = 2; /* Mark elements for indexes */
		}

		ie = doc.select_single_node("fileWriter/dumpInterval");
		time_window = ie.node().child_value("timeWindow");
		c->time_window = atoi(time_window.c_str());

		record_limit = ie.node().child_value("recordLimit");
		c->records_window = atoi(record_limit.c_str());

		record_limit = ie.node().child_value("bufferSize");
		c->buff_size = atoi(record_limit.c_str());

		time_alignment = ie.node().child_value("timeAlignment");

		ie = doc.select_single_node("fileWriter/namingStrategy");
		name_prefix = ie.node().child_value("prefix");
		c->prefix = name_prefix;

		time(&(c->last_flush));
		
		name_type = ie.node().child_value("type");
		if (name_type == "time") {
			c->dump_name = TIME;
			if (time_alignment == "yes") {
				if (c->time_window > 0) {
					/* operators '/' and '*' are used for round down time to time window */
					c->last_flush = ((c->last_flush / c->time_window) * c->time_window);
				}
			}

			timeinfo = localtime(&(c->last_flush));
			strftime(formated_time, 17, "%Y%m%d%H%M%S", timeinfo);
			c->window_dir = c->prefix + std::string(formated_time) + "/";
		} else if (name_type == "incremental") {
			c->dump_name = INCREMENTAL;
			c->window_dir = c->prefix + "000000000001/";
		} else if (name_type == "prefix") {
			c->dump_name = PREFIX;
			if (c->prefix == "") {
				c->prefix = "fbitfiles";
			}

			c->window_dir = c->prefix + "/";
		}

		if (sem_init(&(c->sem), 0, 1)) {
			MSG_ERROR(msg_module, "Semaphore initialization error");
			return 1;
		}
	} else {
		return 1;
	}

	return 0;
}

extern "C"
int storage_init(char *params, void **config)
{
	MSG_DEBUG(msg_module, "Fastbit plugin: initialization");
	struct fastbit_config *c = NULL;

	/* Create config structure */
	*config = new struct fastbit_config;
	if (*config == NULL) {
		MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)", __FILE__, __LINE__);
		return 1;
	}

	c = (struct fastbit_config*) *config;
	c->od_infos = new std::map<uint32_t, std::map<uint32_t, struct od_info>*>;
	if (c->od_infos == NULL) {
		MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)", __FILE__, __LINE__);
		return 1;
	}

	c->elements_types = new	std::map<uint32_t, std::map<uint16_t, enum store_type>>;
	if (c->elements_types == NULL) {
		MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)", __FILE__, __LINE__);
		return 1;
	}

	c->elements_lengths = new std::map<uint32_t, std::map<uint16_t, int>>;
	if (c->elements_lengths == NULL) {
		MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)", __FILE__, __LINE__);
		return 1;
	}

	c->index_en_id = new std::vector<std::string>;
	if (c->index_en_id == NULL) {
		MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)", __FILE__, __LINE__);
		return 1;
	}

	c->dirs = new std::vector<std::string>;
	if (c->dirs == NULL) {
		MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)", __FILE__, __LINE__);
		return 1;
	}

	/* Parse configuration xml and updated configure structure according to it */
	if (process_startup_xml(params, c)) {
		MSG_ERROR(msg_module, "Unable to parse plugin configuration");
		return 1;
	}
	
	/* On startup we expect to write to new directory */
	c->new_dir = true;
	return 0;
}

extern "C"
int store_packet(void *config, const struct ipfix_message *ipfix_msg,
		const struct ipfix_template_mgr *template_mgr)
{
	(void) template_mgr;
	std::map<uint16_t, template_table*>::iterator table;
	struct fastbit_config *conf = (struct fastbit_config *) config;
	std::map<uint16_t, template_table*> *templates = NULL;
	std::map<uint16_t, template_table*> *old_templates = NULL; /* Templates to be removed */

	std::map<uint32_t, std::map<uint32_t, od_info>*> *od_infos = conf->od_infos;
	std::map<uint32_t, std::map<uint32_t, od_info>*>::iterator exporter_it;
	std::map<uint32_t, od_info>::iterator odid_it;

	static int rcnt = 0;

	uint16_t template_id;
	uint32_t odid = ntohl(ipfix_msg->pkt_header->observation_domain_id);
	struct input_info_network *input = (struct input_info_network *) ipfix_msg->input_info;

	int rc_flows = 0;
	uint64_t rc_flows_sum = 0;

	char exporter_ip_addr[INET6_ADDRSTRLEN];
	if (input->l3_proto == 6) { /* IPv6 */
		ipv6_addr_non_canonical(exporter_ip_addr, &(input->src_addr.ipv6));
	} else { /* IPv4 */
		inet_ntop(AF_INET, &(input->src_addr.ipv4.s_addr), exporter_ip_addr, INET_ADDRSTRLEN);
	}

	/* Calculate CRC32 over IP address, to have a constant 'key' length for IPv4
	 * and IPv6 addresses */
	uint32_t exporter_ip_addr_crc = 512;

	/* Find exporter in od_infos data structure */
	if ((exporter_it = od_infos->find(exporter_ip_addr_crc)) == od_infos->end()) {
		MSG_INFO(msg_module, "Received data for new exporter: %s", exporter_ip_addr);

		/* Add new exporter to data structure */
		std::map<uint32_t, od_info> *new_exporter = new std::map<uint32_t, od_info>;
		od_infos->insert(std::make_pair(exporter_ip_addr_crc, new_exporter));
		exporter_it = od_infos->find(exporter_ip_addr_crc);
	}

	/* Find ODID in template_info data structure (under exporter) */
	if ((odid_it = exporter_it->second->find(odid)) == exporter_it->second->end()) {
		MSG_INFO(msg_module, "Received new ODID for exporter %s: %u", exporter_ip_addr, odid);

		/* Add new ODID to data structure (under exporter) */
		od_info new_odid;
		new_odid.exporter_ip_addr = exporter_ip_addr;
		new_odid.path = generate_path(conf, exporter_ip_addr, odid);

		exporter_it->second->insert(std::make_pair(odid, new_odid));
		odid_it = exporter_it->second->find(odid);
	}

	templates = &(odid_it->second.template_info);

	/* Process all datasets in message */
	int i;
	for (i = 0 ; i < MSG_MAX_DATA_COUPLES && ipfix_msg->data_couple[i].data_set; i++) {	
		if (ipfix_msg->data_couple[i].data_template == NULL) {
			/* Skip data couples without templates */
			continue;
		}

		template_id = ipfix_msg->data_couple[i].data_template->template_id;

		/* If template (ID) is unknown, add it to the template map */
		if ((table = templates->find(template_id)) == templates->end()) {
			MSG_DEBUG(msg_module, "Received new template: %hu", template_id);
			template_table *table_tmp = new template_table(template_id, conf->buff_size);
			if (table_tmp->parse_template(ipfix_msg->data_couple[i].data_template, conf) != 0) {
				/* Template cannot be parsed, skip data set */
				delete table_tmp;
				continue;
			}
			
			templates->insert(std::pair<uint16_t, template_table*>(template_id, table_tmp));
			table = templates->find(template_id);
		} else {
			/* Check template time. On reception of a new template it is crucial to rewrite the old one. */
			if (ipfix_msg->data_couple[i].data_template->first_transmission > table->second->get_first_transmission()) {
				MSG_DEBUG(msg_module, "Received new template with already used template ID: %hu", template_id);

				/* Init map for old template if necessary */
				if (old_templates == NULL) {
					old_templates = new std::map<uint16_t,template_table*>;
				}

				/* Store old template */
				old_templates->insert(std::pair<uint16_t, template_table*>(table->first, table->second));

				/* Flush data */
				flush_data(conf, exporter_ip_addr_crc, odid, old_templates, false);

				/* Remove rewritten template */
				delete table->second;
				delete old_templates;
				old_templates = NULL;

				/* Remove old template from current list */
				templates->erase(table);

				/* Add the new template */
				template_table *table_tmp = new template_table(template_id, conf->buff_size);
				if (table_tmp->parse_template(ipfix_msg->data_couple[i].data_template, conf) != 0) {
					/* Template cannot be parsed; skip data set */
					delete table_tmp;
					continue;
				}

				templates->insert(std::pair<uint16_t, template_table*>(template_id, table_tmp));
				table = templates->find(template_id);
				/* New template was created; create new directory if necessary */
			}
		}

		/* Check whether data has to be flushed before storing data record */
		bool flush_records = conf->records_window > 0 && rcnt > conf->records_window;
		bool flush_time = false;
		time_t now;
		if (conf->time_window > 0) {
			time(&now);
			flush_time = difftime(now, conf->last_flush) > conf->time_window;
		}

		if (flush_records || flush_time) {
			/* Flush data for all exporters and ODIDs */
			flush_all_data(conf, false);

			/* Time management differs between flush policies (records vs. time) */
			if (flush_records) {
				time(&(conf->last_flush));
			} else if (flush_time) {
				while (difftime(now, conf->last_flush) > conf->time_window) {
					conf->last_flush = conf->last_flush + conf->time_window;
				}
			}

			/* Update window name and path */
			update_window_name(conf);
			odid_it->second.path = generate_path(conf, exporter_ip_addr, (*odid_it).first);

			rcnt = 0;
			conf->new_dir = true;
		}

		/* Store this data record */
		rc_flows = (*table).second->store(ipfix_msg->data_couple[i].data_set, odid_it->second.path, conf->new_dir);
		if (rc_flows >= 0) {
			rc_flows_sum += rc_flows;
			rcnt += rc_flows;
		} else {
			/* No need for showing error message here, since it is already done 
			 * by store() in case of an error */
			// MSG_ERROR(msg_module, "An error occurred during FastBit table store; no records were stored");
		}
	}

	/* We've told all tables that the directory has changed */
	conf->new_dir = false;

	if (rc_flows_sum) {
		odid_it->second.flow_watch.add_flows(rc_flows_sum);
	}

	odid_it->second.flow_watch.update_seq_no(ntohl(ipfix_msg->pkt_header->sequence_number));
	return 0;
}

extern "C"
int store_now(const void *config)
{
	(void) config;
	return 0;
}

extern "C"
int storage_close(void **config)
{
	struct fastbit_config *conf = (struct fastbit_config *) (*config);

	std::map<uint32_t, std::map<uint32_t, od_info>*> *od_infos = conf->od_infos;
	std::map<uint32_t, std::map<uint32_t, od_info>*>::iterator exporter_it;
	std::map<uint32_t, od_info>::iterator odid_it;

	std::map<uint16_t, template_table*> *templates;
	std::map<uint16_t, template_table*>::iterator table;

	/* Iterate over all exporters and ODIDs, flush data and release templates */
	for (exporter_it = od_infos->begin(); exporter_it != od_infos->end(); ++exporter_it) {
		for (odid_it = exporter_it->second->begin(); odid_it != exporter_it->second->end(); ++odid_it) {
			templates = &(odid_it->second.template_info);
			flush_data(conf, exporter_it->first, odid_it->first, templates, true);

			/* Free templates */
			for (table = templates->begin(); table != templates->end(); table++) {
				delete (*table).second;
			}
		}

		delete (*exporter_it).second;
	}

	/* Free config structure */
	delete od_infos;
	delete conf->index_en_id;
	delete conf->dirs;
	delete conf->elements_types;
	delete conf->elements_lengths;
	delete conf;
	return 0;
}
