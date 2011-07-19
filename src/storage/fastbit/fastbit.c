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
#include <endian.h>
#include <capi.h>

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


//TODO CHECK RECORD LENGTH IF IS ZERO?!!!!!!!!!!!!!!!!!!!

struct fastbit_conf {
	int cache_size;
	struct element_cache *ie_cache;

};

struct element_cache {
	uint16_t id;
	uint16_t type;
	uint32_t en;
	char name[16];
};

struct data_type{
	char ipfix[25];
	char fastbit[10];
	int (*store)(uint8_t * data, uint16_t length, struct element_cache * cache);
};

int store_array(uint8_t *data, uint16_t length, struct element_cache * cache);
int store_ubyte(uint8_t *data, uint16_t length, struct element_cache * cache);
int store_ushort(uint8_t *data, uint16_t length, struct element_cache * cache);
int store_uint(uint8_t *data, uint16_t length, struct element_cache * cache);
int store_ulong(uint8_t *data, uint16_t length, struct element_cache * cache);
int store_ipv6(uint8_t *data, uint16_t length, struct element_cache * cache);
int store_byte(uint8_t *data, uint16_t length, struct element_cache * cache);
int store_short(uint8_t *data, uint16_t length, struct element_cache * cache);
int store_int(uint8_t *data, uint16_t length, struct element_cache * cache);
int store_long(uint8_t *data, uint16_t length, struct element_cache * cache);
int store_text(uint8_t *data, uint16_t length, struct element_cache * cache);
int store_float(uint8_t *data, uint16_t length, struct element_cache * cache);
int store_double(uint8_t *data, uint16_t length, struct element_cache * cache);


// INTEGER, STRING, FLOAT, OCTETARRAYS can be shirked!
#define TYPES_COUNT 23
struct data_type mtypes[TYPES_COUNT] = {
	{"octetArray", "ubyte", store_array},
	{"unsigned8" , "ubyte", store_ubyte},
	{"unsigned16", "ushort", store_ushort},
	{"unsigned32", "uint", store_uint},
	{"unsigned64", "ulong", store_ulong},
	{"signed8"   , "byte", store_byte},
	{"signed16"  , "short", store_short},
	{"signed32"  , "int", store_int},
	{"signed64"  , "long", store_long},
	{"float32"   , "float", store_float},
	{"float64"   , "double", store_double},
	{"boolean"   , "byte", store_byte},
	{"macAddress", "ulong", store_ulong},
	{"string"    , "text", store_text},
	{"dateTimeSeconds" 	, "uint", store_uint},
	{"dateTimeMilliseconds" , "ulong", store_ulong},
	{"dateTimeMicroseconds" , "ulong", store_ulong},
	{"dateTimeNanoseconds"  , "ulong", store_ulong},
	{"ipv4Address"		, "uint", store_uint},
	{"ipv6Address"		, "ulong", store_ipv6}, // 2x ulong 
	{"basicList"		, "ubyte", store_array}, //? what is this??  TODO
	{"subTemplateList"	, "ubyte", store_array}, //?
	{"subTemplateMultiList"	, "ubyte", store_array} //?
};

//TODO fix this terrible idea
int store_array(uint8_t *data, uint16_t length, struct element_cache * cache){
	char name[25];
	int i;
	int size;
	int offset = 0;
	if(length==65535){ //this is Variable-Length Information Element!
		size = (int) *data;
		offset = 1;
		if( size == 255){ // Size extended!
			size = ntohs(*((uint16_t *) &data[1]));
			offset = 3;
		}
	}
	for(i=0;i<length;i++){
		VERBOSE(CL_VERBOSE_ADVANCED,"ubyte: %u", (int) data[i+offset]); // <-this will spam!
		sprintf(name,"%sp%i", cache->name,i);
		fastbit_add_values(name, mtypes[cache->type].fastbit, &data[i+offset],1,0);
	}
	return 0;
}

