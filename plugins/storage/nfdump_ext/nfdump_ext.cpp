/**
 * \file json.cpp
 * \author Michal Kozubik <kozubik@cesnet.cz>
 * \brief ipficol storage plugin based on json
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
#include <ipfixcol.h>
#include <siso.h>
 
/* API version constant */
IPFIXCOL_API_VERSION;
}

#include <stdexcept>

#include "pugixml.hpp"
#include <libxml/parser.h>

#include "nfdump_ext.h"
#include "Storage.h"
#include "Printer.h"
#include "Sender.h"

static const char *msg_module = "nfdump_ext_storage";

struct nfdump_ext_conf {
	uint64_t intenval;
	std::string *storage_dir
	Storage *storage;
	bool metadata;
	bool align;

};

void process_startup_xml(struct nfdump_ext_conf *conf, char *params) 
{
	xmlDocPtr *doc = NULL;
	xmlNode *cur = NULL;

	/* try to parse configuration file */
	doc = xmlReadMemory(params, strlen(params), "nobase.xml", NULL, 0);
	if (doc == NULL) {
		MSG_ERROR(msg_module, "Plugin configuration not parsed successfully");
		goto err_init;
	}
	cur = xmlDocGetRootElement(doc);
	if (cur == NULL) {
		MSG_ERROR(msg_module, "Empty configuration");
		goto err_xml;
	}
	if (xmlStrcmp(cur->name, (const xmlChar *) "nfdump_ext")) {
		MSG_ERROR(msg_module, "Error in configuration, root node is not nfdump_ext");
		goto err_init;
	}
	cur = cur->xmlChildrenNode;
	while (cur != NULL) {
		/* find out where to look for directory where data storage tree will be created */
		if ((!xmlStrcmp(cur->name, (const xmlChar *) "storagePrefix"))) {
			std::string prefix(xmlNodeListGetString(doc, cur, 1));
			if(prefix != NULL)
				conf->storage  = new std::string(prefix);

		}else if((!xmlStrcmp(cur->name, (const xmlChar *) "timeWindow"))){
			//interval and align

		}else if((!xmlStrcmp(cur->name, (const xmlChar *) "fileOptions"))){
			//prefix, suffix mask, identificator, compression, 

		}
		cur = cur->next;
	}

	/* check whether we have found "interval" element in configuration file */
	if (conf->xml_file == NULL) {
		MSG_WARNING(msg_module, "\"file\" element is missing. No input, "
				"nothing to do");
		goto err_xml;
	}

	pugi::xml_document doc;
	pugi::xml_parse_result result = doc.load(params);


	
	if (!result) {
		throw std::invalid_argument(std::string("Error when parsing parameters: ") + result.description());
	}

	/* Get configuration */
	pugi::xpath_node ie = doc.select_single_node("fileWriter");
	
	/* Check metadata processing */
	//Unnecessary
	std::string meta = ie.node().child_value("metadata");
	conf->metadata = (meta == "yes");

	/* Process all outputs */
	pugi::xpath_node_set outputs = doc.select_nodes("/fileWriter/output");

	for (auto& node: outputs) {
		std::string type = node.node().child_value("type");

		Output *output{NULL};

		if (type == "save") {
		//	output = new Saver(node);
			output = new Printer(node);
		} else if (type == "send") {
			output = new Sender(node);
		} else {
			throw std::invalid_argument("Unknown output type \"" + type + "\"");
		}

		conf->storage->addOutput(output);
	}

	if (!conf->storage->hasSomeOutput()) {
		throw std::invalid_argument("No valid output specified!");
	}
}

/* plugin inicialization */
extern "C"
int storage_init (char *params, void **config)
{	
	try {
		/* Create configuration */
		struct nfdump_ext_conf *conf = new struct nfdump_ext_conf;
		
		/* Create storage */
		conf->storage = new Storage();

		/* Process params */
		process_startup_xml(conf, params);
		
		/* Configure metadata processing */
		conf->storage->setMetadataProcessing(conf->metadata);
		
		/* Save configuration */
		*config = conf;
	} catch (std::exception &e) {
		*config = NULL;
		MSG_ERROR(msg_module, "%s", e.what());
		return 1;
	}
	
	MSG_DEBUG(msg_module, "initialized");
	return 0;
}


extern "C"
int store_packet (void *config, const struct ipfix_message *ipfix_msg,
	const struct ipfix_template_mgr *template_mgr)
{
	(void) template_mgr;
	struct nfdump_ext_conf *conf = (struct nfdump_ext_conf *) config;
	
	conf->storage->storeDataSets(ipfix_msg);
	
	return 0;
}

extern "C"
int store_now (const void *config)
{
	(void) config;
	return 0;
}

extern "C"
int storage_close (void **config)
{
	MSG_DEBUG(msg_module, "CLOSING");
	struct nfdump_ext_conf *conf = (struct nfdump_ext_conf *) *config;
	
	/* Destroy storage */
	delete conf->storage;
	
	/* Destroy configuration */
	delete conf;
	
	*config = NULL;
	
	return 0;
}

