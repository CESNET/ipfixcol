/**
 * \file fastbit.c
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
}

#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>


#include "fastbit_table.h"
#include "fastbit_element.h"

#include <fastbit/ibis.h>

#include <map>
#include <iostream>
#include <iomanip>
#include <string>
#include "pugixml.hpp"

/** Identifier to MSG_* macros */
static const char *msg_module = "fastbit storage";

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
			std::cout << "Reordering: "<< dir << std::endl;
			reorder_part = new ibis::part(dir.c_str(),NULL, false);
			reorder_part->reorder(); //TODO return value
			delete reorder_part;
		}

		/* build indexes */
		if(conf->indexes == 1){ //build all indexes
			std::cout << "Creating indexes: "<< dir << std::endl;
			index_table = ibis::table::create(dir.c_str());
			index_table->buildIndexes();
			delete index_table;
		}
		else if(conf->indexes == 2){ //build selected indexes
			index_table = ibis::table::create(dir.c_str());
			ibis_columns = index_table->columnNames();
			for (unsigned int i=0; i < conf->index_en_id->size(); i++){
				for(unsigned int j=0; j < ibis_columns.size(); j++ ){
					if((*conf->index_en_id)[i] == std::string(ibis_columns[j])){
						std::cout << "Creating indexes: "<< dir << (*conf->index_en_id)[i] <<std::endl;
						index_table->buildIndex(ibis_columns[j]);
					}
				}
			}
			delete index_table;
		}
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

