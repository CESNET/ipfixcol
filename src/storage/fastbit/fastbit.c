/**
 * \file storage.h
 * \author Radek Krejci <rkrejci@cesnet.cz>
 * \brief IPFIX Collector Storage API.
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


#include <commlbr.h>
#include "../../../headers/storage.h"
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>


#include <libxml/parser.h>
#include <libxml/tree.h>

/**
 * \defgroup storageAPI Storage Plugins API
 * \ingroup publicAPIs
 *
 * These functions specifies a communication interface between ipficol core,
 * namely a data manager handling specific Observation Domain ID, and storage
 * plugins. More precisely, each storage plugin communicates with a separated
 * thread of the data manager.
 *
 * \image html arch_scheme_core_comm.png "ipfixcol internal communication"
 * \image latex arch_scheme_core_comm.png "ipfixcol internal communication" width=10cm
 *
 * @{
 */


struct fastbit_conf {
	int cache_size;
	struct element_cache *ie_cache;

};

struct element_cache {
	uint16_t id;
	uint16_t type;
	uint32_t en;
};


#define TYPES_COUNT 23
char types[TYPES_COUNT][2][21] = {
	{"octetArray", "text"},
	{"unsigned8" , "ubyte"},
	{"unsigned16", "ushort"},
	{"unsigned32", "uint"},
	{"unsigned64", "ulong"},
	{"signed8"   , "byte"},
	{"signed16"  , "short"},
	{"signed32"  , "int"},
	{"signed64"  , "long"},
	{"float32"   , "float"},
	{"float64"   , "double"},
	{"boolean"   , "byte"},
	{"macAddress", "text"},
	{"string"    , "text"},
	{"dateTimeSeconds" 	, "uint"},
	{"dateTimeMilliseconds" , "ulong"},
	{"dateTimeMicroseconds" , "ulong"},
	{"dateTimeNanoseconds"  , "ulong"},
	{"ipv4Address"		, "uint"},
	{"ipv6Address"		, "text"},
	{"basicList"		, "text"}, //? what is this??  TODO
	{"subTemplateList"	, "text"}, //?
	{"subTemplateMultiList"	, "text"} //?
};


int
element_count(xmlNode * node)
{
	xmlNode *cnode = NULL;
	int count = 0;

	for (cnode = node; cnode; cnode = cnode->next){
		if(xmlStrEqual((const xmlChar *)cnode->name,(const xmlChar *)"element")){
			count++;
		}
		count += element_count(cnode->children);
	}
	return count;
}

char *trim(char *str){
	int i,j,len;
	len = strlen(str);
	for(i=0;i<len;i++){
		if(str[i]!=' '){
			break;
		}
	}
	for(j=i;j<len;j++){
		if(str[j]==' '){
			break;
		}
	}
	if( i!=0 || j!=len){
		memcpy(str,&(str[i]),len);
		str[j-i]=0;
	}
	return str;
}

void
fill_element_cache(struct element_cache *cache, xmlNode *node){
	xmlNode *cnode = NULL;
	xmlNode *enode = NULL;
	int i = 0;
	int j = 0;

	for (cnode = node; cnode; cnode = cnode->next){
		cache[i].type=0;
		if(xmlStrEqual(cnode->name,(xmlChar *)"element")){
			VERBOSE(CL_VERBOSE_ADVANCED,"Element:");
			for (enode = cnode->children; enode; enode = enode->next){
				if(xmlStrEqual(enode->name,(xmlChar *)"enterprise")){
					cache[i].en = atoi((char *)xmlNodeGetContent(enode));
					VERBOSE(CL_VERBOSE_ADVANCED,"\tenterprise:%d",cache[i].en);
				}else if (xmlStrEqual(enode->name,(xmlChar *)"id")){
					cache[i].id = atoi((char *)xmlNodeGetContent(enode));
					VERBOSE(CL_VERBOSE_ADVANCED,"\tid:%d",cache[i].id);

				}else if (xmlStrEqual(enode->name,(xmlChar *)"name")){

				}else if (xmlStrEqual(enode->name,(xmlChar *)"dataType")){
					for(j=0;j<TYPES_COUNT;j++){
						if(xmlStrEqual((xmlChar *)trim((char *)xmlNodeGetContent(enode)),(xmlChar *)types[j][0])){
							cache[i].type = j;
							VERBOSE(CL_VERBOSE_ADVANCED,"\ttype:%d - (%s, %s)",cache[i].type,types[j][0],types[j][1]);
						}
					}

				}else if (xmlStrEqual(enode->name,(xmlChar *)"semantic")){
					//TODO
				}else if (xmlStrEqual(enode->name,(xmlChar *)"alias")){
					//TODO
				}
			}
		}
	fill_element_cache(cache,cnode->children);
	}
}

/**
 * \brief Storage plugin initialization function.
 *
 * The function is called just once before any other storage API's function.
 *
 * \param[in]  params  String with specific parameters for the storage plugin.
 * \param[out] config  Plugin-specific configuration structure. ipfixcol is not
 * involved in the config's structure, it is just passed to the following calls
 * of storage API's functions.
 * \return 0 on success, nonzero else.
 */
