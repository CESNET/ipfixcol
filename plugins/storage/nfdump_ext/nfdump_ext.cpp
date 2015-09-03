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
#include <sstream>

#include "pugixml.hpp"

#include "nfdump_ext.h"
#include "Storage.h"


static const char *msg_module = "nfdump_ext_storage";

#define DEF_TIME_WINDOW 300
#define DEF_PREFIX "lnfstore."
#define DEF_STORAGE_PATH ""
#define DEF_SUFFIX_MASK "%F%R"
#define DEF_IDENT "lnfstore"
#define DEF_UTILIZE_CHANNELS false
#define DEF_COMPRESS false
#define DEF_ALIGN true

struct nfdump_ext_conf
{
    Storage *storage;

    bool utilize_channels;
    bool compress;
	bool align;
};


void process_startup_xml(struct nfdump_ext_conf *conf, char *params) 
{
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load(params);

    if (!result) {
        throw std::invalid_argument(std::string("Error when parsing parameters: ") + result.description());
    }

    /* Get configuration */
    pugi::xpath_node ie = doc.select_single_node("fileWriter");
    std::string value = (ie.node().child_value("fileFormat"));
    if( value != "nfdump_ext"){
        throw std::invalid_argument(std::string("Bad file writer name: ") + value);
    }

    /* Check metadata processing */
    std::string tmp = ie.node().child_value("utilizeChannels");
    conf->storage->setUtilizeChannels(tmp == "yes");

    tmp = ie.node().child_value("storagePath");
    conf->storage->setStoragePath(tmp);

    tmp = ie.node().child_value("prefix");
    conf->storage->setNamePrefix(tmp);

    tmp = ie.node().child_value("suffixMask");
    conf->storage->setNameSuffixMask(tmp);

    tmp = ie.node().child_value("identificatorField");
    conf->storage->setIdentificator(tmp.substr(0, 128));

    /* Process all outputs */
    pugi::xpath_node_set opts = doc.select_nodes("/fileWriter/dumpInterval/*");

    std::stringstream ss;
    for (auto& node: opts) {
        value = node.node().name();
        if(value == "timeWindow"){
            time_t seconds;
            ss << node.node().child_value();
            ss >> seconds;
            conf->storage->setTimeWindow(seconds);
            continue;
        }else if(value == "align"){
            conf->storage->setWindowAlignment(node.node().child_value() == std::string("yes"));
            continue;
        }
        throw std::invalid_argument(std::string("Not a valid option !")+ value);
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
        //conf->storage->setMetadataProcessing(conf->metadata);
		
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