int store_ubyte(uint8_t *data, uint16_t length, struct element_cache * cache){
	VERBOSE(CL_VERBOSE_ADVANCED,"ubyte: %u", (int) *data);
	fastbit_add_values(cache->name, mtypes[cache->type].fastbit, data,1,0);
	return 0;
}

int store_ushort(uint8_t *data, uint16_t length, struct element_cache * cache){
#define SIZE 2
	char buf[SIZE];
	if(length==SIZE){
		*((uint16_t *) buf) = ntohs(*((uint16_t *) data));
	}else if (length<SIZE){
		memset(&buf,0,SIZE);
		memcpy(&buf[SIZE-length],data,length);
		*((uint16_t *) buf) = ntohs(*((uint16_t *) buf));
	} else {
		VERBOSE(CL_WARNING,"wrong data typ length");
		return 1;
	}
	VERBOSE(CL_VERBOSE_ADVANCED,"ushort: %u",*((uint16_t *) buf));
	fastbit_add_values(cache->name, mtypes[cache->type].fastbit, (uint16_t *) buf,1,0);
	return 0;
#undef SIZE
}

int store_uint(uint8_t *data, uint16_t length, struct element_cache * cache){
#define SIZE 4
	char buf[SIZE];
	if(length==SIZE){
		*((uint32_t*) buf) = ntohl(*((uint32_t *) data));
	}else if (length<SIZE){
		memset(&buf,0,SIZE);
		memcpy(&buf[SIZE-length],data,length);
		*((uint32_t*) buf) = ntohl(*((uint32_t *) buf));

	} else {
		VERBOSE(CL_WARNING,"wrong data typ length");
		return 1;
	}
	VERBOSE(CL_VERBOSE_ADVANCED,"uint: %u",*((uint32_t *) buf));
	fastbit_add_values(cache->name, mtypes[cache->type].fastbit, (uint32_t *) buf,1,0);
	return 0;
#undef SIZE
}


int store_ulong(uint8_t *data, uint16_t length, struct element_cache * cache){
#define SIZE 8
	char buf[SIZE];
	if(length==SIZE){
		*((uint64_t*) buf) = be64toh(*((uint64_t *) data));
	}else if (length<SIZE){
		memset(&buf,0,SIZE);
		memcpy(&buf[SIZE-length],data,length);
		VERBOSE(CL_WARNING,"BUFFER - %lu", *((uint64_t *)&buf));
		*((uint64_t*) buf) = be64toh(*((uint64_t *) buf));
	} else {
		VERBOSE(CL_WARNING,"wrong data typ length");
		return 1;
	}
	VERBOSE(CL_VERBOSE_ADVANCED,"ulong: %lu",*((uint64_t *)&buf));
	fastbit_add_values(cache->name, mtypes[cache->type].fastbit, (uint64_t *) buf,1,0);
	return 0;
#undef SIZE
}


int store_ipv6(uint8_t *data, uint16_t length, struct element_cache * cache){
#define SIZE 16
	char buf[SIZE];
	char name[20];
	if(length==SIZE){
		*((uint64_t*) buf) = be64toh(*((uint64_t *) data));
		VERBOSE(CL_VERBOSE_ADVANCED,"IPv6-p0 ulong: %lu",*((uint64_t *)&buf));
		sprintf(name,"%sp0", cache->name);
		fastbit_add_values(name, mtypes[cache->type].fastbit, (uint64_t *) buf,1,0);

		*((uint64_t*) buf) = be64toh(*((uint64_t *) &data[8]));
		VERBOSE(CL_VERBOSE_ADVANCED,"IPv6-p1 ulong: %lu",*((uint64_t *)&buf));
		sprintf(name,"%sp1", cache->name);
		fastbit_add_values(name, mtypes[cache->type].fastbit, (uint64_t *) buf,1,0);
		return 0;
	}
	VERBOSE(CL_WARNING,"IPv6 address have wrong size!");
	return 1;
#undef SIZE
}

