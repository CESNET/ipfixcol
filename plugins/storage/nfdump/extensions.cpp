/*
 * \file extensions.cpp
 * \author Petr Kramolis <kramolis@cesnet.cz>
 * \brief nfdump storage plugin
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

#include <netinet/in.h>
#include <endian.h>

extern "C" {
#include <ipfixcol/verbose.h>
}

#include "extensions.h"
#include "record_map.h"
//#include "nffile.h"
#include "stdio.h"
#include "nfstore.h"

Extension::Extension(){
	needIdCnt_ = 0;
	offset_ = 0;
	used_ = false;
}

int Extension::checkElements(int ids_cnt, uint16_t *ids, Extension **ids_ext){
	int counter = 0;
	for(int i=0; i<ids_cnt; i++){
		for(int j=0; j<needIdCnt_; j++){
			if(ids[i] == needId_[j][ID]){
				ids_ext[i] = this;
				counter++;
			}
		}
	}
	if(counter){
		return 1;
	}
	return 0;
}

void Extension::readIpfixValue(uint16_t size,uint8_t *element_data, uint64_t *value1 , uint64_t *value2){
	switch (size){
	case 1:
		*value1 = (uint64_t) element_data[0];
		break;
	case 2:
		*value1 = (uint64_t) ntohs(*((uint16_t *) element_data));
		break;
	case 4:
		*value1 = (uint64_t) ntohl(*((uint32_t *) element_data));
		break;
	case 8:
		*value1 = (uint64_t) be64toh(*((uint64_t *) element_data));
		break;
	case 16:
		*value1 = (uint64_t) be64toh(*((uint64_t *) (element_data+8)));
		*value2 = (uint64_t) be64toh(*((uint64_t *) element_data));
		break;
	default:
		if (size<8){ //8 size of uint64_t
			for(int i=0;i<size;i++){
				((uint8_t*)value1)[7-i-(8-size)] = element_data[i];
			}
		}else{
			MSG_WARNING(MSG_MODULE,"Wrong IPFIX element size!");
		}
		break;
	}
}

void Extension::storeNfdumpValue(uint16_t size,uint64_t value1 , uint64_t value2, char *buffer){
	switch (size){
	case 1:
		buffer[0] = value1;
		break;
	case 2:
		*((uint16_t *)(buffer)) = value1;
		break;
	case 4:
		*((uint32_t *)(buffer)) = value1;
		break;
	case 8:
		*((uint64_t *)(buffer)) = value1;
		break;
	case 16:
		*((uint64_t *)(buffer+8)) = value1;
		*((uint64_t *)(buffer)) = value2;
		break;
	default:
		MSG_WARNING(MSG_MODULE,"Wrong extension element size!");
		break;
	}
}

uint16_t Extension::fill(uint16_t id, uint16_t size, uint8_t *element_data
		,char *buffer, struct FlowStats *){
	int offset = 0;
	uint64_t tmp1 = 0;
	uint64_t tmp2 = 0;

	readIpfixValue(size,element_data, &tmp1 , &tmp2);


	char * _buf_location = buffer + offset_;

	for(int i=0; i<needIdCnt_; i++){
		if(id == needId_[i][0]){
			storeNfdumpValue(needId_[i][1], tmp1, tmp2, _buf_location + offset);
		}
		offset+= needId_[i][1];
	}
	return size;
}
void Extension::fillHeader(char *,uint8_t ,uint8_t ,uint16_t ,uint16_t){}

Extension::~Extension() {
	// TODO Auto-generated destructor stub
}


int CommonBlock::checkElements(int ids_cnt, uint16_t *ids, Extension **ids_ext){
	bool flow_start, flow_end, protocol;
	flow_start = flow_end = protocol = false;

	for(int i=0 ; i<ids_cnt; i++){
		switch (ids[i]){
		case START_SEC:
		case START_MILLI:
		case START_MICRO:
		case START_NANO:
			ids_ext[i] = this;
			flow_start = true;
			break;
		case END_SEC:
		case END_MILLI:
		case END_MICRO:
		case END_NANO:
			ids_ext[i] = this;
			flow_end = true;
			break;
		case PROTOCOL:
			ids_ext[i] = this;
			protocol = true;
			break;
		//unnecessary elements
		case FW_STATUS:
		case TCP_FLAGS:
		case CoS:
		case SRC_PORT:
		case DST_PORT:
		case ICMP_TYPE:
			ids_ext[i] = this;
			break;
		default:
			break;
		}
	}
	if(flow_start and flow_end and protocol){
		return 1;
	}
	return 0;
}

void CommonBlock::fillHeader(char *buffer, uint8_t flags,uint8_t tag, uint16_t ext_map, uint16_t size){
	struct common_record_v0_s * record;
	record = (common_record_v0_s *) (buffer + offset_);


	record->type = 1; //type
	record->size = size;
	record->flags = flags;
	record->ext_map = ext_map; //ext_map
}


uint16_t CommonBlock::fill(uint16_t id, uint16_t size, uint8_t *element_data
		,char *buffer, struct FlowStats *stat){

	struct common_record_v0_s * record;
	record = (common_record_v0_s *) (buffer + offset_);
	int factor = 1000;
	uint64_t tmp1 = 0;
	uint64_t tmp2 = 0;
	uint16_t variable_size = 0;

	fflush(stderr);

	if(size == 0xffff){ //variable size element
		if(element_data[0] < 255){
			variable_size = element_data[0] + 1; //1 is firs byte with true size
		}else{
			variable_size = ntohs(*((uint16_t *)&(element_data[1])));
			variable_size+=3; //3 = 1 first byte with 256 and 2 bytes with true size
		}
		return variable_size;
	}

	switch (id){
	case START_NANO:
		factor*=1000;
	case START_MICRO:
		factor*=1000;
	case START_MILLI:
		readIpfixValue(size,element_data, &tmp1 , &tmp2);
		record->first = tmp1/factor;
		record->msec_first = tmp1%factor;
		stat->first_ts = tmp1/factor;
		stat->first_msec_ts = tmp1%factor;
		break;
	case START_SEC:
		readIpfixValue(size,element_data, &tmp1 , &tmp2);
		record->first = (uint32_t) tmp1;
		record->msec_first = 0;
		stat->first_ts = tmp1;
		stat->first_msec_ts = 0;
		break;
	case END_NANO:
		factor*=1000;
	case END_MICRO:
		factor*=1000;
	case END_MILLI:
		readIpfixValue(size,element_data, &tmp1 , &tmp2);
		record->last = tmp1/factor;
		record->msec_last = tmp1%factor;
		stat->last_ts = tmp1/factor;
		stat->last_msec_ts = tmp1%factor;
		break;
	case END_SEC:
		readIpfixValue(size,element_data, &tmp1 , &tmp2);
		record->last = (uint32_t) tmp1;
		record->msec_last = 0;
		stat->last_ts = tmp1;
		stat->last_msec_ts = 0;
		break;
	case FW_STATUS:
		record->fwd_status = element_data[0];
		break;
	case TCP_FLAGS:
		record->tcp_flags = element_data[1];
		break;
	case PROTOCOL:
		record->prot = element_data[0];
		stat->protocol = element_data[0];
		break;
	case CoS:
		record->tos = element_data[0];
		break;
	case SRC_PORT:
		readIpfixValue(size,element_data, &tmp1 , &tmp2);
		record->srcport = (uint16_t) tmp1;
		break;
	case DST_PORT:
		readIpfixValue(size,element_data, &tmp1 , &tmp2);
		record->dstport = (uint16_t) tmp1;
		break;
	case ICMP_TYPE:
		readIpfixValue(size,element_data, &tmp1 , &tmp2);
		record->dstport= (uint16_t) tmp1; //its hold on dstport place!
		break;
	default:
		break;
	}
	return size;
}

//EXTENSION 1
Extension1::Extension1(): IPv4_(false) {}

//EXTENSION 1 IP addresses
int Extension1::checkElements(int ids_cnt, uint16_t *ids, Extension **ids_ext){
	bool srcIPv6,dstIPv6,srcIPv4,dstIPv4;
	srcIPv6 = dstIPv6 = srcIPv4 = dstIPv4 = false;
	for(int i=0 ; i<ids_cnt; i++){
		switch (ids[i]){
		case SRC_IPv4:
			ids_ext[i] = this;
			srcIPv4 = true;
			break;
		case DST_IPv4:
			ids_ext[i] = this;
			dstIPv4 = true;
			break;
		case SRC_IPv6:
			ids_ext[i] = this;
			srcIPv6 = true;
			break;
		case DST_IPv6:
			ids_ext[i] = this;
			dstIPv6 = true;
			break;
		default:
			break;
		}
	}
	if(srcIPv6 and dstIPv6 and srcIPv4 and dstIPv4){
		//there are IPv6 AND IPv4 addresses in ipfix record!
		//ignore IPv6
		IPv4_ = true;
		return 3;
	}
	if(srcIPv4 and dstIPv4){
		IPv4_ = true;
		return 1; // IPv4
	}
	if(srcIPv6 and dstIPv6){
		IPv4_ = false;
		return 2; //IPv6
	}
	return 0; //no IP or no valid src dsc pair;
}

uint16_t Extension1::fill(uint16_t id, uint16_t size, uint8_t *element_data
		,char *buffer, struct FlowStats *stat){

	char * _buf_location = buffer + offset_;
	uint64_t tmp1 = 0;
	uint64_t tmp2 = 0;

	if(IPv4_){
		if(id == SRC_IPv4){
			readIpfixValue(size,element_data, &tmp1 , &tmp2);
			storeNfdumpValue(4, tmp1, tmp2, _buf_location + SRC_IPv4_O);
		}else if(id == DST_IPv4){
			readIpfixValue(size,element_data, &tmp1 , &tmp2);
			storeNfdumpValue(4, tmp1, tmp2, _buf_location + DST_IPv4_O);
		}
	}else{
		stat->flags = stat->flags | 0x1;
		if(id == SRC_IPv6){
			readIpfixValue(size,element_data, &tmp1 , &tmp2);
			storeNfdumpValue(16, tmp1, tmp2, _buf_location + SRC_IPv6_O);
		}else if(id == DST_IPv6){
			readIpfixValue(size,element_data, &tmp1 , &tmp2);
			storeNfdumpValue(16, tmp1, tmp2, _buf_location + DST_IPv6_O);
		}
	}
	return size;
}


//EXTENSION 2
Extension2::Extension2() {
	needIdCnt_ = 1;
	needId_[0][0] = PKT_DELTA_COUNT;
	needId_[0][1] = 8;
	short_ = false;
}

uint16_t Extension2::fill(uint16_t id, uint16_t size, uint8_t *element_data
		,char *buffer, struct FlowStats *stat){

	uint64_t pkt_count;
	char * _buf_location = buffer + offset_;
	uint64_t tmp2 = 0;

	if(id == PKT_DELTA_COUNT){
		readIpfixValue(size,element_data, &pkt_count , &tmp2);
		stat->packets = pkt_count;

		stat->flags = stat->flags | 0x2;
		storeNfdumpValue(8, pkt_count, tmp2, _buf_location);

		/*if(pkt_count > MAX_SHORT){ //TODO
			stat->flags = stat->flags | 0x2;
			printf("PKT COUNT1: %u - %p\n",pkt_count,_buf_location);
			store_nfdump_value(8, pkt_count, tmp2, _buf_location);
		}else{
			_short = true;
			printf("PKT COUNT2: %u - %p\n",pkt_count,_buf_location);
			store_nfdump_value(4, pkt_count, tmp2, _buf_location);
		}*/
	}
	return size;
}

