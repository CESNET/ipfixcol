/**
 * \file fastbit_element.cpp
 * \author Petr Kramolis <kramolis@cesnet.cz>
 * \brief methods of object wrapers for information elements.
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
#include <endian.h>
}

#include "fastbit_element.h"
#include "fastbit_table.h"

#define NFv9_CONVERSION_ENTERPRISE_NUMBER (~((uint32_t) 0))

int load_types_from_xml(struct fastbit_config *conf)
{
	pugi::xml_document doc;
	pugi::xml_parse_result result;
	uint32_t en;
	uint16_t id;
	enum store_type type;
	std::string str_value;

	result = doc.load_file(ipfix_elements);

	/* Check for errors */
	if (!result) {
		MSG_ERROR(msg_module, "Error while parsing '%s': %s", ipfix_elements, result.description());
		return -1;
	}

	pugi::xpath_node_set elements = doc.select_nodes("/ipfix-elements/element");
	for (pugi::xpath_node_set::const_iterator it = elements.begin(); it != elements.end(); ++it)
	{
		str_value = it->node().child_value("enterprise");
		en = strtoul(str_value.c_str(), NULL, 0);
		str_value = it->node().child_value("id");
		id = strtoul(str_value.c_str(), NULL, 0);

		str_value = it->node().child_value("dataType");

		if (str_value == "unsigned8" or str_value == "unsigned16" or str_value == "unsigned32" or str_value == "unsigned64" or \
				str_value == "dateTimeSeconds" or str_value == "dateTimeMilliseconds" or str_value == "dateTimeMicroseconds" or \
				str_value == "dateTimeNanoseconds" or str_value == "ipv4Address" or str_value == "macAddress" or str_value == "boolean") {
			type = UINT;
		} else if (str_value == "signed8" or str_value == "signed16" or str_value == "signed32" or str_value == "signed64") {
			type = INT;
		} else if (str_value == "ipv6Address") {
			type = IPv6;
		} else if (str_value == "float32" or str_value == "float64") {
			type = FLOAT;
		} else if (str_value == "string") {
			type = TEXT;
		} else if (str_value == "octetArray" or str_value == "basicList" or str_value == "subTemplateList" or str_value== "subTemplateMultiList") {
			type = BLOB;
		} else {
			type = UNKNOWN;
		}

		(*conf->elements_types)[en][id] = type;
	}

	return 0;
}

enum store_type get_type_from_xml(struct fastbit_config *conf, uint32_t en, uint16_t id)
{
	/* Check whether a type has been determined for the specified element */
	if ((*conf->elements_types).count(en) == 0 || (*conf->elements_types)[en].count(id) == 0) {
		if (en == NFv9_CONVERSION_ENTERPRISE_NUMBER) {
			MSG_WARNING(msg_module, "No specification for e%uid%u found in '%s' (enterprise number converted from NFv9)", en, id, ipfix_elements);
		} else {
			MSG_WARNING(msg_module, "No specification for e%uid%u found in '%s'", en, id, ipfix_elements);
		}

		return UNKNOWN;
	}

	return (*conf->elements_types)[en][id];
}

void element::byte_reorder(uint8_t *dst, uint8_t *src, int srcSize, int dstSize)
{
	(void) dstSize;
	int i;
	for (i = 0;i < srcSize; i++) {
		dst[i] = src[srcSize - i - 1];
	}
}

void element::setName(uint32_t en, uint16_t id, int part)
{
	if (part == -1) { /* default */
		sprintf(_name, "e%iid%hi", en, id);
	} else {
		sprintf(_name, "e%iid%hip%i", en, id, part);
	}
}

void element::allocate_buffer(uint32_t count)
{
	_buf_max = count;
	_buffer = (char *) realloc(_buffer, _size * count);
	if (_buffer == NULL) {
		MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)", __FILE__, __LINE__);
	}
}

void element::free_buffer()
{
	free(_buffer);
}

int element::append(void *data)
{
	if (_filled >= _buf_max) {
		return 1;
	}

	memcpy(&(_buffer[size()*_filled]), data, size());
	_filled++;
	return 0;
}

