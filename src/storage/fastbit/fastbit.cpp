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
#include "storage.h"
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
//#include <endian.h>
#include <time.h>


#include <ibis.h>

#include <map>
#include <iostream>
#include <string>
#include "pugixml/pugixml.hpp"
const unsigned int RESERVED_SPACE = 200000;
const int IE_NAME_LENGTH = 16;
const int TYPE_NAME_LENGTH = 10;
const char ELEMENTS_XML[] = "/etc/ipfixcol/ipfix-elements.xml";



enum store_type{UINT,INT,BLOB,TEXT,FLOAT,IPv6,UNKNOWN};

enum store_type get_type_from_xml(int en, int id){
	pugi::xml_document doc;
	char node_query[50];
	std::string type;
	if (!doc.load_file("/etc/ipfixcol/ipfix-elements.xml")) return UINT;


	sprintf(node_query,"//element[enterprise='%u' and id='%u']",en,id);
	pugi::xpath_node ie = doc.select_single_node(node_query);
        type=ie.node().child_value("dataType");
	if(type =="unsigned8" or  type =="unsigned16" or type =="unsigned32" or type =="unsigned64" or \
	   type =="dateTimeSeconds" or type =="dateTimeMilliseconds" or type =="dateTimeMicroseconds" or \
           type =="dateTimeNanoseconds" or type =="ipv4Address" or type =="macAddress" or type == "boolean"){
		return UINT;
	}else if(type =="signed8" or type =="signed16" or type =="signed32" or type =="signed64" ){
		return INT;
	}else if(type =="ipv6Address"){
		return IPv6;
	}else if(type =="float32" or type =="float64"){
		return FLOAT;
	}else if(type =="string"){
		return TEXT;
	}else if(type =="octetArray" or type =="basicList" or type =="subTemplateList" or type=="subTemplateMultiList"){
		return BLOB;
	}
	return UNKNOWN;
}

class element
{
protected:
	//row name for this element
	//combination of id and elterprise nuber
	//exp: e0id16, e20id50....
	int _size;
	enum ibis::TYPE_T _type;
	char _name[IE_NAME_LENGTH];
public:
	void *value;
	element(): _size(0), value(0) {sprintf(_name,"e0id0");};
	element(int size, int en, int id){
		_size = size;
		sprintf( _name,"e%iid%hi", en, id);
	}
	char *name() {return _name;}
	void name(int en, int id){sprintf( _name,"e%iid%hi", en, id);}
	void size( int size) {_size = size;}
	int size(){return _size;}
	void type(enum ibis::TYPE_T type) {_type = type;}
	enum ibis::TYPE_T type() {return _type;}
	virtual int fill(uint8_t * data) {std::cout<<"STORE_DEF"<<std::endl;return 0;}
	void byte_reorder(uint8_t *dst,uint8_t *src, int size, int offset=0);
};

void element::byte_reorder(uint8_t *dst,uint8_t *src, int size, int offset){
	int i;
	for(i=0;i<size;i++){
		dst[i+offset] = src[size-i-1];
	}
}

typedef union float_union 
{
	float float32;
	double float64;
}float_u;

class el_float : public element
{
public:
	float_u float_value;
	el_float(int size = 1, int en = 0, int id = 0){
		_size = size;
		sprintf( _name,"e%uid%hu", en, id);
		this->set_type();
	}
	virtual int fill(uint8_t * data){
		switch(_size){
		case 4:
			//flat32
			byte_reorder((uint8_t *) &(float_value.float32),data,_size);
			value = &(float_value.float32);
			break;
		case 8:
			//float64
			byte_reorder((uint8_t *) &(float_value.float64),data,_size);
			value = &(float_value.float64);
			break;
		default:
			std::cerr << "Wrong float size!" << std::endl;
			break;
		}
		return 0;
	}

	virtual int set_type(){
		switch(_size){
		case 4:
			//flat32
			_type=ibis::FLOAT;
			break;
		case 8:
			//float64
			_type=ibis::DOUBLE;
			break;
		default:
			std::cerr << "Wrong float size!" << std::endl;
			break;
		}
		return 0;
	}
};