//EXTENSION 3
Extension3::Extension3() {
	needIdCnt_ = 1;
	needId_[0][0] = BYTE_DELTA_COUNT;
	needId_[0][1] = 8;
	short_ = false;
}

uint16_t Extension3::fill(uint16_t id, uint16_t size, uint8_t *element_data
		,char *buffer, struct FlowStats *stat){

	uint64_t byte_count = 0;
	char * _buf_location = buffer + offset_;
	uint64_t tmp2 = 0;

	if(id == BYTE_DELTA_COUNT){
		readIpfixValue(size,element_data, &byte_count , &tmp2);
		stat->bytes = byte_count;

		stat->flags = stat->flags | 0x4;
		storeNfdumpValue(8, byte_count, tmp2, _buf_location);

		/*if(byte_count > MAX_SHORT){ //TODO
			stat->flags = stat->flags | 0x4;
			printf("BYTE COUNT1: %u - %p\n",byte_count,_buf_location);
			store_nfdump_value(8, byte_count, tmp2, _buf_location);
		}else{
			_short = true;
			printf("BYTE COUNT2: %u - %p\n",byte_count,_buf_location);
			store_nfdump_value(4, byte_count, tmp2, _buf_location);
		}*/
	}
	return size;
}

//OPTIONAL EXTENSIONS

