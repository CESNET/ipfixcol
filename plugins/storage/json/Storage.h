/**
 * \file Storage.h
 * \author Michal Kozubik <kozubik@cesnet.cz>
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

#ifndef STORAGE_H
#define	STORAGE_H

#define __STDC_FORMAT_MACROS
#include <ipfixcol/storage.h>
#include <map>
#include <siso.h>

#include "json.h"
#include "pugixml/pugixml.hpp"
#include "Translator.h"

/* some auxiliary functions for extracting data of exact length */
#define read8(_ptr_)  (*((uint8_t *)  (_ptr_)))
#define read16(_ptr_) (*((uint16_t *) (_ptr_)))
#define read32(_ptr_) (*((uint32_t *) (_ptr_)))
#define read64(_ptr_) (*((uint64_t *) (_ptr_)))

#define IPV6_LEN 16
#define MAC_LEN  6

/* size of bytes 1, 2, 4 or 8 */
#define BYTE1 1
#define BYTE2 2
#define BYTE4 4
#define BYTE8 8

/* white spaces needed to replace them by \n otation (\n \t ..) */
#define SPACE           ' '
#define TABULATOR       '\t'
#define NEWLINE         '\n'
#define QUOTATION       '\"'
#define REVERSESOLIDUS  '\\'
#define SOLIDUS         '/'
#define BACKSPACE       '\b'
#define FORMFEED        '\f'
#define RETURN          '\r'

class Storage {
public:
    /**
     * \brief Constructor
     */
	Storage();
	~Storage();
    
	/**
	 * \brief Add new output processor
	 * \param[in] output
	 */
	void addOutput(Output *output) { outputs.push_back(output); }

	bool hasSomeOutput() { return !outputs.empty(); }
    
    /**
     * \brief Store IPFIX message
     * 
     * @param msg IPFIX message
     */
	void storeDataSets(const struct ipfix_message *msg, struct json_conf * config);
    
	/**
	 * \brief Set metadata processing enabled/disabled
	 * 
     * @param enabled
     */
	void setMetadataProcessing(bool enabled) { processMetadata = enabled; }

	/**
	 * \brief Enable/disable data printing instead of sending them
	 *
	 * \param enabled
	 */
	void setPrintOnly(bool enabled) { printOnly = enabled; }
	
private:
    
    /**
     * \brief Get real field length
     * 
     * @param length length from template
     * @param data data record
     * @param offset field offset
     * @return real length;
     */
    uint16_t realLength(uint16_t length, uint8_t *data, uint16_t &offset) const;
    
    /**
     * \brief Read string field
     * 
     * @param length length from template
     * @param data data record
     * @param offset field offset
     */
	void readString(uint16_t &length, uint8_t *data, uint16_t &offset);
    
    /**
     * \brief Read raw data from record on given offset
     * 
     * @param field_len length from template
     * @param data data record
     * @param offset field offset (will be changed)
     */
	void readRawData(uint16_t &length, uint8_t *data, uint16_t &offset);
    
    /**
     * \brief Store data record
     * 
     * @param mdata Data record's metadata
     */
	void storeDataRecord(struct metadata *mdata, struct json_conf * config);

    /**
	 * \brief Store metadata
	 * 
     * @param mdata Data record's metadata
     */
	void storeMetadata(struct metadata *mdata);
	
    /**
     * \brief Create raw name for unknown elements
     */
    std::string rawName(uint32_t en, uint16_t id) const;
    
    
	/**
	 * \brief Send JSON data to output processors
     */
	void sendData() const;
    
	bool processMetadata{false};	/**< Metadata processing enabled */
	bool printOnly{false};
	uint8_t addr6[IPV6_LEN];
	uint8_t addrMac[MAC_LEN];
	uint16_t offset{0}, id{0}, length{0};
	uint32_t enterprise{0};
    Translator translator;          /**< number -> string translator */

	std::vector<Output*> outputs{};
	std::vector<char> buffer;

	char stringWithEscseq[(65536 * 2) - 1];	/**< String from IPFIXpacket with white spaces replaced by \notations (\n \t ...)  */
	std::string record;
};

#endif	/* STORAGE_H */