/* TODO solve Variable-Length Information Elements
class el_text : public element
{
public:
	char *text_value;
	el_ipv6(int size = 1, int en = 0, int id = 0,  int part = 0){
		_size = size;
		text_value=NULL;
		sprintf( _name,"e%uid%hup%u", en, id, part);
		this->set_type();
	}
	virtual int fill(uint8_t * data){
		//ulong
		byte_reorder((uint8_t *) &(ipv6_value),data,_size, 0);
		value = text_value;
		//std::cout << "FILLED_IPV6" << std::endl;
		return 0;
	}

	virtual int set_type(){
		//ulong
		_type=ibis::ULONG;
		return 0;
	}
};
*/

class el_ipv6 : public element
{
public:
	uint64_t ipv6_value;
	el_ipv6(int size = 1, int en = 0, int id = 0,  int part = 0){
		_size = size;
		sprintf( _name,"e%uid%hup%u", en, id, part);
		this->set_type();
	}
	virtual int fill(uint8_t * data){
		//ulong
		byte_reorder((uint8_t *) &(ipv6_value),data,_size, 0);
		value = &(ipv6_value);
		//std::cout << "FILLED_IPV6" << std::endl;
		return 0;
	}

	virtual int set_type(){
		//ulong
		_type=ibis::ULONG;
		return 0;
	}
};

typedef union uinteger_union 
{
	uint8_t ubyte;
	uint16_t ushort;
	uint32_t uint;
	uint64_t ulong;
}uint_u;

class el_uint : public element
{
public:
	uint_u uint_value;
	el_uint(int size = 1, int en = 0, int id = 0){
		_size = size;
		sprintf( _name,"e%iid%hi", en, id);
		this->set_type();
	}
	virtual int fill(uint8_t * data){
		int offset = 0;
		switch(_size){
		case 1:
			//ubyte
			uint_value.ubyte = data[0];
			value = &(uint_value.ubyte);
			break;
		case 2:
			//ushort
			byte_reorder((uint8_t *) &(uint_value.ushort),data,_size);
			value = &(uint_value.ubyte);
			break;
		case 3:
			offset++;
		case 4:
			//uint
			byte_reorder((uint8_t *) &(uint_value.uint),data,_size, offset);
			//std::cout << _name << " v: " << uint_value.uint << "|" << *((uint32_t *)data) << std::endl;
			value = &(uint_value.ubyte);
			break;
		case 6: //mec addres
			offset++;
		case 7:
			offset++;
		case 8:
			//ulong
			byte_reorder((uint8_t *) &(uint_value.uint),data,_size, offset);
			value = &(uint_value.ubyte);
			break;
		default:
			return 1;
			std::cerr << "Too large uint element!" << std::endl;
			break;
		}
		return 0;
	}
	virtual int set_type(){
		switch(_size){
		case 1:
			//ubyte
			_type=ibis::UBYTE;
			break;
		case 2:
			//ushort
			_type=ibis::USHORT;
			break;
		case 3:
		case 4:
			//uint
			_type=ibis::UINT;
			break;
		case 6: //mec addres
		case 7:
		case 8:
			//ulong
			_type=ibis::ULONG;
			break;
		default:
			return 1;
			std::cerr << "Too large uint element!" << std::endl;
			break;
		}
		return 0;
	}
};


class el_sint : public el_uint
{
public:
	el_sint(int size = 1, int en = 0, int id = 0){
		_size = size;
		sprintf( _name,"e%iid%hi", en, id);
		this->set_type();
	}
	virtual int set_type(){
		switch(_size){
		case 1:
			//ubyte
			_type=ibis::BYTE;
			break;
		case 2:
			//ushort
			_type=ibis::SHORT;
			break;
		case 3:
		case 4:
			//uint
			_type=ibis::INT;
			break;
		case 6: //mec addres
		case 7:
		case 8:
			//ulong
			_type=ibis::LONG;
			break;
		default:
			return 1;
			std::cerr << "Too large uint element!" << std::endl;
			break;
		}
		return 0;
	}
};

class template_table 
{
private:
	uint64_t _rows_count;
	uint16_t _template_id;
	int _record_size;
	ibis::tablex * _tablex;
	char _name[10];
	std::string _dir; // it must end with /
	std::string _path;
	char _index;
public:
	std::vector<element *> elements;	
	std::vector<element *>::iterator el_it;	

	template_table(int template_id): _rows_count(0) 
	{
	_template_id = template_id;
	sprintf(_name,"%u",template_id);
	_tablex = ibis::tablex::create();
	_path = "";
	_dir = "";
	_index=0;
	}
	void dir(std::string dir) {_dir=dir; _path= _dir +_name;}
	int rows() {return _rows_count;}
	void rows(int rows_count) {_rows_count = rows_count;}
	int parse_template(struct ipfix_template * tmp);	
	int store(ipfix_data_set * data_set, std::string path);
	void flush(std::string path){ 
		_tablex->write((path + _name).c_str(),_name, "Generated by ipfixcol fastbit plugin", &_index);
		_tablex->clearData();
		_rows_count = 0;
	}
	~template_table();
};

