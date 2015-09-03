/**
 * \file Storage.cpp
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

extern "C" {
#include <ipfixcol.h>
#include <ipfixcol/profiles.h>
#include <string.h>
#include <endian.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
}

#include "Storage.h"
#include <stdexcept>
#include <sstream>
#include <ctime>
#include <map>

static const char *msg_module = "nfdump_ext_storage";

#define READ_BYTE_ARR(_dst_, _src_, _len_) \
do {\
	for (int i = 0; i < (_len_); ++i) { \
		(_dst_)[i] = read8((_src_) + i); \
	} \
} while(0)

#define QUOTED(_str_) "\"" + (_str_) + "\""

std::map<uint16_t, uint8_t>Storage::ie_id_map{
    { 0, LNF_FLD_ZERO_ },
    { 1, LNF_FLD_DOCTETS },
    { 2, LNF_FLD_DPKTS },
    { 3, LNF_FLD_AGGR_FLOWS },
    { 4, LNF_FLD_PROT },
    { 5, LNF_FLD_TOS },
    { 6, LNF_FLD_TCP_FLAGS },
    { 7, LNF_FLD_SRCPORT },
    { 8, LNF_FLD_SRCADDR },
    { 9, LNF_FLD_SRC_MASK },
    { 10, LNF_FLD_INPUT },
    { 11, LNF_FLD_DSTPORT },
    { 12, LNF_FLD_DSTADDR },
    { 13, LNF_FLD_DST_MASK },
    { 14, LNF_FLD_OUTPUT },
    { 15, LNF_FLD_IP_NEXTHOP },
    { 16, LNF_FLD_SRCAS },
    { 17, LNF_FLD_DSTAS },
    { 18, LNF_FLD_BGP_NEXTHOP },
    { 21, LNF_FLD_LAST },
    { 22, LNF_FLD_FIRST },
    { 23, LNF_FLD_OUT_BYTES },
    { 24, LNF_FLD_OUT_PKTS },
    { 27, LNF_FLD_SRCADDR },
    { 28, LNF_FLD_DSTADDR },
    { 29, LNF_FLD_SRC_MASK },
    { 30, LNF_FLD_DST_MASK },
    { 38, LNF_FLD_ENGINE_TYPE },
    { 39, LNF_FLD_ENGINE_ID },
    { 55, LNF_FLD_DST_TOS },
    { 56, LNF_FLD_IN_SRC_MAC },
    { 57, LNF_FLD_OUT_DST_MAC },
    { 58, LNF_FLD_SRC_VLAN },
    { 59, LNF_FLD_DST_VLAN },
    { 61, LNF_FLD_DIR },
    { 62, LNF_FLD_IP_NEXTHOP },
    { 63, LNF_FLD_BGP_NEXTHOP },
    { 70, LNF_FLD_MPLS_LABEL }, //this refers to base of stack
    { 71, LNF_FLD_MPLS_LABEL },
    { 72, LNF_FLD_MPLS_LABEL },
    { 73, LNF_FLD_MPLS_LABEL },
    { 74, LNF_FLD_MPLS_LABEL },
    { 75, LNF_FLD_MPLS_LABEL },
    { 76, LNF_FLD_MPLS_LABEL },
    { 77, LNF_FLD_MPLS_LABEL },
    { 78, LNF_FLD_MPLS_LABEL },
    { 79, LNF_FLD_MPLS_LABEL },
    { 80, LNF_FLD_OUT_SRC_MAC },
    { 81, LNF_FLD_IN_DST_MAC },
    { 89, LNF_FLD_FWD_STATUS },
    { 128, LNF_FLD_BGPNEXTADJACENTAS },
    { 129, LNF_FLD_BGPPREVADJACENTAS },
    { 148, LNF_FLD_CONN_ID },
    { 152, LNF_FLD_FIRST },
    { 153, LNF_FLD_LAST },
    { 176, LNF_FLD_ICMP_TYPE },
    { 177, LNF_FLD_ICMP_CODE },
    { 225, LNF_FLD_XLATE_SRC_IP },
    { 226, LNF_FLD_XLATE_DST_IP },
    { 227, LNF_FLD_XLATE_SRC_PORT },
    { 228, LNF_FLD_XLATE_DST_PORT },
    { 230, LNF_FLD_EVENT_FLAG }, //not sure
    { 233, /*LNF_FLD_FW_XEVENT*/ LNF_FLD_ZERO_ },
    { 234, LNF_FLD_INGRESS_VRFID },
    { 235, LNF_FLD_EGRESS_VRFID },
    { 258, LNF_FLD_RECEIVED },
    { 281, LNF_FLD_XLATE_SRC_IP },
    { 282, LNF_FLD_XLATE_DST_IP },
};

