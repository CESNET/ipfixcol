/**
 * \file nfstore.cpp
 * \author Petr Kramolis <kramolis@cesnet.cz>
 * \brief nfdump storage plugin
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
#include <sys/stat.h>
#include <errno.h>
#include <lzo/lzoconf.h>

#include <map>
#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>
#include <fstream>
#include <iostream>

#include "pugixml.hpp"
#include "config_struct.h"
#include "nfstore.h"

std::string DirHierarchy(struct nfdumpConfig *config, uint32_t oid){
	struct tm * timeinfo;

	const int ft_size = 1000;
	char formated_time[ft_size];
	std::string dir;
	size_t o_loc = 0; //observation id location in str

	std::stringstream ss;
	std::string domain_id;

	timeinfo = localtime(&(config->lastFlush));

	ss << oid;
	domain_id = ss.str();

	strftime(formated_time,ft_size,(config->sysDir).c_str(),timeinfo);

	dir = std::string(formated_time);
	while ((o_loc = dir.find ( "%o", o_loc)) != std::string::npos){
		dir.replace (o_loc, 2, domain_id);
	}

	dir+= config->windowDir;
	return dir;
}

int DirCheck_r(std::string path){
	size_t pos;
	if(mkdir(path.c_str(), 0777) != 0){
		if(errno == EEXIST) //dir already exists
			return 0;
		if(errno == ENOENT){ //check parent directory
			pos = path.find_last_of("/\\");
			if(pos == std::string::npos){
				MSG_ERROR(MSG_MODULE,"Error while creating directory: %s",path.c_str());
				return 1;
			}
			DirCheck_r(path.substr(0,pos));
			//try create dir again
			if(mkdir(path.c_str(),0777) != 0){
				MSG_ERROR(MSG_MODULE,"Error while creating directory: %s",path.c_str());
				return 1;
			}
			return 0;
		}
		//other error
		MSG_ERROR(MSG_MODULE,"Error while creating directory: %s",path.c_str());
		return 1;
	}
	return 0;
}

int DirCheck(std::string path){
	size_t pos;
	pos = path.find_last_of("/\\");
		if(pos == std::string::npos){
			MSG_ERROR(MSG_MODULE,"Error while creating directory: %s",path.c_str());
			return 1;
		}
	return DirCheck_r(path.substr(0,pos));
}


void updateFileName(struct nfdumpConfig *conf){
	struct tm * timeinfo;
	char formatedTime[15];

	// change window directory name!
	timeinfo = localtime ( &(conf->lastFlush));
	strftime(formatedTime,15,"%Y%m%d%H%M",timeinfo);
	conf->windowDir = conf->prefix + std::string(formatedTime);
}


int processStartupXML(char *params, struct nfdumpConfig* c){
	std::string tmp;

	pugi::xml_document doc;
	doc.load(params);

	if(doc){
		pugi::xpath_node ie = doc.select_single_node("fileWriter");
		tmp=ie.node().child_value("path");
		if(tmp==""){
			MSG_WARNING(MSG_MODULE,"Storage path is not specified! Data are stored in local direcotry!");
			tmp=".";
		}
		//make sure path ends with "/" character
		if(tmp.at(tmp.size() -1) != '/'){
			c->sysDir = tmp + "/";
		} else {
			c->sysDir = tmp;
		}

		tmp=ie.node().child_value("prefix");
		c->prefix = tmp; //no need for default value error value is "" anyway

		tmp=ie.node().child_value("ident");
		if(tmp != ""){
			if(tmp.length() >= IDENTLEN){
				tmp.resize(IDENTLEN);
				c->ident = tmp;
				MSG_WARNING(MSG_MODULE,"Identification string is too long (max length is %u)",IDENTLEN -1);
				MSG_WARNING(MSG_MODULE,"Identification string set to: %s",tmp.c_str());
			} else{
				c->ident = tmp;
			}
		}else{
			c->ident = "none";
		}

		tmp=ie.node().child_value("compression");
		if(tmp == "yes"){
			c->compression = true;
			if (lzo_init() != LZO_E_OK){
				MSG_WARNING(MSG_MODULE,"Compression initialization failed (storing without compression)!");
				c->compression = false;
			}
		} else {
			c->compression = false;
		}

		ie = doc.select_single_node("fileWriter/dumpInterval");
		tmp=ie.node().child_value("timeWindow");
		c->timeWindow = atoi(tmp.c_str());
		if(c->timeWindow == 0){
			c->timeWindow = 360;
		}

		tmp=ie.node().child_value("bufferSize");
		c->bufferSize = atoi(tmp.c_str());
		if(c->bufferSize == 0){
			c->bufferSize = BUFFER_SIZE;
		}

		tmp=ie.node().child_value("timeAlignment");

		time ( &(c->lastFlush));
		if(tmp == "yes"){
			/* operators '/' and '*' are used for round down time to time window */
			c->lastFlush = ((c->lastFlush/c->timeWindow) * c->timeWindow);
		}
		updateFileName(c);
	} else {
		return 1;
	}
	return 0;
}

