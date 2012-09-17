
#include <stdint.h>
#include <string.h>
#include "ext_parse.h"
#include "nffile.h"

extern char *msg_str;


//EXTENSION 0 -- not a real extension its just pading ect
void ext0_parse(uint32_t *data, int *offset, uint8_t flags, struct ipfix_data_set *data_set){
	MSG_NOTICE(msg_str, "\tZERO EXTENSION");
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
		*((uint64_t *) &(data_set->records[data_set->header.length])) = htobe64(*(((uint64_t *) &data[(*offset)])+1)); \
		data_set->header.length += 8; \
		*((uint64_t *) &(data_set->records[data_set->header.length])) = htobe64(*((uint64_t *) &data[*offset])); \
		data_set->header.length += 8; \
		(*offset)+=4;

//EXTENSION 1
void ext1_parse(uint32_t *data, int *offset, uint8_t flags, struct ipfix_data_set *data_set){
	if(TestFlag(flags, FLAG_IPV6_ADDR)){
		MSG_NOTICE(msg_str, "\tIPv6-SRC: hight:%lu low:%lu",*((uint64_t *) &data[*offset]), \
				*((uint64_t *) &data[(*offset)+2]));

		CONVERT_IPv6();

		MSG_NOTICE(msg_str, "\tIPv6-DST: hight:%lu low:%lu",*((uint64_t *) &data[*offset]), \
				*((uint64_t *) &data[(*offset)+2]));
		CONVERT_IPv6();

	} else {
		MSG_NOTICE(msg_str, "\tIPv4-SRC: %u", *((uint32_t *) &data[*offset]));
		CONVERT_32();

		MSG_NOTICE(msg_str, "\tIPv4-DST: %u", *((uint32_t *) &data[*offset]));
		CONVERT_32();
	}
}

//EXTENSION 2
void ext2_parse(uint32_t *data, int *offset, uint8_t flags, struct ipfix_data_set *data_set){
	if(TestFlag(flags, FLAG_PKG_64)){
		MSG_NOTICE(msg_str, "\tPACKET COUNTER: %lu", *((uint64_t *) &data[*offset]));
		CONVERT_64();
	} else {
		MSG_NOTICE(msg_str, "\tPACKET COUNTER: %u", *((uint32_t *) &data[*offset]));
		//32b to 64b!
		*((uint64_t *) &(data_set->records[data_set->header.length])) =  htobe64((uint64_t) *((uint32_t *) &data[*offset]));
		data_set->header.length += 8;
		(*offset)+=1;
	}
}

//EXTENSION 3
void ext3_parse(uint32_t *data, int *offset, uint8_t flags, struct ipfix_data_set *data_set){
	if(TestFlag(flags, FLAG_BYTES_64)){
		MSG_NOTICE(msg_str, "\tBYTE COUNTER: %lu", *((uint64_t *) &data[*offset]));
		CONVERT_64();
	} else {
		MSG_NOTICE(msg_str, "\tBYTE COUNTER: %u", *((uint32_t *) &data[*offset]));
		//32b to 64b!
		*((uint64_t *) &(data_set->records[data_set->header.length])) =  htobe64((uint64_t)*((uint32_t *) &data[*offset]));
		data_set->header.length += 8;
		(*offset)+=1;
	}
}

//OPTIONAL EXTENSIONS
//EXTENSION 4 - interface record (16b ints)
void ext4_parse(uint32_t *data, int *offset, uint8_t flags, struct ipfix_data_set *data_set){
	MSG_NOTICE(msg_str, "\tINTERFACE RECORD INPUT: %hu (16b)", *((uint16_t *) &data[*offset]));
	MSG_NOTICE(msg_str, "\tINTERFACE RECORD OUTPUT: %hu (16b)", *(((uint16_t *) &data[*offset])+1));
	CONVERT_2x16();

}

//EXTENSION 5 - interface record (32b ints)
void ext5_parse(uint32_t *data, int *offset, uint8_t flags, struct ipfix_data_set *data_set){
	MSG_NOTICE(msg_str, "\tINTERFACE RECORD INPUT: %u (32b)", *((uint32_t *) &data[*offset]));
	CONVERT_32();
	MSG_NOTICE(msg_str, "\tINTERFACE RECORD OUTPUT: %u (32b)", *((uint32_t *) &data[*offset]));
	CONVERT_32();
}

//OPTIONAL EXTENSIONS
//EXTENSION 6 - AS record (16b ints)
void ext6_parse(uint32_t *data, int *offset, uint8_t flags, struct ipfix_data_set *data_set){
	MSG_NOTICE(msg_str, "\tAS-SRC: %hu (16b)", *((uint16_t *) &data[*offset]));
	MSG_NOTICE(msg_str, "\tAS-DST: %hu (16b)", *(((uint16_t *) &data[*offset])+1));
	CONVERT_2x16();

}

//EXTENSION 7 - AS record (32b ints)
void ext7_parse(uint32_t *data, int *offset, uint8_t flags, struct ipfix_data_set *data_set){
	MSG_NOTICE(msg_str, "\tAS-SRC: %u (32b)", *((uint32_t *) &data[*offset]));
	CONVERT_32();
	MSG_NOTICE(msg_str, "\tAS-DST: %u (32b)", *((uint32_t *) &data[*offset]));
	CONVERT_32();

}

//EXTENSION 8 - dst tos, dir, srcmask, dstmask in one32b int
void ext8_parse(uint32_t *data, int *offset, uint8_t flags, struct ipfix_data_set *data_set){
	MSG_NOTICE(msg_str, "\tDST-TOS: %hhu (8b)", *((uint8_t *) &data[*offset]));
	MSG_NOTICE(msg_str, "\tDIR: %hhu (8b)", *(((uint8_t *) &data[*offset])+1));
	MSG_NOTICE(msg_str, "\tSRC-MASK: %hhu (8b)", *(((uint8_t *) &data[*offset])+2));
	MSG_NOTICE(msg_str, "\tDST-MASK: %hhu (8b)", *(((uint8_t *) &data[*offset])+3));
	*((uint32_t *) &(data_set->records[data_set->header.length])) = *((uint32_t *) &data[*offset]);
	data_set->header.length += 4;
	(*offset)++;

}

//EXTENSION 9 - next hop ipv4
void ext9_parse(uint32_t *data, int *offset, uint8_t flags, struct ipfix_data_set *data_set){
	MSG_NOTICE(msg_str, "\tNEXT-HOP: %u (ipv4)", *((uint32_t *) &data[*offset]));
	CONVERT_32();

}


//EXTENSION 10 - next hop ipv6
void ext10_parse(uint32_t *data, int *offset, uint8_t flags, struct ipfix_data_set *data_set){

	MSG_NOTICE(msg_str, "\tNEXT-HOP: hight:%lu low:%lu (ipv6)",*((uint64_t *) &data[*offset]), \
			*((uint64_t *) &data[(*offset)+8]));

	CONVERT_IPv6();
}


//EXTENSION 11 - BGP next hop ipv4
void ext11_parse(uint32_t *data, int *offset, uint8_t flags, struct ipfix_data_set *data_set){
	MSG_NOTICE(msg_str, "\tBGP-NEXT-HOP: %u (ipv4)", *((uint32_t *) &data[*offset]));
	CONVERT_32();

}


//EXTENSION 12 - BGP next hop ipv6
void ext12_parse(uint32_t *data, int *offset, uint8_t flags, struct ipfix_data_set *data_set){
	MSG_NOTICE(msg_str, "\tBGP-NEXT-HOP: hight:%lu low:%lu (ipv6)",*((uint64_t *) &data[*offset]), \
			*((uint64_t *) &data[(*offset)+8]));
	CONVERT_IPv6();
}

//EXTENSION 13 - VLAN record (16b ints)
void ext13_parse(uint32_t *data, int *offset, uint8_t flags, struct ipfix_data_set *data_set){
	MSG_NOTICE(msg_str, "\tVLAN-SRC: %hu (16b)", *((uint16_t *) &data[*offset]));
	MSG_NOTICE(msg_str, "\tVLAN-DST: %hu (16b)", *(((uint16_t *) &data[*offset])+1));
	CONVERT_2x16();
}

//EXTENSION 14 - Out packet count (32b int)
void ext14_parse(uint32_t *data, int *offset, uint8_t flags, struct ipfix_data_set *data_set){
	MSG_NOTICE(msg_str, "\tOUT-PACKETS: %u (32b)", *((uint32_t *) &data[*offset]));
	CONVERT_32();

}

//EXTENSION 15 - Out packet count (64b int)
void ext15_parse(uint32_t *data, int *offset, uint8_t flags, struct ipfix_data_set *data_set){
	MSG_NOTICE(msg_str, "\tOUT-PACKETS: %lu (64b)", *((uint64_t *) &data[*offset]));
	CONVERT_64();

}

//EXTENSION 16 - Out bytes count (32b int)
void ext16_parse(uint32_t *data, int *offset, uint8_t flags, struct ipfix_data_set *data_set){
	MSG_NOTICE(msg_str, "\tOUT-BYTES: %u (32b)", *((uint32_t *) &data[*offset]));
	CONVERT_32();
}

//EXTENSION 17 - Out bytes count (64b int)
void ext17_parse(uint32_t *data, int *offset, uint8_t flags, struct ipfix_data_set *data_set){
	MSG_NOTICE(msg_str, "\tOUT-BYTES: %lu (64b)", *((uint64_t *) &data[*offset]));
	CONVERT_64();

}

//EXTENSION 18 - Aggr flows (32b int)
void ext18_parse(uint32_t *data, int *offset, uint8_t flags, struct ipfix_data_set *data_set){
	MSG_NOTICE(msg_str, "\tAGGR-FLOWS: %u (32b)", *((uint32_t *) &data[*offset]));
	CONVERT_32();

}

//EXTENSION 19 - Aggr flows (64b int)
void ext19_parse(uint32_t *data, int *offset, uint8_t flags, struct ipfix_data_set *data_set){
	MSG_NOTICE(msg_str, "\tAGGR-FLOWS: %lu (64b)", *((uint64_t *) &data[*offset]));
	CONVERT_64();

}

//EXTENSION 20 - in src mac, out dst mac (64b int)
void ext20_parse(uint32_t *data, int *offset, uint8_t flags, struct ipfix_data_set *data_set){
	uint64_t buf;
	MSG_NOTICE(msg_str, "\tIN-SRC-MAC: %lu (48b - 64 aling)", *((uint64_t *) &data[*offset]));
	buf = htobe64(data[*offset]);
	memcpy(&(data_set->records[data_set->header.length]), &buf, 6);
	data_set->header.length += 6;
	(*offset)+=2;
	MSG_NOTICE(msg_str, "\tOUT-DST-MAC: %lu (48b - 64 aling)", *((uint64_t *) &data[*offset]));
	buf = htobe64(data[*offset]);
	memcpy(&(data_set->records[data_set->header.length]), &buf, 6);
	data_set->header.length += 6;
	(*offset)+=2;

}

//EXTENSION 21 - in dst mac, out src mac (64b int)
void ext21_parse(uint32_t *data, int *offset, uint8_t flags, struct ipfix_data_set *data_set){
	uint64_t buf;
	MSG_NOTICE(msg_str, "\tIN-DST-MAC: %lu (48b - 64 aling)", *((uint64_t *) &data[*offset]));
	buf = htobe64(data[*offset]);
	memcpy(&(data_set->records[data_set->header.length]), &buf, 6);
	data_set->header.length += 6;
	(*offset)+=2;
	MSG_NOTICE(msg_str, "\tOUT-SRC-MAC: %lu (48b - 64 aling)", *((uint64_t *) &data[*offset]));
	buf = htobe64(data[*offset]);
	memcpy(&(data_set->records[data_set->header.length]), &buf, 6);
	data_set->header.length += 6;
	(*offset)+=2;

}

//EXTENSION 22 - MPLS (32b ints)
void ext22_parse(uint32_t *data, int *offset, uint8_t flags, struct ipfix_data_set *data_set){
	int i=0;
	uint32_t tmp;
	for(i=0;i<10;i++){
		MSG_NOTICE(msg_str, "\tMPLS-LABEL-%i: %u (32b)",i, *((uint32_t *) &data[*offset+1]));
		tmp = htonl(*((uint32_t *) &data[*offset+1]));
		memcpy(&(data_set->records[data_set->header.length]),&tmp,3);
		data_set->header.length += 3;
		i++;
		MSG_NOTICE(msg_str, "\tMPLS-LABEL-%i: %u (32b)",i, *((uint32_t *) &data[*offset]));
		tmp = htonl(*((uint32_t *) &data[*offset]));
		memcpy(&(data_set->records[data_set->header.length]),&tmp,3);
		data_set->header.length += 3;
		(*offset)+=2;
	}
}

//EXTENSION 23 - Router ipv4
void ext23_parse(uint32_t *data, int *offset, uint8_t flags, struct ipfix_data_set *data_set){
	MSG_NOTICE(msg_str, "\tROUTER-IP: %u (ipv4)", *((uint32_t *) &data[*offset]));
	CONVERT_32();
}


//EXTENSION 24 - Router ipv6
void ext24_parse(uint32_t *data, int *offset, uint8_t flags, struct ipfix_data_set *data_set){
	MSG_NOTICE(msg_str, "\tROUTER-IP: hight:%lu low:%lu (ipv6)",*((uint64_t *) &data[*offset]), \
			*((uint64_t *) &data[(*offset)+8]));
	CONVERT_IPv6();

}

//EXTENSION 25 - Router source id
void ext25_parse(uint32_t *data, int *offset, uint8_t flags, struct ipfix_data_set *data_set){
	MSG_NOTICE(msg_str, "\tROUTER-ID-FILL: %hu ", *((uint16_t *) &data[*offset]));
	*((uint16_t *) &(data_set->records[data_set->header.length])) = htons(*((uint16_t *) &data[*offset])+1);
	data_set->header.length += 2;
	MSG_NOTICE(msg_str, "\tROUTER-ID-ENGINE-TYPE: %hhu ", *(((uint8_t *) &data[*offset])+2));
	MSG_NOTICE(msg_str, "\tROUTER-ID-ENGINE-ID: %hhu ", *(((uint8_t *) &data[*offset])+3));
	*((uint16_t *) &(data_set->records[data_set->header.length])) = *((uint16_t *) &data[*offset])+1;
	data_set->header.length += 2;
	(*offset)++;

}