std::map<uint32_t, uint8_t>Storage::enterprise_map{
    { 40005, /*LNF_FLD_FW_XEVENT*/ LNF_FLD_ZERO_  },
    { 33002, LNF_FLD_FW_XEVENT },
    { 33000, LNF_FLD_INGRESS_ACL_ID },
    { 33001, LNF_FLD_EGRESS_ACL_ID },
    { 40000, LNF_FLD_USERNAME },

    //compat mappings according to ASA 8.4
    { 40001, LNF_FLD_XLATE_SRC_PORT },
    { 40002, LNF_FLD_XLATE_DST_PORT },
    { 40003, LNF_FLD_XLATE_SRC_IP },
    { 40004, LNF_FLD_XLATE_DST_IP },

    { 57554, LNF_FLD_CLIENT_NW_DELAY_USEC },
    { 57556, LNF_FLD_SERVER_NW_DELAY_USEC },
    { 57557, LNF_FLD_SERVER_NW_DELAY_USEC },
    { 57558, LNF_FLD_APPL_LATENCY_USEC },
    { 57559, LNF_FLD_APPL_LATENCY_USEC }
};

std::map<uint32_t, std::map<uint16_t, struct ipfix_element> > Storage::elements{};



/**
 * \brief Constructor
 */
Storage::Storage()
{
    /* Load only once for all plugins */
    if (elements.empty()) {
        this->loadElements();
    }

    /* Allocate space for buffers */
    record.reserve(4096);
    buffer.reserve(BUFF_SIZE);

    window_start = 0;
    time_window = 300;
}

Storage::~Storage()
{
    lnf_rec_free(recp);

    for(auto it = file_map.begin(); it != file_map.end(); ++it){
        lnf_close(it->second);
        file_map.erase(it);
    }
}