/* plug-in initialization */
extern "C"
int storage_init (char *params, void **config){
	MSG_DEBUG(MSG_MODULE, "initialization");
	struct nfdumpConfig* c = NULL;

	/* create config structure! */
	(*config) = new  struct nfdumpConfig;
	if(*config == NULL){
		MSG_ERROR(MSG_MODULE,"Can't allocate memory for config structure");
		return 1;
	}
	c = (struct nfdumpConfig *) (*config);

	/* allocate map for observation id to record map conversion */
	c->files = new std::map<uint32_t,NfdumpFile*>;
	if(c->files == NULL){
		MSG_ERROR(MSG_MODULE, "Can't allocate memory for config structure");
		return 1;
	}

	/* parse configuration xml and updated configure structure according to it */
	if(processStartupXML(params, c)){
		MSG_ERROR(MSG_MODULE, "Unable to parse configuration xml!");
		return 1;
	}
	return 0;
}

extern "C"
int store_packet (void *config,	const struct ipfix_message *ipfix_msg,
	const struct ipfix_template_mgr *){
	MSG_DEBUG(MSG_MODULE, "store packet");
	struct nfdumpConfig *conf = (struct nfdumpConfig *) config;
	std::map<uint32_t,NfdumpFile*>::iterator files_it;
	NfdumpFile *file_tmp = NULL;
	std::string file_path;
	time_t rawtime;
	uint32_t oid = -1;

	//should we create new window?
	time ( &rawtime );
	if(difftime(rawtime,conf->lastFlush) > conf->timeWindow){
		conf->lastFlush = conf->lastFlush + conf->timeWindow;
		while(difftime(rawtime,conf->lastFlush) > conf->timeWindow){
			conf->lastFlush = conf->lastFlush + conf->timeWindow;
		}
		updateFileName(conf);

		for(files_it = conf->files->begin(); files_it!=conf->files->end();files_it++){
			files_it->second->closeFile();
			file_path = DirHierarchy(conf,files_it->first);
			DirCheck(file_path);
			files_it->second->newFile(file_path, conf);
		}
	}

	oid = ntohl(ipfix_msg->pkt_header->observation_domain_id);

	/* if there is unknown observation id add it to files map */
	if((files_it = conf->files->find(oid)) == conf->files->end()){
		MSG_DEBUG(MSG_MODULE,"Received new observation id: %hu", oid);
		file_tmp = new NfdumpFile();
		file_path = DirHierarchy(conf,oid);
		DirCheck(file_path);
		file_tmp->newFile(file_path, conf);
		(*conf->files)[oid] = file_tmp;
		files_it = conf->files->find(oid);
	}

	unsigned int recFlows;
	recFlows = files_it->second->bufferPtk(ipfix_msg->data_couple);
	files_it->second->checkSQNumber(ntohl(ipfix_msg->pkt_header->sequence_number), recFlows);
	return 0;
}

extern "C"
int store_now (const void *){
	MSG_DEBUG(MSG_MODULE,"STORE_NOW");
	return 0;
}

extern "C"
int storage_close (void **config){
	MSG_DEBUG(MSG_MODULE,"CLOSE");
	struct nfdumpConfig *conf = (struct nfdumpConfig *) (*config);
	std::map<uint32_t,NfdumpFile*>::iterator files_it;


	for(files_it = conf->files->begin(); files_it!=conf->files->end();files_it++){
		files_it->second->closeFile();
		delete files_it->second;
	}

	delete conf->files;
	delete conf;

	return 0;
}

