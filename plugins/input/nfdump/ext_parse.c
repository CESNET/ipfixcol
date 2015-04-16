/**
 * \file ext_parse.c
 * \brief nfdump input plugin - parsing data.
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

#include <stdint.h>
#include <string.h>
#include "ext_parse.h"
#include "nffile.h"

static const char *msg_module = "nfdump input";


//EXTENSION 0 -- not a real extension its just pading ect
void ext0_parse(uint32_t *data, int *offset, uint8_t flags, struct ipfix_data_set *data_set){
	//MSG_DEBUG(msg_module, "\tZERO EXTENSION");
}


//TODO CREATE MACRO FOR IT !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
#define CONVERT_2x16() \
		*((uint16_t *) &(data_set->records[data_set->header.length])) = htons(*((uint16_t *) &data[*offset])); \
		data_set->header.length += 2; \
		*((uint16_t *) &(data_set->records[data_set->header.length])) = htons(*(((uint16_t *) &data[*offset])+1)); \
		data_set->header.length += 2; \
		(*offset)++;

#define CONVERT_32() \
		*((uint32_t *) &(data_set->records[data_set->header.length])) = htonl(*((uint32_t *) &data[*offset])); \
		data_set->header.length += 4; \
		(*offset)++;


#define CONVERT_64() \
		*((uint64_t *) &(data_set->records[data_set->header.length])) = htobe64(*((uint64_t *) &data[*offset])); \
		data_set->header.length += 8; \
		(*offset)+=2;


#define CONVERT_IPv6() \
		*((uint64_t *) &(data_set->records[data_set->header.length])) = htobe64(*(((uint64_t *) &data[(*offset)]))); \
		data_set->header.length += 8; \
		(*offset)+=2; \
		*((uint64_t *) &(data_set->records[data_set->header.length])) = htobe64(*(((uint64_t *) &data[(*offset)]))); \
		data_set->header.length += 8; \
		(*offset)+=2;

//EXTENSION 1
void ext1_parse(uint32_t *data, int *offset, uint8_t flags, struct ipfix_data_set *data_set){
	if(TestFlag(flags, FLAG_IPV6_ADDR)){
		//MSG_DEBUG(msg_module, "\tIPv6-SRC: hight:%lu low:%lu",*((uint64_t *) &data[*offset]), \
				*((uint64_t *) &data[(*offset)+2]));

		CONVERT_IPv6();

		//MSG_DEBUG(msg_module, "\tIPv6-DST: hight:%lu low:%lu",*((uint64_t *) &data[*offset]), \
				*((uint64_t *) &data[(*offset)+2]));
		CONVERT_IPv6();

	} else {
		//MSG_DEBUG(msg_module, "\tIPv4-SRC: %u", *((uint32_t *) &data[*offset]));
		CONVERT_32();

		//MSG_DEBUG(msg_module, "\tIPv4-DST: %u", *((uint32_t *) &data[*offset]));
		CONVERT_32();
	}
}

//EXTENSION 2
void ext2_parse(uint32_t *data, int *offset, uint8_t flags, struct ipfix_data_set *data_set){
	if(TestFlag(flags, FLAG_PKG_64)){
		//MSG_DEBUG(msg_module, "\tPACKET COUNTER: %lu", *((uint64_t *) &data[*offset]));
		CONVERT_64();
	} else {
		//MSG_DEBUG(msg_module, "\tPACKET COUNTER: %u", *((uint32_t *) &data[*offset]));
		//32b to 64b!
		*((uint64_t *) &(data_set->records[data_set->header.length])) =  htobe64((uint64_t) *((uint32_t *) &data[*offset]));
		data_set->header.length += 8;
		(*offset)+=1;
	}
}

//EXTENSION 3
void ext3_parse(uint32_t *data, int *offset, uint8_t flags, struct ipfix_data_set *data_set){
	if(TestFlag(flags, FLAG_BYTES_64)){
		//MSG_DEBUG(msg_module, "\tBYTE COUNTER: %lu", *((uint64_t *) &data[*offset]));
		CONVERT_64();
	} else {
		//MSG_DEBUG(msg_module, "\tBYTE COUNTER: %u", *((uint32_t *) &data[*offset]));
		//32b to 64b!
		*((uint64_t *) &(data_set->records[data_set->header.length])) =  htobe64((uint64_t)*((uint32_t *) &data[*offset]));
		data_set->header.length += 8;
		(*offset)+=1;
	}
}

//OPTIONAL EXTENSIONS
//EXTENSION 4 - interface record (16b ints)
void ext4_parse(uint32_t *data, int *offset, uint8_t flags, struct ipfix_data_set *data_set){
	//MSG_DEBUG(msg_module, "\tINTERFACE RECORD INPUT: %hu (16b)", *((uint16_t *) &data[*offset]));
	//MSG_DEBUG(msg_module, "\tINTERFACE RECORD OUTPUT: %hu (16b)", *(((uint16_t *) &data[*offset])+1));
	CONVERT_2x16();

}

//EXTENSION 5 - interface record (32b ints)
void ext5_parse(uint32_t *data, int *offset, uint8_t flags, struct ipfix_data_set *data_set){
	//MSG_DEBUG(msg_module, "\tINTERFACE RECORD INPUT: %u (32b)", *((uint32_t *) &data[*offset]));
	CONVERT_32();
	//MSG_DEBUG(msg_module, "\tINTERFACE RECORD OUTPUT: %u (32b)", *((uint32_t *) &data[*offset]));
	CONVERT_32();
}

//OPTIONAL EXTENSIONS
//EXTENSION 6 - AS record (16b ints)
void ext6_parse(uint32_t *data, int *offset, uint8_t flags, struct ipfix_data_set *data_set){
	//MSG_DEBUG(msg_module, "\tAS-SRC: %hu (16b)", *((uint16_t *) &data[*offset]));
	//MSG_DEBUG(msg_module, "\tAS-DST: %hu (16b)", *(((uint16_t *) &data[*offset])+1));
	CONVERT_2x16();

}

//EXTENSION 7 - AS record (32b ints)
void ext7_parse(uint32_t *data, int *offset, uint8_t flags, struct ipfix_data_set *data_set){
	//MSG_DEBUG(msg_module, "\tAS-SRC: %u (32b)", *((uint32_t *) &data[*offset]));
	CONVERT_32();
	//MSG_DEBUG(msg_module, "\tAS-DST: %u (32b)", *((uint32_t *) &data[*offset]));
	CONVERT_32();

}

//EXTENSION 8 - dst tos, dir, srcmask, dstmask in one32b int
void ext8_parse(uint32_t *data, int *offset, uint8_t flags, struct ipfix_data_set *data_set){
	//MSG_DEBUG(msg_module, "\tDST-TOS: %hhu (8b)", *((uint8_t *) &data[*offset]));
	//MSG_DEBUG(msg_module, "\tDIR: %hhu (8b)", *(((uint8_t *) &data[*offset])+1));
	//MSG_DEBUG(msg_module, "\tSRC-MASK: %hhu (8b)", *(((uint8_t *) &data[*offset])+2));
	//MSG_DEBUG(msg_module, "\tDST-MASK: %hhu (8b)", *(((uint8_t *) &data[*offset])+3));
	*((uint32_t *) &(data_set->records[data_set->header.length])) = *((uint32_t *) &data[*offset]);
	data_set->header.length += 4;
	(*offset)++;

}

//EXTENSION 9 - next hop ipv4
void ext9_parse(uint32_t *data, int *offset, uint8_t flags, struct ipfix_data_set *data_set){
	//MSG_DEBUG(msg_module, "\tNEXT-HOP: %u (ipv4)", *((uint32_t *) &data[*offset]));
	CONVERT_32();

}


//EXTENSION 10 - next hop ipv6
void ext10_parse(uint32_t *data, int *offset, uint8_t flags, struct ipfix_data_set *data_set){

	//MSG_DEBUG(msg_module, "\tNEXT-HOP: hight:%lu low:%lu (ipv6)",*((uint64_t *) &data[*offset]), \
			*((uint64_t *) &data[(*offset)+8]));

	CONVERT_IPv6();
}


//EXTENSION 11 - BGP next hop ipv4
void ext11_parse(uint32_t *data, int *offset, uint8_t flags, struct ipfix_data_set *data_set){
	//MSG_DEBUG(msg_module, "\tBGP-NEXT-HOP: %u (ipv4)", *((uint32_t *) &data[*offset]));
	CONVERT_32();

}


//EXTENSION 12 - BGP next hop ipv6
void ext12_parse(uint32_t *data, int *offset, uint8_t flags, struct ipfix_data_set *data_set){
	//MSG_DEBUG(msg_module, "\tBGP-NEXT-HOP: hight:%lu low:%lu (ipv6)",*((uint64_t *) &data[*offset]), \
			*((uint64_t *) &data[(*offset)+8]));
	CONVERT_IPv6();
}

//EXTENSION 13 - VLAN record (16b ints)
void ext13_parse(uint32_t *data, int *offset, uint8_t flags, struct ipfix_data_set *data_set){
	//MSG_DEBUG(msg_module, "\tVLAN-SRC: %hu (16b)", *((uint16_t *) &data[*offset]));
	//MSG_DEBUG(msg_module, "\tVLAN-DST: %hu (16b)", *(((uint16_t *) &data[*offset])+1));
	CONVERT_2x16();
}

//EXTENSION 14 - Out packet count (32b int)
void ext14_parse(uint32_t *data, int *offset, uint8_t flags, struct ipfix_data_set *data_set){
	//MSG_DEBUG(msg_module, "\tOUT-PACKETS: %u (32b)", *((uint32_t *) &data[*offset]));
	CONVERT_32();

}

//EXTENSION 15 - Out packet count (64b int)
void ext15_parse(uint32_t *data, int *offset, uint8_t flags, struct ipfix_data_set *data_set){
	//MSG_DEBUG(msg_module, "\tOUT-PACKETS: %lu (64b)", *((uint64_t *) &data[*offset]));
	CONVERT_64();

}

//EXTENSION 16 - Out bytes count (32b int)
void ext16_parse(uint32_t *data, int *offset, uint8_t flags, struct ipfix_data_set *data_set){
	//MSG_DEBUG(msg_module, "\tOUT-BYTES: %u (32b)", *((uint32_t *) &data[*offset]));
	CONVERT_32();
}

//EXTENSION 17 - Out bytes count (64b int)
void ext17_parse(uint32_t *data, int *offset, uint8_t flags, struct ipfix_data_set *data_set){
	//MSG_DEBUG(msg_module, "\tOUT-BYTES: %lu (64b)", *((uint64_t *) &data[*offset]));
	CONVERT_64();

}

//EXTENSION 18 - Aggr flows (32b int)
void ext18_parse(uint32_t *data, int *offset, uint8_t flags, struct ipfix_data_set *data_set){
	//MSG_DEBUG(msg_module, "\tAGGR-FLOWS: %u (32b)", *((uint32_t *) &data[*offset]));
	CONVERT_32();

}

//EXTENSION 19 - Aggr flows (64b int)
void ext19_parse(uint32_t *data, int *offset, uint8_t flags, struct ipfix_data_set *data_set){
	//MSG_DEBUG(msg_module, "\tAGGR-FLOWS: %lu (64b)", *((uint64_t *) &data[*offset]));
	CONVERT_64();

}

//EXTENSION 20 - in src mac, out dst mac (64b int)
void ext20_parse(uint32_t *data, int *offset, uint8_t flags, struct ipfix_data_set *data_set){
	uint64_t buf;
	//MSG_DEBUG(msg_module, "\tIN-SRC-MAC: %lu (48b - 64 aling)", *((uint64_t *) &data[*offset]));
	buf = htobe64(data[*offset]);
	memcpy(&(data_set->records[data_set->header.length]), &buf, 6);
	data_set->header.length += 6;
	(*offset)+=2;
	//MSG_DEBUG(msg_module, "\tOUT-DST-MAC: %lu (48b - 64 aling)", *((uint64_t *) &data[*offset]));
	buf = htobe64(data[*offset]);
	memcpy(&(data_set->records[data_set->header.length]), &buf, 6);
	data_set->header.length += 6;
	(*offset)+=2;

}

//EXTENSION 21 - in dst mac, out src mac (64b int)
void ext21_parse(uint32_t *data, int *offset, uint8_t flags, struct ipfix_data_set *data_set){
	uint64_t buf;
	//MSG_DEBUG(msg_module, "\tIN-DST-MAC: %lu (48b - 64 aling)", *((uint64_t *) &data[*offset]));
	buf = htobe64(data[*offset]);
	memcpy(&(data_set->records[data_set->header.length]), &buf, 6);
	data_set->header.length += 6;
	(*offset)+=2;
	//MSG_DEBUG(msg_module, "\tOUT-SRC-MAC: %lu (48b - 64 aling)", *((uint64_t *) &data[*offset]));
	buf = htobe64(data[*offset]);
	memcpy(&(data_set->records[data_set->header.length]), &buf, 6);
	data_set->header.length += 6;
	(*offset)+=2;

}

//EXTENSION 22 - MPLS (32b ints)
void ext22_parse(uint32_t *data, int *offset, uint8_t flags, struct ipfix_data_set *data_set){
	int i=0;
	char tmp[4];
	for(i=0;i<10;i++){
		//MSG_DEBUG(msg_module, "\tMPLS-LABEL-%i: %u (32b)",i, *((uint32_t *) &data[*offset]));
		*((uint32_t *)&tmp) = htonl(*((uint32_t *) &data[*offset]));
		memcpy(&(data_set->records[data_set->header.length]),&(tmp[1]),3);
		data_set->header.length += 3;
		(*offset)++;
	}
}

//EXTENSION 23 - Router ipv4
void ext23_parse(uint32_t *data, int *offset, uint8_t flags, struct ipfix_data_set *data_set){
	//MSG_DEBUG(msg_module, "\tROUTER-IP: %u (ipv4)", *((uint32_t *) &data[*offset]));
	CONVERT_32();
}


//EXTENSION 24 - Router ipv6
void ext24_parse(uint32_t *data, int *offset, uint8_t flags, struct ipfix_data_set *data_set){
	//MSG_DEBUG(msg_module, "\tROUTER-IP: hight:%lu low:%lu (ipv6)",*((uint64_t *) &data[*offset]), \
			*((uint64_t *) &data[(*offset)+8]));
	CONVERT_IPv6();

}

//EXTENSION 25 - Router source id
void ext25_parse(uint32_t *data, int *offset, uint8_t flags, struct ipfix_data_set *data_set){
	//MSG_DEBUG(msg_module, "\tROUTER-ID-FILL: %hu ", *((uint16_t *) &data[*offset]));
	*((uint16_t *) &(data_set->records[data_set->header.length])) = htons(*((uint16_t *) &data[*offset])+1);
	data_set->header.length += 2;
	//MSG_DEBUG(msg_module, "\tROUTER-ID-ENGINE-TYPE: %hhu ", *(((uint8_t *) &data[*offset])+2));
	//MSG_DEBUG(msg_module, "\tROUTER-ID-ENGINE-ID: %hhu ", *(((uint8_t *) &data[*offset])+3));
	*((uint16_t *) &(data_set->records[data_set->header.length])) = *((uint16_t *) &data[*offset])+1;
	data_set->header.length += 2;
	(*offset)++;

}
