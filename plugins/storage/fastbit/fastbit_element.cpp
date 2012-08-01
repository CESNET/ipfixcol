/**
 * \file fastbit_element.cpp
 * \author Petr Kramolis <kramolis@cesnet.cz>
 * \brief methods of object wrapers for information elements.
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


#include "fastbit_element.h"
#include "fastbit_table.h"


int load_types_from_xml(struct fastbit_config *conf){
	pugi::xml_document doc;
	uint32_t en;
	uint16_t id;
	enum store_type type;
	std::string str_value;

	if (!doc.load_file("/etc/ipfixcol/ipfix-elements.xml")) return -1;


	pugi::xpath_node_set elements = doc.select_nodes("/ipfix-elements/element");
	for (pugi::xpath_node_set::const_iterator it = elements.begin(); it != elements.end(); ++it)
	{
		pugi::xpath_node node = *it;

		str_value = it->node().child_value("enterprise");
		en = strtoul(str_value.c_str(),NULL,0);
		str_value = it->node().child_value("id");
		id = strtoul(str_value.c_str(),NULL,0);

		str_value = it->node().child_value("dataType");

		if(str_value =="unsigned8" or  str_value =="unsigned16" or str_value =="unsigned32" or str_value =="unsigned64" or \
		   str_value =="dateTimeSeconds" or str_value =="dateTimeMilliseconds" or str_value =="dateTimeMicroseconds" or \
	           str_value =="dateTimeNanoseconds" or str_value =="ipv4Address" or str_value =="macAddress" or str_value == "boolean"){
			type =UINT;
		}else if(str_value =="signed8" or str_value =="signed16" or str_value =="signed32" or str_value =="signed64" ){
			type = INT;
		}else if(str_value =="ipv6Address"){
			type = IPv6;
		}else if(str_value =="float32" or str_value =="float64"){
			type = FLOAT;
		}else if(str_value =="string"){
			type = TEXT;
		}else if(str_value =="octetArray" or str_value =="basicList" or str_value =="subTemplateList" or str_value=="subTemplateMultiList"){
			type = BLOB;
		}else{
			type = UNKNOWN;
		}
		//conf->elements_types->insert(std::make_pair(en , std::make_pair(id, type)));
		(*conf->elements_types)[en][id] = type;
		//std::cout << "el loaded: " << en << ":" << id <<":"<< type << std::endl;
	}

	return 0;
}


/*
 * \brief Search elements xml for type of element
 *
 * This function reads /etc/ipfixcol/ipfix-elements.xml (TODO add this as parameter)
 * and search element specified by element id and enterprise id.
 * When its found it decides if its integer, text, float etc..
 * (it does not provide exact type (size) as uint32_t,double ect.)
 *
 * @param data point
* @param en Enterprise number of element
* @param id ID of information element
 */
enum store_type get_type_from_xml(struct fastbit_config *conf, unsigned int en, unsigned int id){
	return (*conf->elements_types)[en][id];
}

void element::byte_reorder(uint8_t *dst,uint8_t *src, int size, int offset){
	int i;
	for(i=0;i<size;i++){
		dst[i+offset] = src[size-i-1];
	}
}

int el_float::fill(uint8_t * data){
	switch(_size){
	case 4:
		//flat32
		byte_reorder((uint8_t *) &(float_value.float32),data,_size);
		value = &(float_value.float32);
		this->append(&(float_value.float32));
		break;
	case 8:
		//float64
		byte_reorder((uint8_t *) &(float_value.float64),data,_size);
		value = &(float_value.float64);
		this->append(&(float_value.float64));
		break;
	default:
		std::cerr << "Wrong float size!" << std::endl;
		break;
	}
	return 0;
}

int el_float::set_type(){
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

int el_ipv6::fill(uint8_t * data){
	//ulong
	byte_reorder((uint8_t *) &(ipv6_value),data,_size, 0);
	value = &(ipv6_value);
	this->append(&(ipv6_value));
	return 0;
}

int el_ipv6::set_type(){
	//ulong
	_type=ibis::ULONG;
	return 0;
}

int el_text::fill(uint8_t * data){
	_offset = 0;
	//get size of data
	if(_var_size){
		if(data[0] < 255){
			_true_size = data[0];
			_offset = 1;
		}else{
			byte_reorder((uint8_t *) &(_true_size),&(data[1]),2);
			_offset = 3;
		}
	}

	this-> append_str(&(data[_offset]),_true_size);
	return 0;
}

int el_var_size::fill(uint8_t * data){
	//get size of data
	if(data[0] < 255){
		_size = data[0] + 1; //1 is firs byte with true size
	}else{
		byte_reorder((uint8_t *) &(_size),&(data[1]),2);
		_size+=3; //3 = 1 first byte with 256 and 2 bytes with true size
	}
	std::cout << "variable(" << _name << ") whole element size:" << _size << std::endl;
	return 0;
}
int el_var_size::set_type(){
	_type=ibis::UBYTE;
	return 0;
}

int el_blob::fill(uint8_t * data){
	//get size of data
	return 0;
}
int el_blob::set_type(){
	_type=ibis::UBYTE;
	return 0;
}

int el_uint::fill(uint8_t * data){
	int offset = 0;
	switch(_size){
	case 1:
		//ubyte
		uint_value.ubyte = data[0];
		value = &(uint_value.ubyte);
		this->append(&(uint_value.ubyte));
		break;
	case 2:
		//ushort
		byte_reorder((uint8_t *) &(uint_value.ushort),data,_size);
		value = &(uint_value.ubyte);
		this->append(&(uint_value.ushort));
		break;
	case 3:
		offset++;
	case 4:
		//uint
		byte_reorder((uint8_t *) &(uint_value.uint),data,_size, offset);
		//std::cout << _name << " v: " << uint_value.uint << "|" << *((uint32_t *)data) << std::endl;
		value = &(uint_value.ubyte);
		this->append(&(uint_value.uint));
		break;
	case 6: //mec addres
		offset++;
	case 7:
		offset++;
	case 8:
		//ulong
		byte_reorder((uint8_t *) &(uint_value.ulong),data,_size, offset);
		value = &(uint_value.ubyte);
		this->append(&(uint_value.ulong));
		break;
	default:
		return 1;
		std::cerr << "Too large uint element!" << std::endl;
		break;
	}
	return 0;
}

int el_uint::set_type(){
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


int el_sint::set_type(){
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

