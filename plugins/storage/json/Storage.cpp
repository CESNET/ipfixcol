/**
 * \file Storage.cpp
 * \author Michal Kozubik <kozubik@cesnet.cz>
 * \brief Class for JSON storage plugin
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
#include <ipfixcol/profiles.h>
}

#include "Storage.h"
#include <stdexcept>
#include <sstream>

#include <iostream>
#include <iomanip>
#include <map>

static const char *msg_module = "json_storage";

#define READ_BYTE_ARR(_dst_, _src_, _len_) \
do {\
	for (int i = 0; i < (_len_); ++i) { \
		(_dst_)[i] = read8((_src_) + i); \
	} \
} while(0)

#define QUOTED(_str_) "\"" + (_str_) + "\""

std::map<uint32_t, std::map<uint16_t, struct ipfix_element> > Storage::elements{};

/**
 * \brief Constructor
 */
Storage::Storage(sisoconf *new_sender): sender{new_sender}
{
	/* Load only once for all plugins */
	if (elements.empty()) {
		this->loadElements();
	}
}

/**
 * \brief Load elements into memory
 */
void Storage::loadElements()
{
	pugi::xml_document doc;

	/* Load file */
	pugi::xml_parse_result result = doc.load_file(ipfix_elements);

	/* Check for errors */
	if (!result) {
		std::stringstream ss;
		ss << "Error when parsing '" << ipfix_elements << "': " << result.description();
		throw std::invalid_argument(ss.str());
	}

	/* Get all elements */
	pugi::xpath_node_set elements_set = doc.select_nodes("/ipfix-elements/element");
	for (auto node: elements_set) {
		uint32_t en = strtoul(node.node().child_value("enterprise"), NULL, 0);
		uint64_t id = strtoul(node.node().child_value("id"), NULL, 0);

		struct ipfix_element element{};
		
		element.name = node.node().child_value("name");
		std::string dataType = node.node().child_value("dataType");

		if		(element.name == "protocolIdentifier")	 element.type = PROTOCOL;
		else if (element.name == "tcpControlBits")		 element.type = FLAGS;
		else if (dataType	  == "ipv4Address")			 element.type = IPV4;
		else if (dataType	  == "ipv6Address")			 element.type = IPV6;
		else if (dataType	  == "macAddress")			 element.type = MAC;
		else if (dataType	  == "dateTimeSeconds")		 element.type = TSTAMP_SEC;
		else if (dataType	  == "dateTimeMilliseconds") element.type = TSTAMP_MILLI;
		else if (dataType	  == "dateTimeMicroseconds") element.type = TSTAMP_MICRO;
		else if (dataType	  == "dateTimeNanoseconds")  element.type = TSTAMP_NANO;
		else if (dataType	  == "string")				 element.type = STRING;
		else											 element.type = RAW;
		
		elements[en][id] = element;
	}
}

/**
 * \brief Send data record
 */
void Storage::sendData() const
{
	if (siso_send(sender, record.c_str(), record.length()) != SISO_OK) {
		throw std::runtime_error(std::string("Sending data: ") + siso_get_last_err(sender));
	}
}

/**
 * \brief Store data sets
 */
void Storage::storeDataSets(const ipfix_message* ipfix_msg)
{	
	/* Iterate through all data records */
	for (int i = 0; i < ipfix_msg->data_records_count; ++i) {
		this->storeDataRecord(&(ipfix_msg->metadata[i]));
	}
}

/**
 * \brief Get real field length
 */
uint16_t Storage::realLength(uint16_t length, uint8_t *data_record, uint16_t &offset) const
{
	/* Static length */
	if (length != 65535) {
		return length;
	}
	
	/* Variable length */
	length = static_cast<int>(read8(data_record + offset));
	offset++;
	
	if (length == 255) {
		length = ntohs(read16(data_record + offset));
		offset += 2;
	}
	
	return length;
}

/**
 * \brief Read string field
 */
std::string Storage::readString(uint16_t& length, uint8_t *data_record, uint16_t &offset) const
{
	/* Get string length */
	length = this->realLength(length, data_record, offset);
	
	/* Read string */
	return std::string((const char *)(data_record + offset), length);
}

/**
 * \brief Read raw data from record
 */