//EXTENSION 4 & 5 - interface record (16b ints)
Extension5::Extension5() {
	needIdCnt_ = 2;
	needId_[0][0]= INGRESS_INTERFACE;
	needId_[0][1] = 4;
	needId_[1][0] = EGRESS_INTERFACE;
	needId_[1][1] = 4;
}

//EXTENSION 6 & 7 - AS record (16b ints)
Extension7::Extension7() {
	needIdCnt_ = 2;
	needId_[0][0] = SRC_AS;
	needId_[0][1] = 4;
	needId_[1][0] = DST_AS;
	needId_[1][1] = 4;
}

//EXTENSION 8 - dst tos, dir, srcmask, dstmask in one 32b int
int Extension8::checkElements(int ids_cnt, uint16_t *ids, Extension **ids_ext){
	bool srcIPv6pl,dstIPv6pl,srcIPv4pl,dstIPv4pl,postIPCos,flowDir;
	srcIPv6pl = dstIPv6pl = srcIPv4pl = dstIPv4pl = postIPCos = flowDir = false;
	for(int i=0 ; i<ids_cnt; i++){
		switch (ids[i]){
		case POST_IP_CoS:
			ids_ext[i] = this;
			postIPCos = true;
			break;
		case FLOW_DIRECTION:
			ids_ext[i] = this;
			flowDir = true;
			break;
		case SRC_IPv4_PREFIX_LEN:
			ids_ext[i] = this;
			srcIPv4pl = true;
			break;
		case DST_IPv4_PREFIX_LEN:
			ids_ext[i] = this;
			dstIPv4pl = true;
			break;
		case SRC_IPv6_PREFIX_LEN:
			ids_ext[i] = this;
			srcIPv6pl = true;
			break;
		case DST_IPv6_PREFIX_LEN:
			ids_ext[i] = this;
			dstIPv6pl = true;
			break;
		default:
			break;
		}
	}
	if(flowDir and postIPCos){
		if(srcIPv6pl and dstIPv6pl and srcIPv4pl and dstIPv4pl){
			return 3; //there are IPv6 AND IPv4 addresses in ipfix record!
		}
		if(srcIPv4pl and dstIPv4pl){
			return 1; // IPv4
		}
		if(srcIPv6pl and dstIPv6pl){
			return 2; //IPv6
		}
	}
	return 0; //no IP or no valid src dsc pair;
}

