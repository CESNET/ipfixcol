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

extern "C"{
#include <siso.h>
#include <libnf.h>
}

#define __STDC_FORMAT_MACROS
#include <ipfixcol/storage.h>
#include <map>

#include "nfdump_ext.h"
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
    Storage();
    ~Storage();


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

    void setWindowAlignment(bool value = true){ align = value; }
    void setUtilizeChannels(bool enabled = true){ utilize_channels = enabled; }
    void setCompression(bool value = true){ compress = value; }
    void setTimeWindow(unsigned int seconds){ time_window = seconds; }
    void setNameSuffixMask(std::string suffix_mask){ this->suffix_mask = suffix_mask; }
    void setNamePrefix(std::string prefix){ this->prefix = prefix; }
    void setStoragePath(std::string storage_path){ this->storage_path = storage_path; }
    void setIdentificator(std::string ident){ identificator = ident; }


private:
    
    uint8_t mapToLnf(uint64_t en, uint16_t id);

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
     */
	void getElement(uint32_t enterprise, uint16_t id, struct ipfix_element& element);
    
    /**
     * \brief Create raw name for unknown elements
     */
    std::string rawName(uint32_t en, uint16_t id) const;
    
    /**
     * \brief Load informations about IPFIX elements into the memory
     */
    void loadElements();

    int createDirHierarchy(std::string path);
    void createTimeWindow(time_t hint);
    int registerFile(std::string prof_path, std::map<std::string, lnf_file_t*> *files);

    uint16_t offset, id, length;
    uint32_t enterprise;
    Translator translator;          /**< number -> string translator */

    std::vector<char> buffer;
    std::string record;

    static std::map<uint32_t, std::map<uint16_t, struct ipfix_element> > elements;

    static std::map<uint16_t, uint8_t> ie_id_map;          /**< Translation maps ipfix -> lnf */
    static std::map<uint32_t, uint8_t> enterprise_map;
    std::map<std::string, lnf_file_t*> file_map;       /**< Files for record sorting according to profiles */

    lnf_rec_t *recp;

    time_t window_start;    /**< Begining of time window */

    std::string suffix;         /**< Actual time window specific filename suffix */
    std::string dir_hier;       /**< Actual time window specific hierarchy */

    uint64_t time_window;       /**< Dump interval in [s] */

    std::string storage_path;   /**< Root directory for storage files */
    std::string prefix;         /**< Filename prefix */
    std::string suffix_mask;    /**< Filename suffix formatting string */
    std::string identificator;
    bool utilize_channels;
    bool compress;
    bool align;
};

#endif	/* STORAGE_H */
