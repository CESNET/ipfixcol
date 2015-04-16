/*
 * \file extensions.h
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

#ifndef EXTENSIONS_H_
#define EXTENSIONS_H_

#include <ipfixcol.h>
#include <iostream>

class Extension {
protected:
	enum {MAX_ID = 5, ID = 0, NF_SIZE = 1};
	uint32_t needId_[MAX_ID][2]; //stores element ID, its nfdump size
	int needIdCnt_;
	uint32_t offset_;
	bool used_;

public:
	Extension();
	virtual int checkElements(int ids_cnt, uint16_t *ids, Extension **ids_ext);
	virtual uint16_t fill(uint16_t id, uint16_t size, uint8_t *element_data,
			char *buffer,struct FlowStats *stats);
	virtual uint16_t extId() {return 0;}
	void readIpfixValue(uint16_t size,uint8_t *element_data, uint64_t *value1 , uint64_t *value2);
	void storeNfdumpValue(uint16_t size,uint64_t value1 , uint64_t value2, char *buffer);
	virtual void fillHeader(char*,uint8_t ,uint8_t ,uint16_t ,uint16_t);
	bool used(){ return used_;}
	void used( bool use_val){used_ = use_val;}
	void offset( uint32_t offset){offset_ = offset;}
	virtual uint32_t size() {return 0;}
	virtual ~Extension();
};

struct CommonRecord {
 	uint16_t type;
 	uint16_t size;
	uint8_t flags;
	uint8_t exporter;
 	uint16_t ext_map;
 	uint16_t m_ts_first;
 	uint16_t m_ts_last;
 	uint32_t ts_first;
 	uint32_t ts_last;
 	uint8_t fwd_status;
 	uint8_t tcp_flags;
 	uint8_t protocol;
 	uint8_t tos;
 	uint16_t srcport;
 	uint16_t dstport;
};

class CommonBlock: public Extension {
	enum{START_SEC = 150 ,END_SEC = 151,
		START_MILLI = 152, END_MILLI = 153,
		START_MICRO = 154, END_MICRO = 155,
		START_NANO = 156, END_NANO = 157, FW_STATUS = 89, //TODO
		TCP_FLAGS = 6, PROTOCOL = 4, CoS = 5,
		SRC_PORT = 7, DST_PORT = 11, ICMP_TYPE = 32};

	enum offsets{ MSEC_START_O = 8 ,MSEC_END_O = 10,
				START_O = 12, END_O = 16, FW_STAT_O=20,
				TCP_FLAGS_O = 21, PROT_O = 22, CoS_O = 23,
				SRC_PORT_O = 24, DST_PORT_O = 26, ICMP_TYPE_O = 26};

public:
	virtual int checkElements(int ids_cnt, uint16_t *ids, Extension **ids_ext);
	virtual uint16_t fill(uint16_t id, uint16_t size, uint8_t *element_data,
				char *buffer,struct FlowStats *stats);
	virtual void fillHeader(char *buffer,uint8_t flags,uint8_t tag, uint16_t ext_map, uint16_t size);
	virtual uint16_t extId() {return 0;}
	uint32_t size() {return 28;}
};

class Extension1: public Extension {
	enum{SRC_IPv4 = 8,DST_IPv4 = 12, SRC_IPv6 = 27, DST_IPv6 = 28};
	enum offsets{SRC_IPv4_O = 0,DST_IPv4_O = 4, SRC_IPv6_O = 0, DST_IPv6_O = 16};
	bool IPv4_;
public:
	virtual int checkElements(int ids_cnt, uint16_t *ids, Extension **ids_ext);
	virtual uint16_t fill(uint16_t id, uint16_t size, uint8_t *element_data
			,char *buffer, struct FlowStats *stat);
	virtual uint16_t extId() {return 1;}
	uint32_t size() {return IPv4_?8:32;}
};

class Extension2: public Extension {
	enum{PKT_DELTA_COUNT = 2, MAX_SHORT = 4294967295};
	bool short_;
public:
	Extension2();
	virtual uint16_t fill(uint16_t id, uint16_t size, uint8_t *element_data,
				char *buffer,struct FlowStats *stats);
	virtual uint16_t extId() {return 2;}
	uint32_t size() {return short_?4:8;}
};

class Extension3: public Extension {
	enum{BYTE_DELTA_COUNT = 1,MAX_SHORT = 4294967295 };
	bool short_;
public:
	Extension3();
	virtual uint16_t fill(uint16_t id, uint16_t size, uint8_t *element_data,
				char *buffer,struct FlowStats *stats);
	virtual uint16_t extId() {return 3;}
	uint32_t size() {return short_?4:8;}
};

//class extension4: public extension {}; //NO needed its same as ext5
class Extension5: public Extension {
	enum{INGRESS_INTERFACE = 10,EGRESS_INTERFACE = 14 };
public:
	Extension5();
	virtual uint16_t extId() {return 5;}
	uint32_t size() {return 8;}
};


//class extension6: public extension {}; // NO needed is same as ext7
class Extension7: public Extension {
	enum{SRC_AS = 16,DST_AS = 17 };
public:
	Extension7();
	virtual uint16_t extId() {return 7;}
	uint32_t size() {return 8;}
};


class Extension8: public Extension {
	enum{POST_IP_CoS = 55,FLOW_DIRECTION = 61, SRC_IPv6_PREFIX_LEN = 29,
		DST_IPv6_PREFIX_LEN = 30, SRC_IPv4_PREFIX_LEN = 9,
		DST_IPv4_PREFIX_LEN = 13};
	enum offsets{POST_IP_CoS_O = 0,FLOW_DIRECTION_O = 1, SRC_IPv6_PREFIX_LEN_O =2,
			DST_IPv6_PREFIX_LEN_O = 3, SRC_IPv4_PREFIX_LEN_O = 2,
			DST_IPv4_PREFIX_LEN_O = 3};
public:
	virtual int checkElements(int ids_cnt, uint16_t *ids, Extension **ids_ext);
	uint16_t fill(uint16_t id, uint16_t size, uint8_t *element_data
			,char *buffer, struct FlowStats *stat);
	virtual uint16_t extId() {return 8;}
	uint32_t size() {return 4;}
};

class Extension9: public Extension {
	enum{IPv4_NEXT_HOP = 15};
public:
	Extension9();
	virtual uint16_t extId() {return 9;}
	uint32_t size() {return 4;}
};

class Extension10: public Extension {
		enum{IPv6_NEXT_HOP = 62};
	public:
		Extension10();
		virtual uint16_t extId() {return 10;}
		uint32_t size() {return 16;}
};

class Extension11: public Extension {
	enum{BGP_IPv4_NEXT_HOP = 18};
public:
	Extension11();
	virtual uint16_t extId() {return 11;}
	uint32_t size() {return 4;}
};

class Extension12: public Extension {
	enum{BGP_IPv6_NEXT_HOP = 63};
public:
	Extension12();
	virtual uint16_t extId() {return 12;}
	uint32_t size() {return 16;}
};

class Extension13: public Extension {
	enum{VLAN_ID = 58, POST_VLAN_ID = 59};
public:
	Extension13();
	virtual uint16_t extId() {return 13;}
	uint32_t size() {return 4;}
};

//class extension14: public extension //NO needed same as ext 15
class Extension15: public Extension {
	enum{POST_PKT_DELTA_COUNT = 24 };
public:
	Extension15();
	virtual uint16_t extId() {return 15;}
	uint32_t size() {return 8;}
};

//class extension16: public extension {}; //NO needed same as ext 17
class Extension17: public Extension {
	enum{POST_BYTE_DELTA_COUNT = 23};
public:
	Extension17();
	virtual uint16_t extId() {return 17;}
	uint32_t size() {return 8;}
};


//class extension18: public extension {}; //NO needed same as ext 19
class Extension19: public Extension {
	enum{AGGR_FLOWS = 3};
public:
	Extension19();
	virtual uint16_t extId() {return 19;}
	uint32_t size() {return 8;}
};

class Extension20: public Extension {
	enum{SRC_MAC = 56,POST_DST_MAC = 57 };
public:
	Extension20();
	virtual uint16_t extId() {return 20;}
	uint32_t size() {return 16;}
};

class Extension21: public Extension {
	enum{DST_MAC = 80,POST_SRC_MAC = 81 };
public:
	Extension21();
	virtual uint16_t extId() {return 21;}
	uint32_t size() {return 16;}
};

class Extension22: public Extension {
	enum{MPLS_LABEL0 = 70,MPLS_LABEL1 = 71,
		 MPLS_LABEL2 = 72,MPLS_LABEL3 = 73,
		 MPLS_LABEL4 = 74,MPLS_LABEL5 = 75,
		 MPLS_LABEL6 = 76,MPLS_LABEL7 = 77,
		 MPLS_LABEL8 = 78,MPLS_LABEL9 = 79 };
public:
	virtual int checkElements(int ids_cnt, uint16_t *ids, Extension **ids_ext);
	virtual uint16_t fill(uint16_t id, uint16_t size, uint8_t *element_data
			,char *buffer, struct FlowStats *stat);
	virtual uint16_t extId() {return 22;}
	uint32_t size() {return 40;}
};

//class extension23: public extension {}; //router IPv4 there is no such element in ipfix
//class extension24: public extension {}; //router IPv6 there is no such element in ipfix
//class extension25: public extension {}; //router IDs there is no such element in ipfix

#endif /* EXTENSIONS_H_ */