uint16_t Extension8::fill(uint16_t id, uint16_t size, uint8_t *element_data
		,char *buffer, struct FlowStats *){

	char * _buf_location = buffer + offset_;
	uint64_t tmp1 = 0;
	uint64_t tmp2 = 0;

	switch (id){
	case POST_IP_CoS:
		readIpfixValue(size,element_data, &tmp1 , &tmp2);
		storeNfdumpValue(1, tmp1, tmp2, _buf_location + POST_IP_CoS_O);
		break;
	case FLOW_DIRECTION:
		readIpfixValue(size,element_data, &tmp1 , &tmp2);
		storeNfdumpValue(1, tmp1, tmp2, _buf_location + FLOW_DIRECTION_O);
		break;
	case SRC_IPv4_PREFIX_LEN:
		readIpfixValue(size,element_data, &tmp1 , &tmp2);
		storeNfdumpValue(1, tmp1, tmp2, _buf_location + SRC_IPv4_PREFIX_LEN_O);
		break;
	case DST_IPv4_PREFIX_LEN:
		readIpfixValue(size,element_data, &tmp1 , &tmp2);
		storeNfdumpValue(1, tmp1, tmp2, _buf_location + DST_IPv4_PREFIX_LEN_O);
		break;
	case SRC_IPv6_PREFIX_LEN:
		readIpfixValue(size,element_data, &tmp1 , &tmp2);
		storeNfdumpValue(1, tmp1, tmp2, _buf_location + SRC_IPv6_PREFIX_LEN_O);
		break;
	case DST_IPv6_PREFIX_LEN:
		readIpfixValue(size,element_data, &tmp1 , &tmp2);
		storeNfdumpValue(1, tmp1, tmp2, _buf_location + DST_IPv6_PREFIX_LEN_O);
		break;
	default:
		break;
	}
	return size;
}

//EXTENSION 9 - next hop ipv4
Extension9::Extension9() {
	needIdCnt_ = 1;
	needId_[0][0] = IPv4_NEXT_HOP;
	needId_[0][1] = 4;
}

//EXTENSION 10 - next hop ipv6
Extension10::Extension10() {
	needIdCnt_ = 1;
	needId_[0][0] = IPv6_NEXT_HOP;
	needId_[0][1] = 16;
}

//EXTENSION 11 - BGP next hop ipv4
Extension11::Extension11() {
	needIdCnt_ = 1;
	needId_[0][0] = BGP_IPv4_NEXT_HOP;
	needId_[0][1] = 4;
}

//EXTENSION 12 - BGP next hop ipv6
Extension12::Extension12() {
	needIdCnt_ = 1;
	needId_[0][0] = BGP_IPv6_NEXT_HOP;
	needId_[0][1] = 16;
}

