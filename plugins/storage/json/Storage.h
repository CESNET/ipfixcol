/**
 * \file Storage.h
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

#ifndef STORAGE_H
#define	STORAGE_H

#include <ipfixcol/storage.h>
#include <map>
#include <siso.h>

#include "pugixml/pugixml.hpp"
#include "Translator.h"

/* some auxiliary functions for extracting data of exact length */
#define read8(_ptr_)  (*((uint8_t *)  (_ptr_)))
#define read16(_ptr_) (*((uint16_t *) (_ptr_)))
#define read32(_ptr_) (*((uint32_t *) (_ptr_)))
#define read64(_ptr_) (*((uint64_t *) (_ptr_)))

#define IPV6_LEN 16
#define MAC_LEN  6

enum element_type {
    UNKNOWN,
    PROTOCOL,
    FLAGS,
    IPV4,
    IPV6,
    MAC,
    TSTAMP_SEC,
    TSTAMP_MILLI,
    TSTAMP_MICRO,
    TSTAMP_NANO,
    STRING,
    RAW
};

struct ipfix_element {
    enum element_type type;
    std::string name;
};

class Storage {
public:
    /**
     * \brief Constructor
     */
    Storage(sisoconf *new_sender = NULL);
    
    /**
     * \brief Set new sender
     * 
     * @param new_sender
     */
    void setSender(sisoconf *new_sender) { this->sender = new_sender; }
    
    /**
     * \brief Get current sender
     * 
     * @return sender
     */
    sisoconf *getSender() { return this->sender; }
    
    /**
     * \brief Store IPFIX message
     * 
     * @param msg IPFIX message
     */
    void storeDataSets(const struct ipfix_message *msg);
    
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
     * @return string
     */
    std::string readString(uint16_t &length, uint8_t *data, uint16_t &offset) const;
    
    /**
     * \brief Read raw data from record on given offset
     * 
     * @param field_len length from template
     * @param data data record
     * @param offset field offset (will be changed)
     * @return field value
     */
    std::string readRawData(uint16_t &length, uint8_t *data, uint16_t &offset) const;
    
    /**
     * \brief Store data record
     * 
     * @param mdata Data record's metadata
     */
    void storeDataRecord(struct metadata *mdata);

    /**
	 * \brief Store metadata
	 * 
     * @param mdata Data record's metadata
     */
	void storeMetadata(struct metadata *mdata);
	
    /**
     * \brief Get element by enterprise and id number
     * 
     * @param enterprise Enterprise number
     * @param id element ID
     * @return element
     */
    struct ipfix_element getElement(uint32_t enterprise, uint16_t id) { return elements[enterprise][id]; }
    
    /**
     * \brief Create raw name for unknown elements
     */
    std::string rawName(uint32_t en, uint16_t id) const;
    
    /**
     * \brief Load informations about IPFIX elements into the memory
     */
    void loadElements();
    
	/**
	 * \brief Send JSON data
     */
    void sendData() const;
    
	bool processMetadata{false};	/**< Metadata processing enabled */
	bool printOnly{false};
	uint8_t addr6[IPV6_LEN];
	uint8_t addrMac[MAC_LEN];
	uint16_t offset, id, length;
	uint32_t enterprise;
    Translator translator;          /**< number -> string translator */
	std::string record, value;		/**< Data record */
    sisoconf *sender;               /**< sender "class" */
    
	static std::map<uint32_t, std::map<uint16_t, struct ipfix_element> > elements;
};

#endif	/* STORAGE_H */

