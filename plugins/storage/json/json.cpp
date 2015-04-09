/**
 * \file json.cpp
 * \author Michal Kozubik <kozubik@cesnet.cz>
 * \brief ipficol storage plugin based on json
 *
 * Copyright (C) 2014 CESNET, z.s.p.o.
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
}

#include <stdexcept>

#include "pugixml.hpp"
#include "Storage.h"

static const char *msg_module = "json_storage";

#define DEFAULT_IP		"127.0.0.1"
#define DEFAULT_PORT	"4739"
#define DEFAULT_TYPE	"UDP"

struct json_conf {
	bool metadata;
	bool printOnly;
	Storage *storage;
	sisoconf *sender;
};

void process_startup_xml(struct json_conf *conf, char *params) 
{
	pugi::xml_document doc;
	
	/* parse params */
	pugi::xml_parse_result result = doc.load(params);
	
	if (!result) {
		throw std::invalid_argument(std::string("Error when parsing parameters: ") + result.description());
	}

	/* get values */
	pugi::xpath_node ie = doc.select_single_node("fileWriter");
	
	std::string ip   = ie.node().child_value("ip");
	std::string port = ie.node().child_value("port");
	std::string type = ie.node().child_value("type");
	std::string meta = ie.node().child_value("metadata");
	std::string print= ie.node().child_value("printOnly");
	
	/* Check metadata processing */
	conf->metadata = (meta == "yes");

	/* Print or send data? */
	conf->printOnly = (print == "yes");

	if (conf->printOnly) {
		return;
	}

	/* Check IP address */
	if (ip.empty()) {
		MSG_WARNING(msg_module, "IP address not set, using default: %s", DEFAULT_IP);
		ip = DEFAULT_IP;
	}
	
	/* Check port number */
	if (port.empty()) {
		MSG_WARNING(msg_module, "Port number not set, using default: %s", DEFAULT_PORT);
		port = DEFAULT_PORT;
	}
	
	/* Check connection type */
	if (type.empty()) {
		MSG_WARNING(msg_module, "Connection type not set, using default: %s", DEFAULT_TYPE);
		type = DEFAULT_TYPE;
	}
	
	/* Create sender */
	conf->sender = siso_create();
	if (!conf->sender) {
		throw std::runtime_error("Memory error - cannot create sender object");
	}
	
	/* Connect it */
	if (siso_create_connection(conf->sender, ip.c_str(), port.c_str(), type.c_str()) != SISO_OK) {
		throw std::runtime_error(siso_get_last_err(conf->sender));
	}
}

/* plugin inicialization */
extern "C"
int storage_init (char *params, void **config)
{	
	try {
		/* Create configuration */
		struct json_conf *conf = new struct json_conf;
		
		/* Process params */
		process_startup_xml(conf, params);
		
		/* Create storage */
		conf->storage = new Storage(conf->sender);

		conf->storage->setPrintOnly(conf->printOnly);
		
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
	struct json_conf *conf = (struct json_conf *) config;
	
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
	struct json_conf *conf = (struct json_conf *) *config;
	
	/* Destroy sender */
	if (!conf->printOnly) {
		siso_destroy(conf->sender);
	}
	
	/* Destroy storage */
	delete conf->storage;
	
	/* Destroy configuration */
	delete conf;
	
	*config = NULL;
	
	return 0;
}