template_table::~template_table(){
	for (el_it = elements.begin(); el_it!=elements.end(); ++el_it) {
		delete (*el_it);
	}

	delete _tablex;
}

int template_table::store(ipfix_data_set * data_set, std::string path){
	uint8_t *data = data_set->records;
	int ri;
	if (data == NULL){
		return 0;
	}

	//how many records is in  data_set?
	int record_count = (ntohs(data_set->header.length)-(sizeof(struct ipfix_set_header)))/_record_size; 

        for(ri=0;ri<record_count;ri++){
		for (el_it = elements.begin(); el_it!=elements.end(); ++el_it) {
			//CHECK DATA SIZE?!?
			(*el_it)->fill(data);
			_tablex->append((*el_it)->name(), _rows_count, _rows_count+1, (*el_it)->value);
			data += (*el_it)->size();
		}
		_rows_count++;
		if(_rows_count >= RESERVED_SPACE){
			_tablex->write((path + _name).c_str(),_name, "Generated by ipfixcol fastbit plugin", &_index);
			_tablex->clearData();
			std::cout << "WRITEN " << _name << " rows: " << _rows_count << std::endl;
			_rows_count = 0;
		}
	}
	return ri; // TODO FIX!
}

int template_table::parse_template(struct ipfix_template * tmp){
	int i;
	int en = 0; // enterprise number (0 = IANA elements)
	template_ie *field;
	element *new_element;
	//Is there anything to parse?
	if(tmp == NULL){
		return 1;
	}
	//we dont want to parse option tables ect. so check it!
	if(tmp->template_type != TM_TEMPLATE){
		return 1; 
	}
	_template_id = tmp->template_id;
	_record_size = tmp->data_length;

	//Find elements
	for(i=0;i<tmp->field_count;i++){
		field = &(tmp->fields[i]);
		
		//Is this an enterprise element?
		if(field->ie.id & 0x8000){
			i++;
			en = tmp->fields[i].enterprise_number;
		}
		switch(get_type_from_xml(en, field->ie.id & 0x7FFF)){
			case UINT:
				//std::cout << "UINT!" << std::endl;
				new_element = new el_uint(field->ie.length, en, field->ie.id & 0x7FFF);
				_tablex->addColumn(new_element->name(), new_element->type());
				break;
			case IPv6:
				//std::cout << "IPv6!" << std::endl;
				new_element = new el_ipv6(sizeof(uint64_t), en, field->ie.id & 0x7FFF, 1);
				_tablex->addColumn(new_element->name(), new_element->type());
				elements.push_back(new_element);

				new_element = new el_ipv6(sizeof(uint64_t), en, field->ie.id & 0x7FFF, 0);
				_tablex->addColumn(new_element->name(), new_element->type());
				break;
			case INT: 
				new_element = new el_sint(field->ie.length, en, field->ie.id & 0x7FFF);
				_tablex->addColumn(new_element->name(), new_element->type());
				break;
			case FLOAT:
				new_element = new el_float(field->ie.length, en, field->ie.id & 0x7FFF);
				_tablex->addColumn(new_element->name(), new_element->type());
				break;
			case BLOB:
			case TEXT:
			case UNKNOWN:
			default:
				std::cout << "UNKNOWN!" << std::endl;
				new_element = new element(field->ie.length, en, field->ie.id & 0x7FFF);

		}
		//element *new_element;
		if(!new_element){
			std::cerr << "Something is wrong with template elements!" << std::endl;
			return 1;
		}
		elements.push_back(new_element);
		//std::cout << "new_elemet pushed!: " << new_element->name() << std::endl;
	}
	_tablex->reserveSpace(RESERVED_SPACE);
	return 0;
}

enum name_type{TIME,RECORDS};

struct fastbit_config{
	std::map<uint16_t,template_table*> *templates;
	int time_window;	
	int records_window;
	enum name_type dump_name;
	std::string sys_dir;
	std::string window_dir;
	time_t last_flush;
};