std::string Storage::readRawData(uint16_t &length, uint8_t* data_record, uint16_t &offset) const
{
	/* Read raw value */
	std::ostringstream ss;

	switch (length) {
	case (1):
		ss << static_cast<int>(read8(data_record + offset));
		break;
	case (2):
		ss << ntohs(read16(data_record + offset));
		break;
	case (4):
		ss << ntohl(read32(data_record + offset));
		break;
	case (8):
		ss << be64toh(read64(data_record + offset));
		break;
	default:
		length = this->realLength(length, data_record, offset);
		ss << "0x" << std::hex;
		for (int i = 0; i < length; i++) {
			ss << std::setw(2) << std::setfill('0') << static_cast<int>((data_record + offset)[i]);
		}
	}
	
	return ss.str();
}

/**
 * \brief Create raw name for unknown elements
 */
std::string Storage::rawName(uint32_t en, uint16_t id) const
{
	std::ostringstream ss;
	ss << "e" << en << "id" << id;
	return ss.str();
}

/**
 * \brief Store data record
 */
void Storage::storeDataRecord(struct metadata *mdata)
{
	offset = 0;
	record = "{\"@type\": \"ipfix.entry\", \"ipfix\": {";
	
	struct ipfix_template *templ = mdata->record.templ;
	uint8_t *data_record = (uint8_t*) mdata->record.record;
	
	/* get all fields */
	for (uint16_t count = 0, index = 0; count < templ->field_count; ++count, ++index) {
		/* Get Enterprise number and ID */
		id = templ->fields[index].ie.id;
		length = templ->fields[index].ie.length;
		enterprise = 0;
		
		if (id & 0x8000) {
			id &= 0x7fff;
			enterprise = templ->fields[++index].enterprise_number;
		}
		
		/* Get element informations */
		struct ipfix_element element = this->getElement(enterprise, id);
		
		switch (element.type) {
		case PROTOCOL:
			value = translator.formatProtocol(read8(data_record + offset));
			break;
		case FLAGS:
			value = translator.formatFlags(read16(data_record + offset));
			break;
		case IPV4:
			value = translator.formatIPv4(read32(data_record + offset));
			break;
		case IPV6:{
			READ_BYTE_ARR(addr6, data_record + offset, IPV6_LEN);
			value = translator.formatIPv6(addr6);
			break;}
		case MAC: {
			READ_BYTE_ARR(addrMac, data_record + offset, MAC_LEN);
			value = translator.formatMac(addrMac);
			break;}
		case TSTAMP_SEC:
			value = translator.formatTimestamp(read64(data_record + offset), t_units::SEC);
			break;
		case TSTAMP_MILLI:
			value = translator.formatTimestamp(read64(data_record + offset), t_units::MILLISEC);
			break;
		case TSTAMP_MICRO:
			value = translator.formatTimestamp(read64(data_record + offset), t_units::MICROSEC);
			break;
		case TSTAMP_NANO:
			value = translator.formatTimestamp(read64(data_record + offset), t_units::NANOSEC);
			break;
		case STRING:
			value = this->readString(length, data_record, offset);
			break;
		case RAW:
			value = this->readRawData(length, data_record, offset);
			break;
		default:
			MSG_DEBUG(msg_module, "Unknown element (enterprise %u, id %u)", enterprise, id);
			element.name = rawName(enterprise, id);
			value = this->readRawData(length, data_record, offset);
			break;
		}
		
		if (count > 0) {
			record += ", ";
		}
		
		record += QUOTED(element.name) + ": " + QUOTED(value);
		
		offset += length;
	}
	
	/* Store metadata */
	if (processMetadata) {
		record += "}, \"metadata\": {";
		storeMetadata(mdata);
	}
	
	record += "}}\n";
	sendData();
}

/**
 * \brief Store metadata information
 */
void Storage::storeMetadata(metadata* mdata)
{
	std::stringstream ss;
	
	/* Geolocation info */
	ss << "\"srcAS\": \"" << mdata->srcAS << "\", ";
	ss << "\"dstAS\": \"" << mdata->dstAS << "\", ";
	ss << "\"srcCountry\": \"" << mdata->srcCountry << "\", ";
	ss << "\"dstCountry\": \"" << mdata->dstCountry << "\", ";
	
	
	/* Profiles */
	if (mdata->channels) {
		ss << "\"profiles\": [";

		for (int i = 0; mdata->channels[i] != 0; ++i) {
			if (i > 0) {
				ss << ", ";
			}
			
			std::string channel = channel_get_name(mdata->channels[i]);
			std::string profile = profile_get_name(channel_get_profile(mdata->channels[i]));

			ss << "{\"profile\": \"" << profile << "\", ";
			ss <<  "\"channel\": \"" << channel << "\"}";
		}

		ss << "]";
	}
	
	record += ss.str();
}