int storage_init (char *params, void **config){
	VERBOSE(CL_VERBOSE_ADVANCED,"Fastbit config init");
	int i=0;
	char elements_xml[] = "/etc/ipfixcol/ipfix-elements.xml";

	struct fastbit_conf * conf;
	struct element_cache *cache;

	xmlDocPtr doc;
	xmlNode *root_element = NULL;

	doc = xmlReadFile(elements_xml, NULL, 0);
	if(doc==NULL){
		VERBOSE(CL_VERBOSE_ADVANCED,"Unable to parse xml file: %s",elements_xml);
	}
	root_element = xmlDocGetRootElement(doc);

	VERBOSE(CL_VERBOSE_ADVANCED,"Element count: %i\n",element_count(root_element));

	*config = (struct fastbit_conf *) malloc(sizeof(struct fastbit_conf)); //TODO return value
	conf = *config;
	conf->ie_cache = (struct element_cache *) malloc(sizeof(struct element_cache) * i); //TODO return value
	conf->cache_size = i;
	cache = conf->ie_cache;

	fill_element_cache(cache, root_element);
	return 0;
}

/**
 * \brief Pass IPFIX data with supplemental structures from ipfixcol core into
 * the storage plugin.
 *
 * The way of data processing is completely up to the specific storage plugin.
 * The basic usage is to store all data in a specific format, but also various
 * processing (statistics, etc.) can be done by storage plugin. In general any
 * processing with IPFIX data can be done by the storage plugin.
 *
 * \param[in] config     Plugin-specific configuration data prepared by init
 * function.
 * \param[in] ipfix_msg  Covering structure including IPFIX data as well as
 * supplementary structures for better/faster parsing of IPFIX data.
 * \param[in] templates  The list of preprocessed templates for possible
 * better/faster data processing.
 * \return 0 on success, nonzero else.
 */
int store_packet (void *config, const struct ipfix_message *ipfix_msg,
	const struct ipfix_template_mgr *template_mgr){
	VERBOSE(CL_VERBOSE_ADVANCED,"Fastbit store_packet");

        int i,j;
        const struct data_template_couple *dtc = NULL;
        struct ipfix_data_set * data = NULL;
        struct ipfix_template * template = NULL;

        template_ie *field;
        int32_t enterprise = 0; // enterprise 0 - is reserved by IANA (NOT used)

        for(i=0;i<1023;i++ ){ //TODO magic number!
                dtc = &(ipfix_msg->data_set[i]);
                if(dtc->data_set==NULL){
                        VERBOSE(CL_VERBOSE_ADVANCED,"Read %i data_sets!", i);
                        break;
                }

                VERBOSE(CL_VERBOSE_ADVANCED,"Data_set: %i", i);
                data = dtc->data_set;
                template = dtc->template;

                if(template->template_type != 0){ //its NOT "data" template (skip it)
                        VERBOSE(CL_VERBOSE_ADVANCED,"\tData record %i is not paired with \"common\" template (skiped)", i);
                        continue;
                }


                VERBOSE(CL_VERBOSE_ADVANCED,"\tTemplate id: %i",template->template_id);
                VERBOSE(CL_VERBOSE_ADVANCED,"\tFlowSet id: %i",ntohs(data->header.flowset_id));

                for(j=0;j<dtc->template->field_count;j++){
                        //TODO record size check!!
                        field = &(template->fields[j]);
                        if(field->ie.id & 0x8000){ //its ENTERPRISE element
                                j++;
                                enterprise = template->fields[j].enterprise_number;
                        }
                        VERBOSE(CL_VERBOSE_ADVANCED,"\t\tField id: %i length: %i enum: %i",field->ie.id & 0x7FFF,field->ie.length, enterprise);
                        //check_element(field->id & 0x7FFF, enterprise); //check if its known element -> it have description in config file.
                }
        }

	return 0;  
}
/**
 * \brief Announce willing to store currently processing data.
 *
 * This way ipfixcol announces willing to store immediately as much data as
 * possible. The impulse to this action is taken from the user and broadcasted
 * by ipfixcol to all storage plugins. The real response of the storage plugin
 * is completely up to the specific plugin.
 *
 * \param[in] config  Plugin-specific configuration data prepared by init
 * function.
 * \return 0 on success, nonzero else.
 */
int store_now (const void *config){
	VERBOSE(CL_VERBOSE_ADVANCED,"Fastbit store_packet");
	return 0;
}


/**
 * \brief Storage plugin "destructor".
 *
 * Clean up all used plugin-specific structures and memory allocations. This
 * function is used only once as a last function call of the specific storage
 * plugin.
 *
 * \param[in,out] config  Plugin-specific configuration data prepared by init
 * \return 0 on success and config is changed to NULL, nonzero else.
 */
int storage_close (void **config){
	VERBOSE(CL_VERBOSE_ADVANCED,"Fastbit config close");
	free(((struct fastbit_conf *) *config)->ie_cache);
	free(*config);
	return 0;
}

/**@}*/

//mlock = PTHREAD_MUTEX_INITIALIZER;
//int main() {
//	void *conf;
//	verbose = CL_VERBOSE_ADVANCED;


//	storage_init (NULL, &conf);
//	store_packet (conf, NULL, NULL);

//	store_now (conf);
//	storage_close (&conf);
//}

