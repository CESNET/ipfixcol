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
	#include <commlbr.h>
	#include "../../../headers/storage.h"
}
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
//#include <endian.h>
#include <time.h>

#include "fastbit_table.h"
#include "fastbit_element.h"

#include <fastbit/ibis.h>

#include <map>
#include <iostream>
#include <iomanip>
#include <string>
#include "pugixml/pugixml.hpp"


/* this enum specifies types of file naming strategy */
enum name_type{TIME,INCREMENTAL};

/* fastbit configuration structure */
struct fastbit_config{
	//std::map<uint32_t,(std::map<uint16_t,template_table*>*)> *ob_dom;
	std::map<uint32_t,std::map<uint16_t,template_table*>* > *ob_dom;
        std::map<uint16_t,template_table*> *templates; /* map with template id / template_table pairs */
        int time_window; 		/* specifies time interval for storage direcotry rotation (0 = no time based rotation ) */
        int records_window;		/* specifies record count for storage direcotry rotation (0 = no record based rotation ) */
        enum name_type dump_name; 	/* hold type of name strategy for storage direcotry rotation */
        std::string sys_dir;		/* path to direcotry where should be storage direcotry flushed */
        std::string window_dir;		/* current window direcotry */
        std::string prefix;		/* user prefix for storage direcotry */
        time_t last_flush;		/* time of last flush (used for time based rotation, name is based on start of interval not its end!) */
        int indexes;			/* if this variable holds true value indexes are created on storage direcotry flush*/
};


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
	std::cout << "Final hierarchy: " << dir << std::endl;
	return dir;
}

/* plugin inicialization */
extern "C"
int storage_init (char *params, void **config){
	VERBOSE(CL_VERBOSE_BASIC,"Fastbit plugin: initialization");

	struct tm * timeinfo;
	char formated_time[15];
	ibis::fileManager::adjustCacheSize(1000000000000);

	/* create config structure! */
	(*config) = (struct fastbit_config *)malloc(sizeof( struct fastbit_config));
	(*config) = new  struct fastbit_config;

	/* inicialize template map */
	((struct fastbit_config*)(*config))->templates = new std::map<uint16_t,template_table*>;

	struct fastbit_config* c = (struct fastbit_config*)(*config);
	
	c->ob_dom = new std::map<uint32_t,std::map<uint16_t,template_table*>* >;

	/* parse configuratin xml and upated configure structure accorging to it */
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

		indexes=ie.node().child_value("onTheFlyIndexes");
		if(indexes == "yes"){
			c->indexes = 1;
		} else {
			c->indexes = 0;
		}

	        ie = doc.select_single_node("fileWriter/dumpInterval");
        	timeWindow=ie.node().child_value("timeWindow");
		c->time_window = atoi(timeWindow.c_str());

        	recordLimit=ie.node().child_value("recordLimit");
		c->records_window = atoi(recordLimit.c_str());

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
	} else {
		VERBOSE(CL_ERROR,"Fastbit plugin: ERROR Unable to parse configuration xml!");
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
	ibis::table *index_table;
	uint32_t oid = 0;
	std::string dir;

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
			//skip data without tamplate!
			continue;
		}

		uint16_t template_id = ipfix_msg->data_couple[i].data_template->template_id;


		/* if there is unknow template parse it and add it to template map */
		if((table = templates->find(template_id)) == templates->end()){
			std::cout << "NEW TEMPLATE: " << template_id << std::endl;
			template_table *table_tmp = new template_table(template_id);
			table_tmp->parse_template(ipfix_msg->data_couple[i].data_template);
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
		}
		if(conf->time_window !=0){
			time ( &rawtime );
			if(difftime(rawtime,conf->last_flush) > conf->time_window){
				flush=1;
				conf->last_flush += conf->time_window;
			}
		}

		if(flush){
			flushed ++;
			std::cout << "FLUSH" << std::endl;
			/* flush all templates! */
			for(dom_id = ob_dom->begin(); dom_id!=ob_dom->end();dom_id++){
				templates = (*dom_id).second;
				dir = dir_hierarchy(conf,(*dom_id).first);
				for(table = templates->begin(); table!=templates->end();table++){
					(*table).second->flush(dir);
					if(conf->indexes){
						std::cout << "Creating indexes: "<< dir << std::endl;
						index_table = ibis::table::create(dir.c_str());
						index_table->buildIndexes();
						delete index_table;
					}
				}
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
	struct fastbit_config *conf = (struct fastbit_config *) (*config);
	std::map<uint16_t,template_table*> *templates = conf->templates;
	ibis::table *index_table;
	std::map<uint32_t,std::map<uint16_t,template_table*>* > *ob_dom = conf->ob_dom;
	std::map<uint32_t,std::map<uint16_t,template_table*>* >::iterator dom_id;
	std::string dir;

	/* flush data to hdd */
	std::cout << "FLUSH" << std::endl;

	for(dom_id = ob_dom->begin(); dom_id!=ob_dom->end();dom_id++){
		templates = (*dom_id).second; 
		dir = dir_hierarchy(conf,(*dom_id).first);

		for(table = templates->begin(); table!=templates->end();table++){
			(*table).second->flush(dir);
			if(conf->indexes){
				std::cout << "Creating indexes: "<< dir << std::endl;
				index_table = ibis::table::create(dir.c_str());
				index_table->buildIndexes();
				delete index_table;
			}
			delete (*table).second;
		}
		delete (*dom_id).second;
	}
	
	delete ob_dom;
	return 0;
}

