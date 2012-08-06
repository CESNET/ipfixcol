/**
 * \file fastbit_elemetn.h
 * \author Petr Kramolis <kramolis@cesnet.cz>
 * \brief object wrapers for information elements.
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

#ifndef FASTBIT_ELEMENT
#define FASTBIT_ELEMENT

extern "C" {
#include <ipfixcol/storage.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
}

#include <map>
#include <iostream>
#include <string>

#include <fastbit/ibis.h>

#include "pugixml.hpp"

#include "fastbit.h"
#include "config_struct.h"




const int IE_NAME_LENGTH = 16;
const int TYPE_NAME_LENGTH = 10;
const char ELEMENTS_XML[] = "/etc/ipfixcol/ipfix-elements.xml";


class template_table; //Needed becouse of Circular dependency

/**
 * \brief Load elements types from xml to configure structure
 *
 * This function reads /etc/ipfixcol/ipfix-elements.xml
 * and stores elements data type in configuration structure
 *
 * @param conf fastbit storage plug-in configuration structure
 */
int load_types_from_xml(struct fastbit_config *conf);

/**
 * \brief Search for element type in configure structure
 *
 * @param conf fastbit storage plug-in configuration structure
 * @param en Enterprise number of element
 * @param id ID of information element
 * @return element type
 */
enum store_type get_type_from_xml(struct fastbit_config *conf,unsigned int en,unsigned int id);

/**
 * \brief Class wrapper for information elements
 */
class element
{
protected:
	int _size;
	enum ibis::TYPE_T _type;
	/* row name for this element
	   combination of id and enterprise number
	   exp: e0id16, e20id50.... */
	char _name[IE_NAME_LENGTH];

	int _filled;  /* number of items are stored in buffer*/
	int _buf_max; /* maximum number of items buffer can hold */
	char *_buffer; /* items buffer */

public:
	/* points to elements data after fill() */
	void *value;
	element();
	element(int size, int en, int id, uint32_t buf_size = RESERVED_SPACE);

	/* get & set methods*/
	/**
	 * \brief Get method for element name
	 *
	 * @return Null terminated string with element name
	 */
	char *name() {return _name;}

	/**
	 * \brief Set element name
	 *
	 * Element name is based on enterprise id and element id.
	 * (e.g e0id5, e5id10 )
	 *
	 * @param en Enterprise number of element
	 * @param id ID of information element
	 */
	void name(int en, int id){sprintf( _name,"e%iid%hi", en, id);}

	/**
	 * \brief Get method for element size
	 *
	 * @return element size
	 */
	virtual int size(){return _size;}

	/**
	 * \brief Set element size
	 *
	 * Element size is used for parsing data records
	 * so it must be set properly.
	 *
	 * @param size element size
	 */
	void size( int size) {_size = size;}

	/**
	 * \brief Set element type
	 *
	 * This sets which fastbit data type
	 * is used for this element when its
	 * stored to fastbit data base.
	 *
	 * @param type value of one fastbit data type for enum ibis::TYPE_T
	 */
	void type(enum ibis::TYPE_T type) {_type = type;}
	enum ibis::TYPE_T type() {return _type;}

	/* core methods */
	/**
	 * \brief fill internal element value according to given data
	 *
	 * This method transforms data from ipfix record to internal
	 * value usable by fastbit. Number of bytes to read is specified by _size
	 * variable. This method converts endianity. Data can be accesed by value pointer.
	 *
	 * @param data pointer to input data (usualy ipfix element)
	 * @return 0 on succes
	 * @return 1 on failure
	 */
	virtual int fill(uint8_t * data) {return 0;}

	/**
	 * \brief Copy data and change byte order
	 *
	 * This method simply copy data from src to dst
	 * in oposite byte order.
	 *
	 *
	 * @param dst pointer to destination memory where data should be writen (the memory MUST be preallocated)
	 * @param src pointer to source data for byte reorder
	 * @param size size of memory to copy & reorder from source data
	 * @param offset offset for destination memory (useful when reordering int16_t to int32_t etc)
	 */
	void byte_reorder(uint8_t *dst,uint8_t *src, int size, int offset=0);

