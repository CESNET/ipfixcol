/**
 * \file fastbit.cpp
 * \author Petr Kramolis <kramolis@cesnet.cz>
 * \brief ipficol storage plugin based on fastbit
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


extern "C" {
#include <ipfixcol/storage.h>
#include <ipfixcol/verbose.h>
#include <pthread.h>
#include <semaphore.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
}

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
#include "FlowWatch.h"


void * reorder_index(void * config){
	struct fastbit_config *conf = static_cast<struct fastbit_config*>(config);
	ibis::table *index_table;
	std::string dir;
	ibis::part *reorder_part;
	ibis::table::stringList ibis_columns;
	sem_wait(&(conf->sem));

	for(unsigned int i = 0; i < conf->dirs->size(); i++){
		dir = (*conf->dirs)[i];
		/* reorder partitions */
		if(conf->reorder == 1){
			MSG_DEBUG(MSG_MODULE,"Reordering: %s",dir.c_str());
			reorder_part = new ibis::part(dir.c_str(),NULL, false);
			reorder_part->reorder(); //TODO return value
			delete reorder_part;
		}

		/* build indexes */
		if(conf->indexes == 1){ //build all indexes
			MSG_DEBUG(MSG_MODULE,"Creating indexes: %s",dir.c_str());
			index_table = ibis::table::create(dir.c_str());
			index_table->buildIndexes(NULL);
			delete index_table;
		}
		else if(conf->indexes == 2){ //build selected indexes
			index_table = ibis::table::create(dir.c_str());
			ibis_columns = index_table->columnNames();
			for (unsigned int i=0; i < conf->index_en_id->size(); i++){
				for(unsigned int j=0; j < ibis_columns.size(); j++ ){
					if((*conf->index_en_id)[i] == std::string(ibis_columns[j])){
						MSG_DEBUG(MSG_MODULE,"Creating indexes: %s%s",dir.c_str(),(*conf->index_en_id)[i].c_str());
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


std::string dir_hierarchy(struct fastbit_config *config, uint32_t oid){
	struct tm * timeinfo;

	const int ft_size = 1000;
	char formated_time[ft_size];
	std::string dir;
	size_t o_loc = 0; //observation id location in str

	std::stringstream ss;
	std::string domain_id;

	timeinfo = localtime(&(config->last_flush));

	ss << oid;
	domain_id = ss.str();

	strftime(formated_time,ft_size,(config->sys_dir).c_str(),timeinfo);

	dir = std::string(formated_time);
	while ((o_loc = dir.find ( "%o", o_loc)) != std::string::npos){
		dir.replace (o_loc, 2, domain_id);
	}

	dir+= config->window_dir;
	//std::cout << "Final hierarchy: " << dir << std::endl;
	return dir;
}

void update_window_name(struct fastbit_config *conf){
	std::stringstream ss;
	static int flushed = 1;
	struct tm * timeinfo;
	char formated_time[17];

	// change window directory name!
	if (conf->dump_name == PREFIX){
		conf->window_dir = conf->prefix + "/";
	} else if (conf->dump_name == INCREMENTAL) {
		ss << std::setw(12) << std::setfill('0') << flushed;
		conf->window_dir = conf->prefix + ss.str() + "/";
		ss.str("");
		flushed++;
	} else {
		timeinfo = localtime ( &(conf->last_flush));
		strftime(formated_time,17,"%Y%m%d%H%M%S",timeinfo);
		conf->window_dir = conf->prefix + std::string(formated_time) + "/";
	}
}

void flush_data(struct fastbit_config *conf, uint32_t odid, std::map<uint16_t,template_table*> *templates, bool close) {
	std::string dir;
	std::map<uint16_t,template_table*>::iterator table;
	int s;
	pthread_t index_thread;
	std::stringstream ss;


	MSG_DEBUG(MSG_MODULE,"Flushing data to disk");


	sem_wait(&(conf->sem));
	{ /*  */
		conf->dirs->clear();

		MSG_DEBUG(MSG_MODULE,"ODID [%u]: Exported: %u Collected: %u", odid,
				conf->flowWatch->at(odid).exportedFlows(),
				conf->flowWatch->at(odid).receivedFlows());


		dir = dir_hierarchy(conf, odid);

		for (table = templates->begin(); table != templates->end();table++) {
			conf->dirs->push_back(dir + ((*table).second)->name() + "/");
			(*table).second->flush(dir);
			(*table).second->reset_rows();
		}

		if (conf->flowWatch->at(odid).write(dir) == -1) {
			MSG_ERROR(MSG_MODULE,"Unable to write flows stats: %s", dir.c_str());
		}
		conf->flowWatch->at(odid).reset();
	}
	sem_post(&(conf->sem));

	s = pthread_create(&index_thread, NULL, reorder_index, conf);
	if (s != 0) {
		MSG_ERROR(MSG_MODULE,"pthread_create");
	}
	if (close){
		s = pthread_join(index_thread,NULL);
		if (s != 0) {
			MSG_ERROR(MSG_MODULE,"pthread_join");
		}
	} else {
		s = pthread_detach(index_thread);
		if (s != 0) {
			MSG_ERROR(MSG_MODULE,"pthread_detach");
		}
	}
}

int process_startup_xml(char *params, struct fastbit_config* c){
	struct tm * timeinfo;
	char formated_time[17];
	std::string path,timeWindow,recordLimit,nameType,namePrefix,indexes,test,timeAligment;

	pugi::xml_document doc;
	doc.load(params);

	if(doc){
		/*load element types from xml */
		if (load_types_from_xml(c) != 0) {
			return 1;
		}

		pugi::xpath_node ie = doc.select_single_node("fileWriter");
		path=ie.node().child_value("path");
		//make sure path ends with "/" character
		if(path.at(path.size() -1) != '/'){
			c->sys_dir = path + "/";
		} else {
			c->sys_dir = path;
		}

		c->indexes = 0;
		indexes=ie.node().child_value("onTheFlyIndexes");
		if(indexes == "yes"){
			c->indexes = 1;
		}
		c->reorder = 0;
		indexes=ie.node().child_value("reorder");
		if(indexes == "yes"){
			c->reorder = 1;
		}

		pugi::xpath_node_set index_e = doc.select_nodes("fileWriter/indexes/element");
		for (pugi::xpath_node_set::const_iterator it = index_e.begin(); it != index_e.end(); ++it)
		{
			pugi::xpath_node node = *it;
			std::string en = "0";
			std::string id = "0";
			for (pugi::xml_attribute_iterator ait = node.node().attributes_begin(); ait != node.node().attributes_end(); ++ait)
			{
				if(std::string(ait->name()) == "enterprise"){
					en = ait->value();
				}else if (std::string(ait->name()) == "id"){
					id = ait->value();
				}
			}
			/* make sure IPv6 elements are indexed */
			if(IPv6 == get_type_from_xml(c,strtoul(en.c_str(),NULL,0), strtoul(id.c_str(),NULL,0))){
				c->index_en_id->push_back("e"+en+"id"+id+"p0");
				c->index_en_id->push_back("e"+en+"id"+id+"p1");
			} else {
				c->index_en_id->push_back("e"+en+"id"+id);
			}
		}

		if(c->index_en_id->size() > 0 && c->indexes){
			c->indexes = 2; //mark elements for indexes
		}

		ie = doc.select_single_node("fileWriter/dumpInterval");
		timeWindow=ie.node().child_value("timeWindow");
		c->time_window = atoi(timeWindow.c_str());

		recordLimit=ie.node().child_value("recordLimit");
		c->records_window = atoi(recordLimit.c_str());

		recordLimit=ie.node().child_value("bufferSize");
		c->buff_size = atoi(recordLimit.c_str());

		timeAligment=ie.node().child_value("timeAlignment");

		ie = doc.select_single_node("fileWriter/namingStrategy");
		namePrefix=ie.node().child_value("prefix");
		c->prefix = namePrefix;

		time ( &(c->last_flush));
		
		nameType=ie.node().child_value("type");
		if (nameType == "time") {
			c->dump_name = TIME;
			if (timeAligment == "yes") {
				if (c->time_window > 0) {
					/* operators '/' and '*' are used for round down time to time window */
					c->last_flush = ((c->last_flush/c->time_window) * c->time_window);
				}
			}
			timeinfo = localtime ( &(c->last_flush));
			strftime(formated_time,17,"%Y%m%d%H%M%S",timeinfo);
			c->window_dir = c->prefix + std::string(formated_time) + "/";
		} else if (nameType == "incremental") {
			c->dump_name = INCREMENTAL;
			c->window_dir = c->prefix + "000000000001/";
		} else if (nameType == "prefix") {
			c->dump_name = PREFIX;
			if (c->prefix == "") {
				c->prefix == "fbitfiles";
			}
			c->window_dir = c->prefix + "/";
		}
		if (sem_init(&(c->sem),0,1)) {
			MSG_ERROR(MSG_MODULE,"Error semaphore init");
			return 1;
		}
	} else {
		return 1;
	}
	return 0;
}

/* plugin inicialization */
extern "C"
int storage_init (char *params, void **config){
	MSG_DEBUG(MSG_MODULE, "Fastbit plugin: initialization");
	struct fastbit_config* c = NULL;

	/* create config structure! */
	(*config) = new  struct fastbit_config;
	if(*config == NULL){
		MSG_ERROR(MSG_MODULE,"Can't allocate memory for config structure");
		return 1;
	}
	c = (struct fastbit_config*)(*config);
	c->ob_dom = new std::map<uint32_t,std::map<uint16_t,template_table*>* >;
	if(c->ob_dom == NULL){
		MSG_ERROR(MSG_MODULE, "Can't allocate memory for config structure");
		return 1;
	}

	c->flowWatch = new std::map<uint32_t,FlowWatch>;
		if(c->flowWatch == NULL){
			MSG_ERROR(MSG_MODULE, "Can't allocate memory for config structure");
			return 1;
	}

	c->elements_types = new	std::map<uint32_t,std::map<uint16_t,enum store_type> >;
	if(c->elements_types == NULL){
		MSG_ERROR(MSG_MODULE, "Can't allocate memory for config structure");
		return 1;
	}
	c->index_en_id = new std::vector<std::string>;
	if(c->index_en_id == NULL){
		MSG_ERROR(MSG_MODULE, "Can't allocate memory for config structure");
		return 1;
	}
	c->dirs = new std::vector<std::string>;
	if(c->dirs == NULL){
		MSG_ERROR(MSG_MODULE, "Can't allocate memory for config structure");
		return 1;
	}

	/* parse configuration xml and updated configure structure according to it */
	if(process_startup_xml(params, c)){
		MSG_ERROR(MSG_MODULE, "Unable to parse configuration xml!");
		return 1;
	}
	
	/* On startup we expect to write to new directory */
	c->new_dir = true;
	return 0;
}


extern "C"
int store_packet (void *config, const struct ipfix_message *ipfix_msg,
	const struct ipfix_template_mgr *template_mgr){
	std::map<uint16_t,template_table*>::iterator table;
	struct fastbit_config *conf = (struct fastbit_config *) config;
	std::map<uint16_t,template_table*> *templates = NULL;
	std::map<uint16_t,template_table*> *old_templates = NULL; /* Templates to be removed */
	std::map<uint32_t,std::map<uint16_t,template_table*>* > *ob_dom = conf->ob_dom;
	std::map<uint32_t,std::map<uint16_t,template_table*>* >::iterator dom_id;
	static int rcnt = 0;
	uint rcFlows = 0;
	uint rcFlowsSum = 0;
	std::string domain_name;
	time_t rawtime;
	uint32_t oid = 0;
	std::string dir;
	int i;
	uint16_t template_id;

	oid = ntohl(ipfix_msg->pkt_header->observation_domain_id);
	if((dom_id = ob_dom->find(oid)) == ob_dom->end()){
		MSG_DEBUG(MSG_MODULE,"Received new domain id: %u", oid);
		std::map<uint16_t,template_table*> *new_dom_id = new std::map<uint16_t,template_table*>;
		ob_dom->insert(std::make_pair(oid, new_dom_id));
		dom_id = ob_dom->find(oid);

		(*conf->flowWatch)[oid] = FlowWatch();
	}

	templates = (*dom_id).second;
	dir = dir_hierarchy(conf,(*dom_id).first);

	/* message from ipfixcol have maximum of MSG_MAX_DATA_COUPLES data records */
	for (i = 0 ; i < MSG_MAX_DATA_COUPLES; i++) {
		if (ipfix_msg->data_couple[i].data_set == NULL) {
			//there are no more filled data_sets	
			break;
		}
	
		if (ipfix_msg->data_couple[i].data_template == NULL) {
			//skip data without template!
			continue;
		}

		template_id = ipfix_msg->data_couple[i].data_template->template_id;

		/* if there is unknown template parse it and add it to template map */
		if ((table = templates->find(template_id)) == templates->end()) {
			MSG_DEBUG(MSG_MODULE,"Received new template: %hu", template_id);
			template_table *table_tmp = new template_table(template_id, conf->buff_size);
			if (table_tmp->parse_template(ipfix_msg->data_couple[i].data_template, conf) != 0) {
				/* Template cannot be parsed, skip data set */
				delete table_tmp;
				continue;
			}
			
			templates->insert(std::pair<uint16_t,template_table*>(template_id,table_tmp));
			table = templates->find(template_id);
		} else {
			/* Check template time. On reception of a new template it is crucial to rewrite the old one. */
			if (ipfix_msg->data_couple[i].data_template->last_transmission > table->second->get_last_transmission()) {
				MSG_DEBUG(MSG_MODULE,"Received new template with already used Template ID: %hu", template_id);
				//std::cout << "Template " << template_id << " time: " << ctime(&ipfix_msg->data_couple[i].data_template->last_transmission) << std::endl;

				/* Init map for old template if necessary */
				if (old_templates == NULL) old_templates = new std::map<uint16_t,template_table*>;

				/* Store old template */
				old_templates->insert(std::pair<uint16_t,template_table*>(table->first,table->second));

				/* Flush data */
				flush_data(conf, oid, old_templates, false);

				/* Remove rewritten template */
				delete table->second;
				delete old_templates;
				old_templates = NULL;

				/* Remove old template from current list */
				templates->erase(table);

				/* Add the new template */
				template_table *table_tmp = new template_table(template_id, conf->buff_size);
				if (table_tmp->parse_template(ipfix_msg->data_couple[i].data_template, conf) != 0) {
					/* Template cannot be parsed, skip data set */
					delete table_tmp;
					continue;
				}
				templates->insert(std::pair<uint16_t,template_table*>(template_id,table_tmp));
				table = templates->find(template_id);
				/* New template was created, it creates new directory if necessary */
			}
		}

		
		//should we create new window? 
		if (conf->records_window != 0 && rcnt > conf->records_window) {
			/* Flush data for all ODID */
			for(dom_id = ob_dom->begin(); dom_id!=ob_dom->end();dom_id++){
				/* flush data */
				flush_data(conf, (*dom_id).first, (*dom_id).second, false);
			}

			time ( &(conf->last_flush));
			update_window_name(conf);
			dir = dir_hierarchy(conf,(*dom_id).first);
			rcnt = 0;
			conf->new_dir = true;
		}

		if (conf->time_window != 0) {
			time ( &rawtime );
			if(difftime(rawtime,conf->last_flush) > conf->time_window){
				/* Flush data for all ODID */
				for(dom_id = ob_dom->begin(); dom_id!=ob_dom->end();dom_id++){
					/* flush data */
					flush_data(conf, (*dom_id).first, (*dom_id).second, false);
				}

				conf->last_flush = conf->last_flush + conf->time_window;
				while(difftime(rawtime,conf->last_flush) > conf->time_window){
					conf->last_flush = conf->last_flush + conf->time_window;
				}
				update_window_name(conf);
				dir = dir_hierarchy(conf,(*dom_id).first);
				rcnt = 0;
				conf->new_dir = true;
			}
		}

		/* store this data record */
		rcFlows = (*table).second->store(ipfix_msg->data_couple[i].data_set, dir, conf->new_dir);
		rcFlowsSum += rcFlows;
		rcnt += rcFlows;

	}

	/* We've told all tables that the directory has changed */
	conf->new_dir = false;

	if(rcFlowsSum){
		conf->flowWatch->at(oid).addFlows(rcFlowsSum);
	}
	conf->flowWatch->at(oid).updateSQ(ntohl(ipfix_msg->pkt_header->sequence_number));
	return 0;
}

extern "C"
int store_now (const void *config){
	MSG_DEBUG(MSG_MODULE,"STORE_NOW");
	return 0;
}

extern "C"
int storage_close (void **config){
	MSG_DEBUG(MSG_MODULE,"CLOSE");
	struct fastbit_config *conf = (struct fastbit_config *) (*config);
	std::map<uint16_t,template_table*>::iterator table;
	std::map<uint16_t,template_table*> *templates;
	std::map<uint32_t,std::map<uint16_t,template_table*>* > *ob_dom = conf->ob_dom;
	std::map<uint32_t,std::map<uint16_t,template_table*>* >::iterator dom_id;

	/* flush data, remove templates */
	for(dom_id = ob_dom->begin(); dom_id!=ob_dom->end();dom_id++){
		/* flush data */
		templates = (*dom_id).second;
		flush_data(conf, (*dom_id).first, templates, true);

		/* free templates */
		for(table = templates->begin(); table!=templates->end();table++){
			delete (*table).second;
		}
		delete (*dom_id).second;
	}

	/* free config structure */
	delete ob_dom;
	delete conf->index_en_id;
	delete conf->dirs;
	delete conf->flowWatch;
	delete conf->elements_types;
	delete conf;
	return 0;
}