/* plugin inicialization */
extern "C"
int storage_init (char *params, void **config){
	MSG_NOTICE(msg_module, "Fastbit plugin: initialization");

	struct tm * timeinfo;
	char formated_time[15];
	//ibis::fileManager::adjustCacheSize(1000000000000);

	/* create config structure! */
	(*config) = new  struct fastbit_config;
	if(*config == NULL){
		std::cerr << "Can't allocate memory for config structure" << std::endl;
		return 1;
	}

	/* initialize template map */
	//((struct fastbit_config*)(*config))->templates = new std::map<uint16_t,template_table*>;
	//if(((struct fastbit_config*)(*config))->templates == NULL){
	//	std::cerr << "Can't allocate memory for config structure" << std::endl;
	//	return 1;
	//}

	struct fastbit_config* c = (struct fastbit_config*)(*config);
	
	c->ob_dom = new std::map<uint32_t,std::map<uint16_t,template_table*>* >;
	if(c->ob_dom == NULL){
		std::cerr << "Can't allocate memory for config structure" << std::endl;
		return 1;
	}

	c->index_en_id = new std::vector<std::string>;
	if(c->index_en_id == NULL){
		std::cerr << "Can't allocate memory for config structure" << std::endl;
		return 1;
	}
	c->dirs = new std::vector<std::string>;
	if(c->dirs == NULL){
		std::cerr << "Can't allocate memory for config structure" << std::endl;
		return 1;
	}

	/* parse configuration xml and updated configure structure according to it */
	pugi::xml_document doc;
	doc.load(params);
	std::string path,timeWindow,recordLimit,nameType,namePrefix,indexes,test,timeAligment;

	if(doc){
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

			if(IPv6 == get_type_from_xml(strtoul(en.c_str(),NULL,0), strtoul(id.c_str(),NULL,0))){
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

		nameType=ie.node().child_value("type");
		if(nameType == "time"){
			c->dump_name = TIME;
			time ( &(c->last_flush));
			if(timeAligment == "yes"){
				/* operators '/' and '*' are used for round down time to time window */
				c->last_flush = ((c->last_flush/c->time_window) * c->time_window);
			}
			timeinfo = localtime ( &(c->last_flush));
			strftime(formated_time,15,"%Y%m%d%H%M",timeinfo);
			c->window_dir = c->prefix + std::string(formated_time) + "/";
		} else if (nameType == "incremental") {
			c->dump_name = INCREMENTAL;
			c->window_dir = c->prefix + "000000000001/";
		}
		if ( sem_init(&(c->sem),0,1) ){
		  std::cerr << "Error semaphore init" << std::endl;
		  return 1;
		}
	} else {
		MSG_ERROR(msg_module, "Fastbit plugin: ERROR Unable to parse configuration xml!");
	}

	return 0;
}


extern "C"
int store_packet (void *config, const struct ipfix_message *ipfix_msg,
	const struct ipfix_template_mgr *template_mgr){
	std::map<uint16_t,template_table*>::iterator table;
	struct fastbit_config *conf = (struct fastbit_config *) config;
	std::map<uint16_t,template_table*> *templates = conf->templates;
	std::map<uint32_t,std::map<uint16_t,template_table*>* > *ob_dom = conf->ob_dom;
	std::map<uint32_t,std::map<uint16_t,template_table*>* >::iterator dom_id;
	static int rcnt = 0;
	static int flushed = 1;
	std::stringstream ss;
	std::string domain_name;
	time_t rawtime;
	struct tm * timeinfo;
	char formated_time[15];
	//ibis::table *index_table;
	uint32_t oid = 0;
	std::string dir;
	pthread_t index_thread;
	int s;
	int i;
	int flush=0;

	oid = ntohl(ipfix_msg->pkt_header->observation_domain_id);
	if((dom_id = ob_dom->find(oid)) == ob_dom->end()){
		std::cout << "NEW DOMAIN ID: " << oid << std::endl;
		std::map<uint16_t,template_table*> *new_dom_id = new std::map<uint16_t,template_table*>;
		ob_dom->insert(std::make_pair(oid, new_dom_id));
		dom_id = ob_dom->find(oid);
	}

	templates = (*dom_id).second;
	dir = dir_hierarchy(conf,(*dom_id).first);
	
	/* message from ipfixcol have maximum of 1023 data records */
	for(i = 0 ; i < 1023; i++){ //TODO magic number! add constant to storage.h 
		if(ipfix_msg->data_couple[i].data_set == NULL){
			//there are no more filled data_sets	
			return 0;
		}

	
		if(ipfix_msg->data_couple[i].data_template == NULL){
			//skip data without template!
			continue;
		}

		uint16_t template_id = ipfix_msg->data_couple[i].data_template->template_id;


		/* if there is unknown template parse it and add it to template map */
		if((table = templates->find(template_id)) == templates->end()){
			std::cout << "NEW TEMPLATE: " << template_id << std::endl;
			template_table *table_tmp = new template_table(template_id, conf->buff_size);
			table_tmp->parse_template(ipfix_msg->data_couple[i].data_template, conf);
			templates->insert(std::pair<uint16_t,template_table*>(template_id,table_tmp));
			table = templates->find(template_id);
		}

		/* store this data record */	
		rcnt += (*table).second->store(ipfix_msg->data_couple[i].data_set, dir);

		
		//should we create new window? 
		//-----------TODO rewrite and create function for it?--------------
		if(rcnt > conf->records_window && conf->records_window !=0){
			flush = 1;
			time ( &(conf->last_flush));
			//dir = dir_hierarchy(conf,(*dom_id).first);
		}
		time_t last_flush;
		last_flush = 0;
		if(conf->time_window !=0){
			time ( &rawtime );
			if(difftime(rawtime,conf->last_flush) > conf->time_window){
				flush=1;
				//dir = dir_hierarchy(conf,(*dom_id).first);
				last_flush = conf->last_flush + conf->time_window;
			}
		}

		if(flush){
			flushed ++;
			template_table* el_table;
			std::cout << "FLUSH" << std::endl;
			sem_wait(&(conf->sem));
			/* flush all templates! */
			conf->dirs->clear();
			for(dom_id = ob_dom->begin(); dom_id!=ob_dom->end();dom_id++){
				templates = (*dom_id).second;
				dir = dir_hierarchy(conf,(*dom_id).first);
				for(table = templates->begin(); table!=templates->end();table++){
					conf->dirs->push_back(dir + ((*table).second)->name() + "/");
					(*table).second->flush(dir);
					(*table).second->reset_rows();
					el_table = (*table).second;
				}
			}
			if(last_flush){
				conf->last_flush = last_flush;
			}


			sem_post(&(conf->sem));
			s = pthread_create(&index_thread, NULL, reorder_index, conf);
			if (s != 0){
				std::cerr << " Error: pthread_create" << std::endl;
			}
			s = pthread_detach(index_thread);
			if (s != 0){
				std::cerr << " Error: pthread_detach" << std::endl;
			}


			/* change window directory name! */
			if (conf->dump_name == INCREMENTAL){
				ss << std::setw(12) << std::setfill('0') << flushed;
				conf->window_dir = conf->prefix + ss.str() + "/";
				ss.str("");
			}else{
				timeinfo = localtime ( &(conf->last_flush));

				strftime(formated_time,15,"%Y%m%d%H%M",timeinfo);
				conf->window_dir = conf->prefix + std::string(formated_time) + "/";
			}
			rcnt = 0;
			flush=0;
		}
		//-------------------------------------------------------------------
	}
	return 0;
}

extern "C"
int store_now (const void *config){
	//TODO 
	std::cout <<"STORE_NOW" << std::endl;
	return 0;
}

extern "C"
int storage_close (void **config){
	std::cerr <<"CLOSE" << std::endl;
	std::map<uint16_t,template_table*>::iterator table;
	//template_table* el_table;
	struct fastbit_config *conf = (struct fastbit_config *) (*config);
	std::map<uint16_t,template_table*> *templates;
	//ibis::table *index_table;
	//ibis::part *reorder_part;
	std::map<uint32_t,std::map<uint16_t,template_table*>* > *ob_dom = conf->ob_dom;
	std::map<uint32_t,std::map<uint16_t,template_table*>* >::iterator dom_id;
	std::string dir;
	pthread_t index_thread;
	int s;

	/* flush data to hdd */
	std::cout << "FLUSH" << std::endl;
	sem_wait(&(conf->sem));
	conf->dirs->clear();
	for(dom_id = ob_dom->begin(); dom_id!=ob_dom->end();dom_id++){
		templates = (*dom_id).second; 
		dir = dir_hierarchy(conf,(*dom_id).first);
		for(table = templates->begin(); table!=templates->end();table++){
			conf->dirs->push_back(dir + ((*table).second)->name() + "/");
			(*table).second->flush(dir);
			delete (*table).second;
		}
		delete (*dom_id).second;
	}
	
	sem_post(&(conf->sem));
	s = pthread_create(&index_thread, NULL, reorder_index, conf);
	if (s != 0){
		std::cerr << " Error: pthread_create" << std::endl;
	}
	s = pthread_join(index_thread,NULL);
	if (s != 0){
			std::cerr << " Error: pthread_join" << std::endl;
	}

	delete ob_dom;
	delete conf->index_en_id;
	delete conf->dirs;
	delete conf;
	return 0;
}