int element::flush(std::string path)
{
	FILE *f;
	size_t check;
	if (_filled > 0) {
		f = fopen((path + "/" + _name).c_str(), "a+");
		if (f == NULL) {
			fprintf(stderr, "Error while writing data (fopen)\n");
			return 1;
		}

		if (_buffer == NULL) {
			fprintf(stderr, "Error while writing data (buffer)\n");
			fclose(f);
			return 1;
		}

		check = fwrite(_buffer, size(), _filled, f);
		if (check != (size_t) _filled) {
			fprintf(stderr, "Error while writing data (fwrite)\n");
			fclose(f);
			return 1;
		}

		_filled = 0;
		fclose(f);
	}

	return 0;
}

std::string element::get_part_info()
{
	return std::string("\nBegin Column") + \
		"\nname = " + std::string(this->_name) + \
		"\ndata_type = " + ibis::TYPESTRING[(int) this->_type] + \
		"\nEnd Column\n";
}

el_var_size::el_var_size(int size, uint32_t en, uint16_t id, uint32_t buf_size)
{
	(void) buf_size;
	data = NULL;
	_size = size;
	_filled = 0;
	_buffer = NULL;

	setName(en, id);
	this->set_type();
}

uint16_t el_var_size::fill(uint8_t *data)
{
	/* Get size of data */
	if (data[0] < 255) {
		_size = data[0] + 1; /* 1 is first byte with true size */
	} else if (data[0] == 255) {
		/* Length is stored in second and third octet */
		_size = ntohs(*((uint16_t *) &data[1]));
		_size += 3;
	} else {
		MSG_ERROR(msg_module, "Invalid (variable) length specification: %u", data[0]);
	}

	return _size;
}

int el_var_size::set_type()
{
	_type = ibis::UBYTE;
	return 0;
}

el_float::el_float(int size, uint32_t en, uint16_t id, uint32_t buf_size)
{
	_size = size;
	_filled = 0;
	_buffer = NULL;

	setName(en, id);
	this->set_type();

	if (buf_size == 0) {
		buf_size = RESERVED_SPACE;
	}

	allocate_buffer(buf_size);
}

uint16_t el_float::fill(uint8_t *data)
{
	switch(_size) {
	case 4:
		/* float32 */
		*((uint32_t*)&(float_value.float32)) = ntohl(*((uint32_t*) data));
		this->append(&(float_value.float32));
		break;
	case 8:
		/* float64 */
		*((uint64_t*)&(float_value.float64)) = be64toh(*((uint64_t*) data));
		this->append(&(float_value.float64));
		break;
	default:
		MSG_ERROR(msg_module, "Invalid element size (%s - %u)", _name, _size);
		break;
	}

	return _size;
}

int el_float::set_type()
{
	switch(_size) {
	case 4:
		/* float32 */
		_type = ibis::FLOAT;
		break;
	case 8:
		/* float64 */
		_type = ibis::DOUBLE;
		break;
	default:
		MSG_ERROR(msg_module, "Invalid element size (%s - %u)", _name, _size);
		break;
	}

	return 0;
}

el_text::el_text(int size, uint32_t en, uint16_t id, uint32_t buf_size)
{
	_size = 1; // this is size for flush function
	_true_size = size; // this holds true size of string (var of fix size)
	_var_size = false;
	_offset = 0;
	_filled = 0;
	_buffer = NULL;

	if (size == VAR_IE_LENGTH) { //its element with variable size
		_var_size = true;
	}

	setName(en, id);
	this->set_type();

	if (buf_size == 0) {
		 buf_size = RESERVED_SPACE;
	}

	allocate_buffer(buf_size);
}

int el_text::append_str(void *data, int size)
{
	/* Check buffer space */
	if (_filled + size + 1 >= _buf_max) { // 1 = terminating zero
		_buf_max = _buf_max + (100 * size) + 1; // TODO
		_buffer = (char *) realloc(_buffer, _size * _buf_max);
	}

	memcpy(_buffer + _filled, data, size);

	/* Set terminating character, just to be sure it is there */
	_buffer[_filled + size] = '\0';

	/* Get the real string length (data may contain more '\0' accidentally) */
	_filled += strlen(_buffer + _filled) + 1;

	return 0;
}

uint16_t el_text::fill(uint8_t *data)
{
	/* Get size of data */
	if (_var_size) {
		if (data[0] < 255) {
			_true_size = data[0];
			_offset = 1;
		} else {
			_true_size = ntohs(*((uint16_t *) &data[1]));
			_offset = 3;
		}
	}

	this->append_str(&(data[_offset]), _true_size);
	return _true_size + _offset;
}