//EXTENSION 13 - VLAN record (16b ints)
Extension13::Extension13() {
	needIdCnt_ = 2;
	needId_[0][0] = VLAN_ID;
	needId_[0][1] = 2;
	needId_[1][0] = POST_VLAN_ID;
	needId_[1][1] = 2;
}

//EXTENSION 14 & 15 - Out packet count (32b int)
Extension15::Extension15() {
	needIdCnt_ = 1;
	needId_[0][0] = POST_PKT_DELTA_COUNT;
	needId_[0][1] = 8;
}

//EXTENSION 16 & 17 - Out bytes count (32b int)
Extension17::Extension17() {
	needIdCnt_ = 1;
	needId_[0][0] = POST_BYTE_DELTA_COUNT;
	needId_[0][1] = 8;
}

//EXTENSION 18 & 19 - Aggr flows (32b int)
Extension19::Extension19() {
	needIdCnt_ = 1;
	needId_[0][0] = AGGR_FLOWS;
	needId_[0][1] = 8;
}

//EXTENSION 20 - in src mac, out dst mac (64b int)
Extension20::Extension20() {
	needIdCnt_ = 2;
	needId_[0][0] = SRC_MAC;
	needId_[0][1] = 8;
	needId_[1][0] = POST_DST_MAC;
	needId_[1][1] = 8;
}

//EXTENSION 21 - in dst mac, out src mac (64b int)
Extension21::Extension21() {
	needIdCnt_ = 2;
	needId_[0][0] = DST_MAC;
	needId_[0][1] = 8;
	needId_[1][0] = POST_SRC_MAC;
	needId_[1][1] = 8;
}

//EXTENSION 22 - MPLS (32b ints)
int Extension22::checkElements(int ids_cnt, uint16_t *ids, Extension **ids_ext){
	bool mpls_found = false;
	for(int i=0; i<ids_cnt; i++){
		if(ids[i] >= MPLS_LABEL0 and ids[i] <= MPLS_LABEL9){
			ids_ext[i] = this;
			//one MPLS label is enough
			mpls_found = true;
		}
	}
	if(mpls_found) return 1;
	return 0;
}

uint16_t Extension22::fill(uint16_t id, uint16_t size, uint8_t *element_data
		,char *buffer, struct FlowStats *){

	int offset = 0;
	uint64_t tmp1 = 0;
	uint64_t tmp2 = 0;

	char * _buf_location = buffer + offset_;

	switch(id){
	case MPLS_LABEL9:
		offset+= 4;
	case MPLS_LABEL8:
		offset+= 4;
	case MPLS_LABEL7:
		offset+= 4;
	case MPLS_LABEL6:
		offset+= 4;
	case MPLS_LABEL5:
		offset+= 4;
	case MPLS_LABEL4:
		offset+= 4;
	case MPLS_LABEL3:
		offset+= 4;
	case MPLS_LABEL2:
		offset+= 4;
	case MPLS_LABEL1:
		offset+= 4;
	case MPLS_LABEL0:
		readIpfixValue(size,element_data, &tmp1 , &tmp2);
		storeNfdumpValue(4, tmp1, tmp2, _buf_location + offset);
	 }
	return size;
}

/*void ext22_fill_tm(uint8_t flags, struct ipfix_data_template * data_template){
        int i=0;
        for(i=0;i<10;i++){
                data_template->fields[data_template->field_count].ie.id = 70 + i;
                data_template->fields[data_template->field_count].ie.length = 3;
                data_template->field_count++;
                data_template->data_length += 3;
        }
        data_template->data_template_length += 40;
}*/

//EXTENSION 23 - Router ipv4
/*void ext23_fill_tm(uint8_t flags, struct ipfix_data_template * data_template){
        MSG_WARNING(msg_str, "There is no element for router ip (this extension is ignored)");
}*/


//EXTENSION 24 - Router ipv6
/*void ext24_fill_tm(uint8_t flags, struct ipfix_data_template * data_template){
        MSG_WARNING(msg_str, "There is no element for router ip (this extension is ignored)");
}*/

//EXTENSION 25 - Router source id
/*void ext25_fill_tm(uint8_t flags, struct ipfix_data_template * data_template){
        MSG_INFO(msg_str, "There is no element for router sourc id (filled as reserved 38 and 39 elements)");
        data_template->fields[data_template->field_count].ie.id = 38;
        data_template->fields[data_template->field_count].ie.length = 1;
        data_template->field_count++;
        data_template->data_length += 1;
        data_template->fields[data_template->field_count].ie.id = 39;
        data_template->fields[data_template->field_count].ie.length = 1;
        data_template->field_count++;
        data_template->data_length += 1;
        data_template->data_template_length += 8;
}*/