void Storage::getElement(uint32_t enterprise, uint16_t id, struct ipfix_element& element)
{
    element = elements[enterprise][id];
    if (element.type == UNKNOWN && element.name.empty()) {
        element.name = rawName(enterprise, id);
        elements[enterprise][id] = element;
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
 * \brief Store data sets
 */
void Storage::storeDataSets(const ipfix_message* ipfix_msg)
{	
    /* Iterate through all data records */
    for (int i = 0; i < ipfix_msg->data_records_count; ++i) {
        storeDataRecord(&(ipfix_msg->metadata[i]));
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
 * \brief Read string field
 */
void Storage::readString(uint16_t& length, uint8_t *data_record, uint16_t &offset)
{
    /* Get string length */
    length = realLength(length, data_record, offset);

    /* Read string */
    record.append((const char *)(data_record + offset), length);
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
        if (length * 2 > buffer.capacity()) {
            buffer.reserve(length * 2 + 1);
        }

        for (int i = 0; i < length; i++) {
            sprintf(buffer.data() + i * 2, "%02x", (data_record + offset)[i]);
        }
        record += "0x";
    }

    record += buffer.data();
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
    record.clear();

    lnf_rec_init(&recp);
    uint64_t nflows = 1;

    lnf_rec_fset(recp, LNF_FLD_AGGR_FLOWS, &nflows);

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
        struct ipfix_element& element = elements[enterprise][id];
        if (element.type == UNKNOWN && element.name.empty()) {
            element.name = rawName(enterprise, id);
            elements[enterprise][id] = element;
            MSG_DEBUG(msg_module, "Unknown element (%s)", element.name.c_str());
        }

        uint8_t lnf_fld;
        auto it = ie_id_map.find(id);
        if(it != ie_id_map.end() ){
            lnf_fld = it->second;

            union retype {
                uint8_t B[16];
                uint16_t W[8];
                uint32_t DW[4];
                uint64_t QW[2];
            };

            union retype varBuff={0};

            switch (length) {
            case sizeof(uint8_t):
                lnf_rec_fset(recp, lnf_fld, data_record + offset);
                break;
            case sizeof(uint16_t):
                varBuff.W[0] = htobe16(read16(data_record + offset));
                lnf_rec_fset(recp, lnf_fld, &varBuff);
                break;
            case sizeof(uint32_t):
                if(element.type != IPV4){
                    varBuff.DW[0] = htobe32(read32(data_record + offset));
                    lnf_rec_fset(recp, lnf_fld, &varBuff);
                }else{
                    memset(&varBuff, 0x0, sizeof(lnf_ip_t));
                    varBuff.DW[3] = htole32(read32(data_record + offset));
                    lnf_rec_fset(recp, lnf_fld, &varBuff);
                }
                break;
            case sizeof(uint64_t):
                varBuff.QW[0] = htobe64(read64(data_record + offset));
                lnf_rec_fset(recp, lnf_fld, &varBuff);
                break;
            case 2*sizeof(uint64_t):
                varBuff.DW[1] = htobe64(read64(data_record + offset));
                varBuff.DW[0] = htobe64(read64(data_record + offset + sizeof(uint32_t)));
                lnf_rec_fset(recp, lnf_fld, &varBuff);
                break;
            default:
                MSG_WARNING(msg_module, "Data endinannes not corrected !");
                lnf_rec_fset(recp, lnf_fld, data_record + offset);
            }
        }
        offset += length;
    }

    time_t now = time(NULL);
    if(difftime(now, window_start) > time_window){

        createTimeWindow(now);
        registerFile("noprof", &file_map);
    }

    if (mdata->channels && utilize_channels) {
        int status;
        for(int i = 0; mdata->channels[i] != 0; ++i){
            std::string prof_path = profile_get_path(channel_get_profile(mdata->channels[i]));

            auto x = file_map.find(prof_path);
            if(x == file_map.end()){

                registerFile(prof_path, &file_map);
                status = lnf_write(file_map[prof_path], recp);

            }else{
                status = lnf_write(x->second, recp);
            }

            if(status != LNF_OK){
                MSG_WARNING(msg_module, "Can not write record to profile %s", prof_path);
            }
        }
    } else {
        int status = lnf_write(file_map[std::string("noprof")], recp);
        if (status != LNF_OK){
            MSG_WARNING(msg_module, "Can not write record");
        }
    }
}

int Storage::registerFile(std::string prof_path, std::map<std::string, lnf_file_t*> *files)
{
    lnf_file_t* f;

    if(createDirHierarchy( storage_path + prof_path + dir_hier )){
        return 1;
    }

    if(lnf_open(&f, (storage_path + prof_path + dir_hier + prefix + suffix).c_str(),
                LNF_WRITE, identificator.c_str() ) != LNF_OK){
        MSG_WARNING(msg_module, "Failed to open file !");
        return 1;

    } else {
        auto status = files->insert(std::pair<std::string, lnf_file_t*>{ prof_path, f });
        if( status.second == false){
            MSG_WARNING(msg_module, "File for profile %s already registered", prof_path.c_str());
            return 2;
        }
    }
    return 0;
}

void Storage::createTimeWindow(time_t hint)
{
    char buffer[25] = "";

    window_start = hint;
    if(align){
        window_start = (window_start / time_window)* time_window;
    }

    strftime(buffer, 25, "/%Y/%m/%d/", gmtime(&window_start));
    this->dir_hier = buffer;
    strftime(buffer, 25, suffix_mask.c_str(), gmtime(&window_start));
    this->suffix = buffer;

    //Close all active opened files
    for(auto it = file_map.begin(); it != file_map.end(); ++it){
        lnf_close(it->second);
        file_map.erase(it);
    }
}

int Storage::createDirHierarchy(std::string path)
{
    struct stat s;

    std::string::size_type pos = 0;
    while(pos != std::string::npos){
        bool failed = false;
        pos = (path).find_first_of("/\\", pos+1);
        std::string pdir = (path).substr(0,pos);
try_again:
        int status = stat(pdir.c_str(),&s);
        if(status == -1){
            if(ENOENT == errno){
                if(mkdir(pdir.c_str(),S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)){
                    if(!failed){
                        failed = true;
                        goto try_again;
                    }
                    int err = errno;
                    MSG_ERROR(msg_module, "Failed to create directory: %s", pdir.c_str());
                    errno = err;
                    perror(msg_module);
                    return 1;
                }
            }
        } else if(!S_ISDIR(s.st_mode)){
            MSG_ERROR(msg_module, "Failed to create directory, %s is file", pdir.c_str());
            return 2;
        }

    }
    return 0;
}