el_ipv6::el_ipv6(int size, uint32_t en, uint16_t id, int part, uint32_t buf_size)
{
	ipv6_value = 0;
	_size = size;
	_filled = 0;
	_buffer = NULL;

	setName(en, id, part);
	this->set_type();

	if (buf_size == 0) {
		buf_size = RESERVED_SPACE;
	}

	allocate_buffer(buf_size);
}

uint16_t el_ipv6::fill(uint8_t *data)
{
	/* ulong */
	ipv6_value = be64toh(*((uint64_t*) data));
	this->append(&(ipv6_value));
	return _size;
}

int el_ipv6::set_type()
{
	/* ulong */
	_type = ibis::ULONG;
	return 0;
}

el_blob::el_blob(int size, uint32_t en, uint16_t id, uint32_t buf_size):
	_var_size(false), _true_size(size), _sp_buffer(NULL)
{
	/* Set variables defined in parent */
	_size = 1; /* This is size for flush function */
	_buffer = NULL;
	_filled = 0;
	uint_value = 0;

	if (size == VAR_IE_LENGTH) { /* Element with variable length */
		_var_size = true;
	}

	setName(en, id);
	this->set_type();

	if (buf_size == 0) {
		 buf_size = RESERVED_SPACE;
	}

	allocate_buffer(buf_size);

	/* Allocate the sp buffer */
	_sp_buffer_size = buf_size;
	_sp_buffer = (char *) realloc(_sp_buffer, _sp_buffer_size);
	if (_sp_buffer == NULL) {
		MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)", __FILE__, __LINE__);
		exit(-1);
	}

	/* Fill the offset of first element */
	*(uint64_t *) _sp_buffer = 0;
	_sp_buffer_offset = 8; /* 8 byte numbers are used to record offset */
}

uint16_t el_blob::fill(uint8_t *data)
{
	uint8_t _offset = 0;

	/* Get real size of the data */
	if (_var_size) {
		/* Length is stored in first octet if < 255 */
		if (data[0] < 255) {
			_true_size = data[0];
			_offset = 1;
		} else if (data[0] == 255) {
			/* Length is stored in second and third octet */
			_true_size = ntohs(*((uint16_t *) &data[1]));
			_offset = 3;
		} else {
			MSG_ERROR(msg_module, "Invalid (variable) length specification: %u", data[0]);
		}
	}

	if (_filled + _true_size >= _buf_max) {
		_buf_max += 100 * _true_size; // TODO find some better constant
		_buffer = (char *) realloc(_buffer, _buf_max);
		if (_buffer == NULL) {
			perror("realloc blob buffer");
			exit(-1);
		}
	}

	memcpy(&(_buffer[_filled]), data + _offset, _true_size);

	/* Update the number of filled bytes */
	_filled += _true_size;

	/* Check that the sp buffer is big enough */
	if (_sp_buffer_offset + 8 >= _sp_buffer_size) {
		_sp_buffer_size *= 2; /* Double the buffer */
		_sp_buffer = (char *) realloc(_sp_buffer, _sp_buffer_size);
		if (_sp_buffer == NULL) {
			perror("realloc blob sp buffer");
			exit(-1);
		}
	}

	/* Update the sp buffer */
	*(uint64_t *) (_sp_buffer + _sp_buffer_offset) = (uint64_t) _filled;
	_sp_buffer_offset += 8;

	/* Return size of processed data */
	return _true_size  + _offset;
}

int el_blob::flush(std::string path)
{
	FILE *f;
	size_t check;
	
	if (_filled > 0 && _sp_buffer != NULL) {
		f = fopen((path + "/" + _name + ".sp").c_str(), "a+");
		if (f == NULL) {
			MSG_ERROR(msg_module, "Error while writing data (fopen)");
			return 1;
		}

		check = fwrite(_sp_buffer, 1 , _sp_buffer_offset, f);
		if (check != (size_t) _sp_buffer_offset) {
			MSG_ERROR(msg_module, "Error while writing data (fwrite)");
			fclose(f);
			return 1;
		}

		/* Close the file */
		fclose(f);

		/* Reset buffer */
		_sp_buffer_offset = 8;

		/* TODO
		 * It is necessary to get the offset right before next write to disk
		 * Get the size of the element on disk and use it to calculate offset
		 */
	}

	/* Call parent function to write the data buffer */
	element::flush(path);

	return 0;
}

el_blob::~el_blob()
{
	free(_sp_buffer);
}

