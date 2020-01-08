/**
 * \file Storage.cpp
 * \author Michal Kozubik <kozubik@cesnet.cz>
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Class for JSON storage plugin
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

/**
 * \todo We are not sure if conversion for type BOOLEAN is in little
 * or big endian -> how to translate it to json. It is necessary to solve it.
 */

extern "C" {
#include <ipfixcol.h>
#include <ipfixcol/profiles.h>
//#include <ipfix_element.h>
}

#include <string.h>

#include "Storage.h"
#include <stdexcept>
#include <sstream>
#include <iostream>
#include <iomanip>
#include "branchlut2.h"

static const char *msg_module = "json_storage";

#define READ_BYTE_ARR(_dst_, _src_, _len_) \
do {\
	for (int i = 0; i < (_len_); ++i) { \
		(_dst_)[i] = read8((_src_) + i); \
	} \
} while(0)

#define STR_APPEND(_string_, _addition_) _string_.append(_addition_, sizeof(_addition_) - 1)

/**
 * \brief Constructor
 */
Storage::Storage()
{
	/* Allocate space for buffers */
	record.reserve(4096);
	buffer.reserve(4096);
}

Storage::~Storage()
{
	for (Output *output: outputs) {
		delete output;
	}
}

/**
 * \brief Send data record
 */
void Storage::sendData() const
{
	for (Output *output: outputs) {
		output->ProcessDataRecord(record);
	}
}

/**
 * \brief Store data sets
 */
void Storage::storeDataSets(const ipfix_message* ipfix_msg, struct json_conf * config)
{
	/* Iterate through all data records */
	for (int i = 0; i < ipfix_msg->data_records_count; ++i) {
		storeDataRecord(&(ipfix_msg->metadata[i]), ipfix_msg, config);
	}
}

/**
 * \brief Get real field length
 */