int store_byte(uint8_t *data, uint16_t length, struct element_cache * cache){
	VERBOSE(CL_VERBOSE_ADVANCED,"byte: %i", (int) *data);
	fastbit_add_values(cache->name, mtypes[cache->type].fastbit, data,1,0);
	return 1;
}

int store_short(uint8_t *data, uint16_t length, struct element_cache * cache){
#define SIZE 2
	char buf[SIZE];
	if(length==SIZE){
		*((int16_t*) buf) = ntohs(*((int16_t *) data));
	}else if (length<SIZE){
		memset(&buf,0,SIZE);
		memcpy(&buf[SIZE-length],data,length);
		*((int16_t*) buf) = ntohs(*((int16_t *) buf));

	} else {
		VERBOSE(CL_WARNING,"wrong data typ length");
		return 1;
	}
	VERBOSE(CL_VERBOSE_ADVANCED,"uint: %i",*((int16_t *) buf));
	fastbit_add_values(cache->name, mtypes[cache->type].fastbit, (int16_t *) buf,1,0);
	return 0;
#undef SIZE
}

int store_int(uint8_t *data, uint16_t length, struct element_cache * cache){
#define SIZE 4
	char buf[SIZE];
	if(length==SIZE){
		*((int32_t*) buf) = ntohl(*((int32_t *) data));
	}else if (length<SIZE){
		memset(&buf,0,SIZE);
		memcpy(&buf[SIZE-length],data,length);
		*((int32_t*) buf) = ntohl(*((int32_t *) buf));

	} else {
		VERBOSE(CL_WARNING,"wrong data typ length");
		return 1;
	}
	VERBOSE(CL_VERBOSE_ADVANCED,"uint: %i",*((uint32_t *) buf));
	fastbit_add_values(cache->name, mtypes[cache->type].fastbit, (uint32_t *) buf,1,0);
	return 0;
#undef SIZE
}


int store_long(uint8_t *data, uint16_t length, struct element_cache * cache){
#define SIZE 8
	char buf[SIZE];
	if(length==SIZE){
		*((int64_t*) buf) = be64toh(*((int64_t *) data));
	}else if (length<SIZE){
		memset(&buf,0,SIZE);
		memcpy(&buf[SIZE-length],data,length);
		*((int64_t*) buf) = be64toh(*((int64_t *) buf));
	} else {
		VERBOSE(CL_WARNING,"wrong data typ length");
		return 1;
	}
	VERBOSE(CL_VERBOSE_ADVANCED,"ulong: %li", *((uint64_t *) buf));
	fastbit_add_values(cache->name, mtypes[cache->type].fastbit, ((uint64_t *)buf),1,0);
	return 0;
#undef SIZE
}


int store_text(uint8_t *data, uint16_t length, struct element_cache * cache){
	VERBOSE(CL_VERBOSE_ADVANCED,"text: %s", data);
	fastbit_add_values(cache->name, mtypes[cache->type].fastbit, data,1,0);
	return 0;
}


int store_float(uint8_t *data, uint16_t length, struct element_cache * cache){
	VERBOSE(CL_VERBOSE_ADVANCED,"float: %f",*((float *) data));
	fastbit_add_values(cache->name, mtypes[cache->type].fastbit, ((float *) data),1,0);
	return 0;
}