extern "C"
int storage_init (char *params, void **config){
	std::cerr <<"INIT" << std::endl;
	struct tm * timeinfo;
	char formated_time[15];
	(*config) = (struct fastbit_config *)malloc(sizeof( struct fastbit_config));
	(*config) = new  struct fastbit_config;
	((struct fastbit_config*)(*config))->templates = new std::map<uint16_t,template_table*>;
	((struct fastbit_config*)(*config))->time_window = 300;
	((struct fastbit_config*)(*config))->records_window = 1000000;

	//struct fastbit_config*)(*config))->dump_name = RECORDS;
	((struct fastbit_config*)(*config))->dump_name = TIME;

	((struct fastbit_config*)(*config))->sys_dir = "sys_dir/";

	if(((struct fastbit_config*)(*config))->dump_name != TIME){
		((struct fastbit_config*)(*config))->window_dir = "0/";
	} else {
		time ( &(((struct fastbit_config*)(*config))->last_flush));
		timeinfo = localtime ( &(((struct fastbit_config*)(*config))->last_flush) );

		strftime(formated_time,15,"%Y%m%d%H%M",timeinfo);
		((struct fastbit_config*)(*config))->window_dir = std::string(formated_time) + "/";
	}
	return 0;
}


extern "C"
int store_packet (void *config, const struct ipfix_message *ipfix_msg,
	const struct ipfix_template_mgr *template_mgr){
	std::map<uint16_t,template_table*>::iterator table;
	struct fastbit_config *conf = (struct fastbit_config *) config;
	std::map<uint16_t,template_table*> *templates = conf->templates;
	static int rcnt = 0;
	static int flushed = 0;
	std::stringstream ss;
	time_t rawtime;
	struct tm * timeinfo;
	char formated_time[15];

	int i;
	int flush=0;

	for(i = 0 ; i < 1023; i++){ //TODO magic number! add constant to storage.h 
		if(ipfix_msg->data_set[i].data_set == NULL){
			//there are no more filled data_sets	
			return 0;
		}

	
		if(ipfix_msg->data_set[i].tmplate == NULL){
			//skip data without tamplate!
			continue;
		}

		uint16_t template_id = ipfix_msg->data_set[i].tmplate->template_id;


		if((table = templates->find(template_id)) == templates->end()){
			//NEW TEMPLATE!		
			std::cout << "NEW TEMPLATE: " << template_id << std::endl;
			template_table *table_tmp = new template_table(template_id); // TODO adr prefix!
			table_tmp->parse_template(ipfix_msg->data_set[i].tmplate);
			templates->insert(std::pair<uint16_t,template_table*>(template_id,table_tmp));
			table = templates->find(template_id);
		} else {
			//TEMPLATE IS KNOWN!
			//std::cout << "TEMPLATE IS KNOWN: "<< template_id << std::endl;
		}

		rcnt += (*table).second->store(ipfix_msg->data_set[i].data_set, conf->sys_dir + conf->window_dir);

		//TODO check elapsed time!
		
		//should we create new window?
		if(rcnt > conf->records_window && conf->records_window !=0){
			flush = 1;
		}
		if(conf->time_window !=0){
			time ( &rawtime );
			if(difftime(rawtime,conf->last_flush) > conf->time_window){
				flush=1;
			}
		}

		if(flush){
			flushed ++;
			time ( &(conf->last_flush));
			

			//flush all templates!
			for(table = templates->begin(); table!=templates->end();table++){
				(*table).second->flush(conf->sys_dir + conf->window_dir);
			}
			//change window directory name!
			if (conf->dump_name == RECORDS){
				ss << flushed;
				conf->window_dir = ss.str() + "/";
			}else{
				timeinfo = localtime ( &(conf->last_flush));

				strftime(formated_time,15,"%Y%m%d%H%M",timeinfo);
				conf->window_dir = std::string(formated_time) + "/";
			}
			rcnt = 0;
		}
	}
	return 0;
}

extern "C"
int store_now (const void *config){
	std::cout <<"STORE_NOW" << std::endl;
	return 0;
}

extern "C"
int storage_close (void **config){
	std::cerr <<"CLOSE" << std::endl;
	std::map<uint16_t,template_table*>::iterator table;
	struct fastbit_config *conf = (struct fastbit_config *) (*config);
	std::map<uint16_t,template_table*> *templates = conf->templates;

	for(table = templates->begin(); table!=templates->end();table++){
		(*table).second->flush(conf->sys_dir + conf->window_dir);
		delete (*table).second;
	}
	
	delete templates;
	return 0;
}