	/**
	 * \brief allocate memory for buffer
	 */
	void allocate_buffer(int count);

	/**
	 * \brief free memory for buffer
	 */
	void free_buffer();
	int append(void *data);
	int append_str(void *data,int size);

	int flush(std::string path);

	std::string get_part_info();
};

class el_var_size : public element
{
public:
	void *data;
	el_var_size(int size = 0, int en = 0, int id = 0, uint32_t buf_size = RESERVED_SPACE);
	/* core methods */
	/**
	 * \brief fill internal element value according to given data
	 *
	 * This method transforms data from ipfix record to internal
	 * value usable by fastbit. Number of bytes to read is specified by _size
	 * variable. This method converts endianity. Data can be accesed by value pointer.
	 *
	 * @param data pointer to input data (usualy ipfix element)
	 * @return 0 on succes
	 * @return 1 on failure
	 */
	virtual int fill(uint8_t * data);
	virtual int set_type();
};


typedef union float_union
{
	float float32;
	double float64;
}float_u;

class el_float : public element
{
public:
	float_u float_value;
	el_float(int size = 1, int en = 0, int id = 0, uint32_t buf_size = RESERVED_SPACE);
	/* core methods */
	/**
	 * \brief fill internal element value according to given data
	 *
	 * This method transforms data from ipfix record to internal
	 * value usable by fastbit. Number of bytes to read is specified by _size
	 * variable. This method converts endianity. Data can be accesed by value pointer.
	 *
	 * @param data pointer to input data (usualy ipfix element)
	 * @return 0 on succes 
	 * @return 1 on failure
	 */
	virtual int fill(uint8_t * data);
	virtual int set_type();
};

// TODO solve Variable-Length Information Elements
class el_text : public element
{
private:
	bool _var_size;
	uint16_t _true_size;
	uint8_t _offset;
public:
	//char *text_value;
	el_text(int size = 1, int en = 0, int id = 0, uint32_t buf_size = RESERVED_SPACE);

	virtual int set_type(){
		_type=ibis::TEXT;
		return 0;
	}
	virtual int fill(uint8_t * data);
	virtual int size(){return _true_size + _offset;} //var. size element carries its size in 1-3 bytes (stored in _offset)
};


class el_ipv6 : public element
{
public:
	uint64_t ipv6_value;
	el_ipv6(int size = 1, int en = 0, int id = 0,  int part = 0, uint32_t buf_size = RESERVED_SPACE);
	/* core methods */
	/**
	 * \brief fill internal element value according to given data
	 *
	 * This method transforms data from ipfix record to internal
	 * value usable by fastbit. Number of bytes to read is specified by _size
	 * variable. This method converts endianity. Data can be accesed by value pointer.
	 *
	 * @param data pointer to input data (usualy ipfix element)
	 * @return 0 on succes
	 * @return 1 on failure
	 */
	virtual int fill(uint8_t * data);
	virtual int set_type();
};

class el_blob : public element
{
public:
	uint8_t uint_value;
	el_blob(int size = 1, int en = 0, int id = 0, uint32_t buf_size = RESERVED_SPACE);
	virtual int fill(uint8_t * data);
	virtual int set_type();
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
	el_uint(int size = 1, int en = 0, int id = 0, uint32_t buf_size = RESERVED_SPACE);
	/* core methods */
	/**
	 * \brief fill internal element value according to given data
	 *
	 * This method transforms data from ipfix record to internal
	 * value usable by fastbit. Number of bytes to read is specified by _size
	 * variable. This method converts endianity. Data can be accesed by value pointer.
	 *
	 * @param data pointer to input data (usualy ipfix element)
	 * @return 0 on succes
	 * @return 1 on failure
	 */
	virtual int fill(uint8_t * data);
	virtual int set_type();
};




class el_sint : public el_uint
{
public:
	el_sint(int size = 1, int en = 0, int id = 0, uint32_t buf_size = RESERVED_SPACE);
	virtual int set_type();
};
#endif