int store_double(uint8_t *data, uint16_t length, struct element_cache * cache){
	//TODO can be shrinked
	VERBOSE(CL_VERBOSE_ADVANCED,"double: %lf",*((double *) data));
	fastbit_add_values(cache->name, mtypes[cache->type].fastbit, ((double *) data),1,0);
	return 0;
}
struct element_cache *
cached_element(struct fastbit_conf * conf, uint16_t id, uint16_t enterprise){
	int i;	
	struct element_cache *cache = conf->ie_cache;
	for(i=0;i<conf->cache_size;i++){
		if( cache[i].id == id ){
			if(cache[i].en == enterprise){
				return &(cache[i]);
			}
		}
	}
	return NULL;
}

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
fill_element_cache(struct element_cache **cache, xmlNode *node){
	xmlNode *cnode = NULL;
	xmlNode *enode = NULL;
	int i = 0;
	int j = 0;

	for (cnode = node; cnode; cnode = cnode->next){
		if(xmlStrEqual(cnode->name,(xmlChar *)"element")){
			(*cache)[i].type=0;
			VERBOSE(CL_VERBOSE_ADVANCED,"Element:");
			for (enode = cnode->children; enode; enode = enode->next){
				if(xmlStrEqual(enode->name,(xmlChar *)"enterprise")){
					(*cache)[i].en = atoi((char *)xmlNodeGetContent(enode));
					VERBOSE(CL_VERBOSE_ADVANCED,"\tenterprise:%d",(*cache)[i].en);
				}else if (xmlStrEqual(enode->name,(xmlChar *)"id")){
					(*cache)[i].id = atoi((char *)xmlNodeGetContent(enode));
					VERBOSE(CL_VERBOSE_ADVANCED,"\tid:%d",(*cache)[i].id);

				}else if (xmlStrEqual(enode->name,(xmlChar *)"name")){

				}else if (xmlStrEqual(enode->name,(xmlChar *)"dataType")){
					for(j=0;j<TYPES_COUNT;j++){
						if(xmlStrEqual((xmlChar *)trim((char *)xmlNodeGetContent(enode)),(xmlChar *)mtypes[j].ipfix)){
							(*cache)[i].type = j;
							VERBOSE(CL_VERBOSE_ADVANCED,"\ttype:%d - (%s, %s)",(*cache)[i].type,mtypes[j].ipfix,mtypes[j].fastbit);
						}
					}

				}else if (xmlStrEqual(enode->name,(xmlChar *)"semantic")){
					//TODO
				}else if (xmlStrEqual(enode->name,(xmlChar *)"alias")){
					//TODO
				}
			}
			
			sprintf((*cache)[i].name,"e%iid%hi", (*cache)[i].en, (*cache)[i].id);
			VERBOSE(CL_VERBOSE_ADVANCED,"\tNAME: (%s)",(*cache)[i].name);
			i++;
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

	fastbit_init(NULL);


	xmlDocPtr doc;
	xmlNode *root_element = NULL;

	doc = xmlReadFile(elements_xml, NULL, 0);
	if(doc==NULL){
		VERBOSE(CL_VERBOSE_ADVANCED,"Unable to parse xml file: %s",elements_xml);
	}
	root_element = xmlDocGetRootElement(doc);

	i = element_count(root_element);
	VERBOSE(CL_VERBOSE_ADVANCED,"Element count: %i\n",i);

	*config = (struct fastbit_conf *) malloc(sizeof(struct fastbit_conf)); //TODO return value
	conf = *config;
	conf->ie_cache = (struct element_cache *) malloc(sizeof(struct element_cache) * i); //TODO return value
	conf->cache_size = i;
	cache = conf->ie_cache;

	fill_element_cache(&cache, root_element);
	return 0;
}


void hex(void *ptr, int size){
	int i,space = 0;
	for(i=1;i<size+1;i++){
		if(!((i-1)%16)){
			fprintf(stderr,"%p  ", &((char *)ptr)[i-1]);
		}
		fprintf(stderr,"%02hhx",((char *)ptr)[i-1]);
                if(!(i%8)){
                        if(space){
                                fprintf(stderr,"\n");
                                space = 0;
                                continue;
                        }
                        fprintf(stderr," ");
                        space = 1; 
		}
                fprintf(stderr," ");
	}
}

void hex_dump(const struct ipfix_message *ipfix_msg){
	fprintf(stderr,"-------------hex-PACKET------------\n");
		hex(ipfix_msg->pkt_header,ntohs(ipfix_msg->pkt_header->length));
	fprintf(stderr,"\n------------/hex/-PACKET-----------\n");
}


void hex_dump_adv(const struct ipfix_message *ipfix_msg){
        const struct data_template_couple *dtc = NULL;
        struct ipfix_data_set * data = NULL;
	int i;
        
	for(i=0;i<1023;i++ ){ //TODO magic number!
                dtc = &(ipfix_msg->data_set[i]);
                data = dtc->data_set;
                if(data != NULL){
			fprintf(stderr,"-------------hex-data-set-%i----------\n",i);
			hex(data->records,ntohs(data->header.length)-4); // 4 = set_header size
			fprintf(stderr,"\n------------|hex-data-set-%i|---------\n",i);
			continue;
		} 
		break;
	}	
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
	int ri; //record index
	int record_count;
	int record_offset=0;
	char dir[5];
        const struct data_template_couple *dtc = NULL;
        struct ipfix_data_set * data = NULL;
        struct ipfix_template * template = NULL;

        template_ie *field;
        int32_t enterprise = 0; // enterprise 0 - is reserved by IANA (NOT used)
	struct element_cache *cache_e=NULL;

	hex_dump(ipfix_msg);
	hex_dump_adv(ipfix_msg);


        for(i=0;i<1023;i++ ){ //TODO magic number!
		record_offset=0;
                dtc = &(ipfix_msg->data_set[i]);
                if(dtc->data_set==NULL){
                        VERBOSE(CL_VERBOSE_ADVANCED,"Read %i data_sets!", i);
                        break;
                }

                VERBOSE(CL_VERBOSE_ADVANCED,"Data_set: %i", i);
                data = dtc->data_set;
                template = dtc->template;
                if(template==NULL){
                        VERBOSE(CL_VERBOSE_ADVANCED,"template for data set si missing!");
                        break;
                }

                if(template->template_type != 0){ //its NOT "data" template (skip it)
                        VERBOSE(CL_VERBOSE_ADVANCED,"\tData record %i is not paired with \"common\" template (skiped)", i);
                        continue;
                }


                VERBOSE(CL_VERBOSE_ADVANCED,"\tTemplate id: %i",template->template_id);
                VERBOSE(CL_VERBOSE_ADVANCED,"\tFlowSet id: %i",ntohs(data->header.flowset_id));
		record_count = (ntohs(data->header.length)-4)/template->data_length; // set_header size
                VERBOSE(CL_VERBOSE_ADVANCED,"\tRecord count id: %i - (%i/%i)",record_count,ntohs(data->header.length),template->data_length);

		for(ri=0;ri<record_count;ri++){
                	VERBOSE(CL_VERBOSE_ADVANCED,"\t\tFlow record: %i",ri);
	                for(j=0;j<dtc->template->field_count;j++){
        	                //TODO record size check!!
                	        field = &(template->fields[j]);
                        	if(field->ie.id & 0x8000){ //its ENTERPRISE element
                                	j++;
	                                enterprise = template->fields[j].enterprise_number;
        	                }
                	        VERBOSE(CL_VERBOSE_ADVANCED,"\t\t\tField id: %i length: %i enum: %i",field->ie.id & 0x7FFF,field->ie.length, enterprise);
                        	//check_element(field->id & 0x7FFF, enterprise); //check if its known element -> it have description in config file.

				cache_e = cached_element(config, field->ie.id & 0x7FFF, enterprise);
				if(cache_e != NULL){
                	        	VERBOSE(CL_VERBOSE_ADVANCED,"\t\t\t\tType: %s - %s - offset:%i - %p", mtypes[cache_e->type].ipfix, mtypes[cache_e->type].fastbit,record_offset,&(data->records[record_offset]));
					mtypes[cache_e->type].store(&(data->records[record_offset]),field->ie.length, cache_e);
					record_offset+=field->ie.length;
				} else {
                	        	VERBOSE(CL_VERBOSE_ADVANCED,"\t\t\t\tType: UNKNOWN!");
				}

                	}
		}
		sprintf(dir,"%i",ntohs(data->header.flowset_id));
		fastbit_flush_buffer(dir);
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

