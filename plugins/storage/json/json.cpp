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

#include <cstring>
#include <stdexcept>
#include "pugixml.hpp"

#include "json.h"
#include "Storage.h"
#include "Printer.h"
#include "Sender.h"
#include "Server.h"
#include "File.h"
#include "Kafka.h"

static const char *msg_module = "json_storage";

void process_startup_xml(struct json_conf *conf, char *params) 
{
	pugi::xml_document doc;
	pugi::xml_parse_result result = doc.load(params);
	
	if (!result) {
		throw std::invalid_argument(std::string("Error when parsing parameters: ") + result.description());
	}

	/* Get configuration */
	pugi::xpath_node ie = doc.select_single_node("fileWriter");
	
	/* Check metadata processing */
	std::string meta = ie.node().child_value("metadata");
	conf->metadata = (strcasecmp(meta.c_str(), "yes") == 0 || meta == "1" ||
		strcasecmp(meta.c_str(), "true") == 0);

	/* Format of TCP flags */
	std::string tcpFlags = ie.node().child_value("tcpFlags");
	conf->tcpFlags = (strcasecmp(tcpFlags.c_str(), "formated") == 0) ||
                (strcasecmp(tcpFlags.c_str(), "formatted") == 0);

	/* Format of timestamps */
	std::string timestamp = ie.node().child_value("timestamp");
	conf->timestamp = (strcasecmp(timestamp.c_str(), "formated") == 0) ||
                (strcasecmp(timestamp.c_str(), "formatted") == 0);

	/* Format of protocols */
	std::string protocol = ie.node().child_value("protocol");
	conf->protocol = (strcasecmp(protocol.c_str(), "raw") == 0);

	/* Ignore unknown elements */
	std::string unknown = ie.node().child_value("ignoreUnknown");
	conf->ignoreUnknown = true;
	if (strcasecmp(unknown.c_str(), "false") == 0 || unknown == "0" ||
			strcasecmp(unknown.c_str(), "no") == 0) {
		conf->ignoreUnknown = false;
	}

	/* Convert and print white spaces in JSON strings */
	std::string whiteSpaces = ie.node().child_value("nonPrintableChar");
	conf->whiteSpaces = true;
	if (strcasecmp(whiteSpaces.c_str(), "false") == 0 || whiteSpaces == "0" ||
			strcasecmp(whiteSpaces.c_str(), "no") == 0) {
		conf->whiteSpaces = false;
	}

	/* Prefix for IPFIX elements */
	/* Set default rpefix */
	conf->prefix = "ipfix.";
	/* Override the default from configuration */
	if (ie.node().child("prefix")) {
		conf->prefix = ie.node().child_value("prefix");
	}

	/* Process all outputs */
	pugi::xpath_node_set outputs = doc.select_nodes("/fileWriter/output");

	for (auto& node: outputs) {
		std::string type = node.node().child_value("type");

		Output *output{NULL};

		if (type == "print") {
			output = new Printer(node);
		} else if (type == "send") {
			output = new Sender(node);
		} else if (type == "server") {
			output = new Server(node);
		} else if (type == "file") {
			output = new File(node);
		} else if (type == "kafka") {
			output = new Kafka(node);
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
	struct json_conf *conf;
	try {
		/* Create configuration */
		conf = new struct json_conf;
		
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

		/* Free allocated memory */
		delete conf->storage;
		delete conf;

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
	
	conf->storage->storeDataSets(ipfix_msg, conf);
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
	
	/* Destroy storage */
	delete conf->storage;
	
	/* Destroy configuration */
	delete conf;
	
	*config = NULL;
	
	return 0;
}