el_uint::el_uint(int size, uint32_t en, uint16_t id, uint32_t buf_size)
{
	_real_size = size;
	_size = 0;
	_filled = 0;
	_buffer = NULL;

	setName(en, id);
	this->set_type();

	if (buf_size == 0) {
		 buf_size = RESERVED_SPACE;
	}

	allocate_buffer(buf_size);
}

uint16_t el_uint::fill(uint8_t *data) {
	uint_value.ulong = 0;
	switch(_real_size) {
	case 1:
		/* ubyte */
		uint_value.ubyte = data[0];
		this->append(&(uint_value.ubyte));
		break;
	case 2:
		/* ushort */
		uint_value.ushort = ntohs(*((uint16_t *) data));
		this->append(&(uint_value.ushort));
		break;
	case 3:
		byte_reorder((uint8_t *) &(uint_value.uint),data,_real_size, sizeof(uint));
		this->append(&(uint_value.uint));
		break;
	case 4:
		/* uint */
		uint_value.uint = ntohl(*((uint32_t *) data));
		this->append(&(uint_value.uint));
		break;
	case 5:
	case 6:
	case 7:
		byte_reorder((uint8_t *) &(uint_value.ulong),data,_real_size, sizeof(ulong));
		this->append(&(uint_value.ulong));
		break;
	case 8:
		/* ulong */
		uint_value.ulong = be64toh(*((uint64_t *) data));
		this->append(&(uint_value.ulong));
		break;
	default:
		MSG_ERROR(msg_module, "Invalid element size (%s - %u)", _name, _size);
		return 1;
		break;
	}

	return _real_size;
}

int el_uint::set_type()
{
	switch(_real_size) {
	case 1:
		/* ubyte */
		_type = ibis::UBYTE;
		_size = 1;
		break;
	case 2:
		/* ushort */
		_type = ibis::USHORT;
		_size = 2;
		break;
	case 3:
	case 4:
		/* uint */
		_type = ibis::UINT;
		_size = 4;
		break;
	case 5:
	case 6:
	case 7:
	case 8:
		/* ulong */
		_type = ibis::ULONG;
		_size = 8;
		break;
	default:
		MSG_ERROR(msg_module, "Invalid element size (%s - %u)", _name, _size);
		return 1;
		break;
	}

	return 0;
}

int el_sint::set_type()
{
	switch(_real_size) {
	case 1:
		/* ubyte */
		_type = ibis::BYTE;
		_size = 1;
		break;
	case 2:
		/* ushort */
		_type = ibis::SHORT;
		_size = 2;
		break;
	case 3:
	case 4:
		/* uint */
		_type = ibis::INT;
		_size = 4;
		break;
	case 5:
	case 6:
	case 7:
	case 8:
		/* ulong */
		_type = ibis::LONG;
		_size = 8;
		break;
	default:
		MSG_ERROR(msg_module, "Invalid element size (%s - %u)", _name, _size);
		return 1;
		break;
	}

	return 0;
}

el_sint::el_sint(int size, uint32_t en, uint16_t id, uint32_t buf_size)
{
	_real_size = size;
	_size = 0;
	_filled = 0;
	_buffer = NULL;

	setName(en, id);
	this->set_type();

	if (buf_size == 0) {
		 buf_size = RESERVED_SPACE;
	}

	allocate_buffer(buf_size);
}

el_unknown::el_unknown(int size, uint32_t en, uint16_t id, int part, uint32_t buf_size)
{
	(void) en;
	(void) id;
	(void) part;
	(void) buf_size;

	_size = size;

	/* Size of VAR_IE_LENGTH means variable-sized IE */
	_var_size = (size == VAR_IE_LENGTH);
}

void el_unknown::allocate_buffer(uint32_t count)
{
	(void) count;
}

void el_unknown::free_buffer()
{}

int el_unknown::append(void *data)
{
	(void) data;
	return 0;
}

int el_unknown::flush(std::string path)
{
	(void) path;
	return 0;
}

uint16_t el_unknown::fill(uint8_t *data)
{
	/* Get real size of the data */
	if (_var_size) {
		uint16_t true_size, offset;
		if (data[0] < 255) {
			true_size = data[0];
			offset = 1;
		} else if (data[0] == 255) {
			/* Length is stored in second and third octet */
			true_size = ntohs(*((uint16_t *) &data[1]));
			offset = 3;
		} else {
			MSG_ERROR(msg_module, "Invalid (variable) length specification: %u", data[0]);
		}

		return true_size + offset;
	}

	/* FIXED size */
	return _size;
}

std::string el_unknown::get_part_info()
{
	return  std::string("");
}
