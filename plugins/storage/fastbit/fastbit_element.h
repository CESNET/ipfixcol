/**
 * \file fastbit_elemetn.h
 * \author Petr Kramolis <kramolis@cesnet.cz>
 * \brief object wrapers for information elements.
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

#ifndef FASTBIT_ELEMENT
#define FASTBIT_ELEMENT

extern "C" {
#include <ipfixcol.h>
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

class template_table; //Needed becouse of Circular dependency

/**
 * \brief Load elements types from xml to configure structure
 *
 * This function reads ipfix-elements.xml
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
enum store_type get_type_from_xml(struct fastbit_config *conf, uint32_t en, uint16_t id);

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

	uint32_t _filled;  /* number of items are stored in buffer */
	uint32_t _buf_max; /* maximum number of items buffer can hold */
	char *_buffer; /* items buffer */

	/**
	 * \brief Get method for element size
	 *
	 * @return element size
	 */
	int size() { return _size; }

	/**
	 * \brief Set element name
	 *
	 * Element name is based on enterprise id and element id.
	 * (e.g e0id5, e5id10 )
	 *
	 * @param en Enterprise number of element
	 * @param id ID of information element
	 * @param part Number of part (used for IPv6)
	 */
	void setName(uint32_t en, uint16_t id, int part = -1);

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
	void byte_reorder(uint8_t *dst, uint8_t *src, int size, int offset = 0);

	/**
	 * \brief allocate memory for buffer
	 *
	 * @param count Number of elements that the buffer should hold
	 */
	void allocate_buffer(uint32_t count);

	/**
	 * \brief free memory for buffer
	 */
	virtual void free_buffer();

	/**
	 * \brief Append data to the buffer
	 * @param data
	 * @return 0 on success, 1 when buffer is full
	 */
	int append(void *data);

public:
	virtual ~element() { free_buffer(); };

	/* core methods */
	/**
	 * \brief fill internal element value according to given data
	 *
	 * This method transforms data from ipfix record to internal
	 * value usable by fastbit. Number of bytes to read is specified by _size
	 * variable. This method converts endianity. Data can be accesed by value pointer.
	 *
	 * @param data pointer to input data (usualy ipfix element)
	 * @return size of the element read from the data
	 */
	virtual uint16_t fill(uint8_t *data) = 0;

	/**
	 * \brief Flush buffer content to file
	 *
	 * @param path of the file to write to
	 * @return 0 on success, 1 otherwise
	 */
	virtual int flush(std::string path);

	/**
	 * \brief Return string with par information for -part.txt FastBit file
	 *
	 * @return string with part information
	 */
	virtual std::string get_part_info();
};

class el_var_size : public element
{
public:
	void *data;
	el_var_size(int size = 0, uint32_t en = 0, uint16_t id = 0, uint32_t buf_size = RESERVED_SPACE);
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
	virtual uint16_t fill(uint8_t *data);

protected:
	int set_type();
};

typedef union float_union
{
	float float32;
	double float64;
} float_u;

class el_float : public element
{
public:
	float_u float_value;
	el_float(int size = 1, uint32_t en = 0, uint16_t id = 0, uint32_t buf_size = RESERVED_SPACE);
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
	virtual uint16_t fill(uint8_t *data);

protected:
	int set_type();
};

class el_text : public element
{
private:
	bool _var_size;
	uint16_t _true_size;
	uint8_t _offset;
public:
	el_text(int size = 1, uint32_t en = 0, uint16_t id = 0, uint32_t buf_size = RESERVED_SPACE);

	virtual uint16_t fill(uint8_t *data);

protected:
	int set_type() {
		_type = ibis::TEXT;
		return 0;
	}

	/**
	 * \brief Append string to buffer
	 * @param data Data to append
	 * @param size size of data to append
	 * @return 0 on success, 1 otherwise
	 */
	int append_str(void *data, int size);
};

class el_ipv6 : public element
{
public:
	uint64_t ipv6_value;
	el_ipv6(int size = 1, uint32_t en = 0, uint16_t id = 0, int part = 0, uint32_t buf_size = RESERVED_SPACE);
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
	virtual uint16_t fill(uint8_t *data);

protected:
	int set_type();
};

class el_blob : public element
{
public:
	el_blob(int size = 1, uint32_t en = 0, uint16_t id = 0, uint32_t buf_size = RESERVED_SPACE);
	virtual uint16_t fill(uint8_t *data);
	virtual ~el_blob();

	/**
	 * \brief Overloaded flush function to write the sp buffer
	 *
	 * Calls parent funtion flush
	 *
	 * @param path to write the file to
	 * @return 0 on success, 1 otherwise
	 */
	virtual int flush(std::string path);

protected:
	bool _var_size;
	uint16_t _true_size;
	uint8_t uint_value;

	char *_sp_buffer;
	uint32_t _sp_buffer_size;
	uint32_t _sp_buffer_offset;
	
	int set_type(){
		_type = ibis::BLOB;
		return 0;
	}
};

typedef union uinteger_union
{
	uint8_t ubyte;
	uint16_t ushort;
	uint32_t uint;
	uint64_t ulong;
} uint_u;

class el_uint : public element
{
public:
	el_uint(int size = 1, uint32_t en = 0, uint16_t id = 0, uint32_t buf_size = RESERVED_SPACE);
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
	virtual uint16_t fill(uint8_t *data);

protected:
	uint_u uint_value;
	uint16_t _real_size; /**< Element Size that was sent (can differ from storage _size) */

	int set_type();
};

class el_sint : public el_uint
{
public:
	el_sint(int size = 1, uint32_t en = 0, uint16_t id = 0, uint32_t buf_size = RESERVED_SPACE);

protected:
	int set_type();
};

class el_unknown : public element
{
protected:
	bool _var_size;

	/**
	 * \brief allocate memory for buffer
	 *
	 * @param count Number of elements that the buffer should hold
	 */
	void allocate_buffer(uint32_t count);

	/**
	 * \brief free memory for buffer
	 */
	virtual void free_buffer();

	/**
	 * \brief Append data to the buffer
	 * @param data
	 * @return 0 on success, 1 when buffer is full
	 */
	int append(void *data);

public:
	el_unknown(int size = 0, uint32_t en = 0, uint16_t id = 0, int part = 0, uint32_t buf_size = 0);

	/* core methods */
	/**
	 * \brief fill internal element value according to given data
	 *
	 * This method transforms data from ipfix record to internal
	 * value usable by fastbit. Number of bytes to read is specified by _size
	 * variable. This method converts endianity. Data can be accesed by value pointer.
	 *
	 * @param data pointer to input data (usualy ipfix element)
	 * @return size of the element read from the data
	 */
	virtual uint16_t fill(uint8_t *data);

	/**
	 * \brief Flush buffer content to file
	 *
	 * @param path of the file to write to
	 * @return 0 on success, 1 otherwise
	 */
	virtual int flush(std::string path);

	/**
	 * \brief Return string with par information for -part.txt FastBit file
	 *
	 * @return string with part information
	 */
	std::string get_part_info();
};

#endif