uint16_t Storage::realLength(uint16_t length, uint8_t *data_record, uint16_t &offset) const
{
	/* Static length */
	if (length != VAR_IE_LENGTH) {
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
 * \brief Read raw data from record
 */
void Storage::readRawData(uint16_t &length, uint8_t* data_record, uint16_t &offset)
{
	/* Read raw value */
	switch (length) {
	case 1:
		sprintf(buffer.data(), "%" PRIu16, static_cast<int>(read8(data_record + offset)));
		break;
	case 2:
		sprintf(buffer.data(), "%" PRIu16, ntohs(read16(data_record + offset)));
		break;
	case 4:
		sprintf(buffer.data(), "%" PRIu32, ntohl(read32(data_record + offset)));
		break;
	case 8:
		sprintf(buffer.data(), "%" PRIu64, be64toh(read64(data_record + offset)));
		break;
	default:
		length = this->realLength(length, data_record, offset);

		if (length == 0) {
			STR_APPEND(record, "null");
			return;
		}

		if (length * 2 > buffer.capacity()) {
			buffer.reserve(length * 2 + 1);
		}

		/* Start the string with 0x and print the rest in hexa */
		strncpy(buffer.data(), "0x", 3);
		for (int i = 0; i < length; i++) {
			sprintf(buffer.data() + i * 2 + 2, "%02x", (data_record + offset)[i]);
		}
	}

	record += '"';
	record += buffer.data();
	record += '"';
}

/**
 * \brief Create raw name for unknown elements
 */
const char* Storage::rawName(uint32_t en, uint16_t id) const
{
	/* Max length is 1("e")+10(en)+2("id")+5(id)+1(\0) */
	static char buf[32];
	snprintf(buf, 32, "e%" PRIu32 "id%" PRIu16, en, id);
	return buf;
}

/**
 * \brief Store data record
 */
void Storage::storeDataRecord(struct metadata *mdata, const struct ipfix_message *ipfix_msg, struct json_conf *config)
{
	const char *element_name = NULL;
	ELEMENT_TYPE element_type;

	offset = 0;
	uint16_t trans_len = 0;
	const char *trans_str = NULL;
	record.clear();
	STR_APPEND(record, "{\"@type\": \"ipfix.entry\", ");

	struct ipfix_template *templ = mdata->record.templ;
	uint8_t *data_record = (uint8_t*) mdata->record.record;

	/* get all fields */
	uint16_t added = 0;
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
		const ipfix_element_t * element = get_element_by_id(id, enterprise);
		if (element != NULL) {
			element_name = element->name;
			element_type = element->type;
		} else {
			// Element not found
			if (config->ignoreUnknown) {
				offset += realLength(length, data_record, offset);
				continue;
			}

			element_name = rawName(enterprise, id);
			element_type = ET_UNASSIGNED;
			MSG_DEBUG(msg_module, "Unknown element (%s)", element_name);
	}

		if (added > 0) {
			STR_APPEND(record, ", ");
		}

		STR_APPEND(record, "\"");
		record += config->prefix;
		record += element_name;
		STR_APPEND(record, "\": ");

		switch (element_type) {
		case ET_UNSIGNED_8:
		case ET_UNSIGNED_16:
		case ET_UNSIGNED_32:
		case ET_UNSIGNED_64:{
			trans_str = translator.toUnsigned(length, &trans_len, data_record, offset,
				element, config);
			record.append(trans_str, trans_len);
		}
			break;
		case ET_SIGNED_8:
		case ET_SIGNED_16:
		case ET_SIGNED_32:
		case ET_SIGNED_64:
			trans_str = translator.toSigned(length, &trans_len, data_record, offset);
			record.append(trans_str, trans_len);
			break;
		case ET_FLOAT_32:
		case ET_FLOAT_64:
			trans_str = translator.toFloat(length, &trans_len, data_record, offset);
			record.append(trans_str, trans_len);
			break;
		case ET_IPV4_ADDRESS:
			record += '"';
			trans_str = translator.formatIPv4(read32(data_record + offset), &trans_len);
			record.append(trans_str, trans_len);
			record += '"';
			break;
		case ET_IPV6_ADDRESS:
			READ_BYTE_ARR(addr6, data_record + offset, IPV6_LEN);
			record += '"';
			record += translator.formatIPv6(addr6);
			record += '"';
			break;
		case ET_MAC_ADDRESS:
			READ_BYTE_ARR(addrMac, data_record + offset, MAC_LEN);
			record += '"';
			record += translator.formatMac(addrMac);
			record += '"';
			break;
		case ET_DATE_TIME_SECONDS:
			record += translator.formatTimestamp(read32(data_record + offset),
				t_units::SEC, config);
			break;
		case ET_DATE_TIME_MILLISECONDS:
			record += translator.formatTimestamp(read64(data_record + offset),
				t_units::MILLISEC, config);
			break;
		case ET_DATE_TIME_MICROSECONDS:
			record += translator.formatTimestamp(read64(data_record + offset),
				t_units::MICROSEC, config);
			break;
		case ET_DATE_TIME_NANOSECONDS:
			record += translator.formatTimestamp(read64(data_record + offset),
				t_units::NANOSEC, config);
			break;
		case ET_STRING:
			length = realLength(length, data_record, offset);
			record += translator.escapeString(length, data_record + offset,
				config);
			break;
		case ET_BOOLEAN:
		case ET_UNASSIGNED:
		default:
			readRawData(length, data_record, offset);
			break;
		}

		offset += length;
		added++;
	}

	/* Store metadata */
	if (processMetadata) {
		STR_APPEND(record, ", \"");
		record += config->prefix;
		STR_APPEND(record, "metadata\": {");
		storeMetadata(mdata);
		STR_APPEND(record, "}");
	}

	/* Store ODID */
	if (config->odid) {
		/* Temporary buffer for the ODID, must be as big as UINT_MAX converted to string */
		char odid_buf[sizeof("4294967295")];

		STR_APPEND(record, ", \"");
		record += config->prefix;
		STR_APPEND(record, "odid\": ");
		/* Convert ODID efficiently */
		char *odid_buf_pos = u32toa_branchlut2(ipfix_msg->input_info->odid, odid_buf);
		record.append(odid_buf, odid_buf_pos - odid_buf);
	}

	/* Store Detailed Information */
	if (config->detailedInfo) {
		char conv_buf[sizeof("4294967295")], *conv_buf_pos = NULL;

		STR_APPEND(record, ", \"ipfixcol.packet_length\": ");
		conv_buf_pos = u32toa_branchlut2(ntohs(ipfix_msg->pkt_header->length), conv_buf);
		record.append(conv_buf, conv_buf_pos - conv_buf);

		STR_APPEND(record, ", \"ipfixcol.export_time\": ");
		conv_buf_pos = u32toa_branchlut2(ntohl(ipfix_msg->pkt_header->export_time), conv_buf);
		record.append(conv_buf, conv_buf_pos - conv_buf);

		STR_APPEND(record, ", \"ipfixcol.sequence_number\": ");
		conv_buf_pos = u32toa_branchlut2(ntohl(ipfix_msg->pkt_header->sequence_number), conv_buf);
		record.append(conv_buf, conv_buf_pos - conv_buf);

		STR_APPEND(record, ", \"ipfixcol.template_id\": ");
		conv_buf_pos = u32toa_branchlut2(templ->original_id, conv_buf);
		record.append(conv_buf, conv_buf_pos - conv_buf);
	}

	STR_APPEND(record, "}\n");
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
	ss << "\"srcName\": \"" << mdata->srcName << "\", ";
	ss << "\"dstName\": \"" << mdata->dstName << "\", ";

	record += ss.str();


	/* Profiles */
	STR_APPEND(record, "\"profiles\": [");
	if (mdata->channels) {
		// Get name of root profile
		void *profile_ptr = NULL;
		void *prev_profile_ptr = NULL;
		const char *root_profile_name;

		profile_ptr = channel_get_profile(mdata->channels[0]);
		while (profile_ptr != NULL) {
			prev_profile_ptr = profile_ptr;
			profile_ptr = profile_get_parent(profile_ptr);
		}
		root_profile_name = profile_get_name(prev_profile_ptr);

		// Process all channels
		for (int i = 0; mdata->channels[i] != 0; ++i) {
			if (i > 0) {
				STR_APPEND(record, ", ");
			}

			STR_APPEND(record, "{\"profile\": \"");
			record += root_profile_name;
			STR_APPEND(record, "/");
			record += profile_get_path(channel_get_profile(mdata->channels[i]));

			STR_APPEND(record, "\", \"channel\": \"");
			record += channel_get_name(mdata->channels[i]);
			STR_APPEND(record, "\"}");
		}
	}
	record += ']';
}

