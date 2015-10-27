/**
 * \file ext_fill.c
 * \brief nfdump input plugin - filling templates.
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
#include "ext_fill.h"
#include "nffile.h"

static const char *msg_module = "nfdump_input";

//EXTENSION 0 -- not a real extension its just pading ect
void ext0_fill_tm(uint16_t flags, struct ipfix_template * template){
	MSG_INFO(msg_module, "ZERO EXTENSION");
}

//EXTENSION 1
void ext1_fill_tm(uint16_t flags, struct ipfix_template * template){
	if(TestFlag(flags, FLAG_IPV6_ADDR)){
		//sourceIPv6Address
		template->fields[template->field_count].ie.id = 27;
		template->fields[template->field_count].ie.length = 16;
		template->field_count++;
		template->data_length += 16;
		//destinationIPv6Address
		template->fields[template->field_count].ie.id = 28;
		template->fields[template->field_count].ie.length = 16;
		template->field_count++;
		template->data_length += 16;
	} else {
		//sourceIPv4Address
		template->fields[template->field_count].ie.id = 8;
		template->fields[template->field_count].ie.length = 4;
		template->field_count++;
		template->data_length += 4;
		//destinationIPv4Address
		template->fields[template->field_count].ie.id = 12;
		template->fields[template->field_count].ie.length = 4;
		template->field_count++;
		template->data_length += 4;
	}
	template->template_length += 8;
}

//EXTENSION 2
void ext2_fill_tm(uint16_t flags, struct ipfix_template * template){
	//packetDeltaCount
	template->fields[template->field_count].ie.id = 2;
	template->fields[template->field_count].ie.length = 8;
	template->field_count++;
	template->data_length += 8;
	template->template_length += 4;
}

//EXTENSION 3
void ext3_fill_tm(uint16_t flags, struct ipfix_template * template){
	//byteDeltaCount
	template->fields[template->field_count].ie.id = 1;
	template->fields[template->field_count].ie.length = 8;
	template->field_count++;
	template->data_length += 8;
	template->template_length += 4;
}

//OPTIONAL EXTENSIONS
//EXTENSION 4 - interface record (16b ints)
void ext4_fill_tm(uint16_t flags, struct ipfix_template * template){
	template->fields[template->field_count].ie.id = 10;
	template->fields[template->field_count].ie.length = 2;
	template->field_count++;
	template->data_length += 2;
	template->fields[template->field_count].ie.id = 14;
	template->fields[template->field_count].ie.length = 2;
	template->field_count++;
	template->data_length += 2;
	template->template_length += 8;
}

//EXTENSION 5 - interface record (32b ints)
void ext5_fill_tm(uint16_t flags, struct ipfix_template * template){
	template->fields[template->field_count].ie.id = 10;
	template->fields[template->field_count].ie.length = 4;
	template->field_count++;
	template->data_length += 4;
	template->fields[template->field_count].ie.id = 14;
	template->fields[template->field_count].ie.length = 4;
	template->field_count++;
	template->data_length += 4;
	template->template_length += 8;
}

//OPTIONAL EXTENSIONS
//EXTENSION 6 - AS record (16b ints)
void ext6_fill_tm(uint16_t flags, struct ipfix_template * template){
	template->fields[template->field_count].ie.id = 16;
	template->fields[template->field_count].ie.length = 2;
	template->field_count++;
	template->data_length += 2;
	template->fields[template->field_count].ie.id = 17;
	template->fields[template->field_count].ie.length = 2;
	template->field_count++;
	template->data_length += 2;
	template->template_length += 8;

}

//EXTENSION 7 - AS record (32b ints)
void ext7_fill_tm(uint16_t flags, struct ipfix_template * template){
	template->fields[template->field_count].ie.id = 16;
	template->fields[template->field_count].ie.length = 4;
	template->field_count++;
	template->data_length += 4;
	template->fields[template->field_count].ie.id = 17;
	template->fields[template->field_count].ie.length = 4;
	template->field_count++;
	template->data_length += 4;
	template->template_length += 8;

}

//EXTENSION 8 - dst tos, dir, srcmask, dstmask in one 32b int
void ext8_fill_tm(uint16_t flags, struct ipfix_template * template){
	template->fields[template->field_count].ie.id = 55;
	template->fields[template->field_count].ie.length = 1;
	template->field_count++;
	template->data_length += 1;
	template->fields[template->field_count].ie.id = 61;
	template->fields[template->field_count].ie.length = 1;
	template->field_count++;
	template->data_length += 1;

	if(TestFlag(flags, FLAG_IPV6_ADDR)){
		template->fields[template->field_count].ie.id = 29;
		template->fields[template->field_count].ie.length = 1;
		template->field_count++;
		template->data_length += 1;
		template->fields[template->field_count].ie.id = 30;
		template->fields[template->field_count].ie.length = 1;
		template->field_count++;
		template->data_length += 1;
	} else {
		template->fields[template->field_count].ie.id = 9;
		template->fields[template->field_count].ie.length = 1;
		template->field_count++;
		template->data_length += 1;
		template->fields[template->field_count].ie.id = 13;
		template->fields[template->field_count].ie.length = 1;
		template->field_count++;
		template->data_length += 1;
	}
	template->template_length += 16;
}

//EXTENSION 9 - next hop ipv4
void ext9_fill_tm(uint16_t flags, struct ipfix_template * template){
	template->fields[template->field_count].ie.id = 15;
	template->fields[template->field_count].ie.length = 4;
	template->field_count++;
	template->data_length += 4;
	template->template_length += 4;
}


//EXTENSION 10 - next hop ipv6
void ext10_fill_tm(uint16_t flags, struct ipfix_template * template){
	template->fields[template->field_count].ie.id = 62;
	template->fields[template->field_count].ie.length = 16;
	template->field_count++;
	template->data_length += 16;
	template->template_length += 4;
}


//EXTENSION 11 - BGP next hop ipv4
void ext11_fill_tm(uint16_t flags, struct ipfix_template * template){
	template->fields[template->field_count].ie.id = 18;
	template->fields[template->field_count].ie.length = 4;
	template->field_count++;
	template->data_length += 4;
	template->template_length += 4;

}


//EXTENSION 12 - BGP next hop ipv6
void ext12_fill_tm(uint16_t flags, struct ipfix_template * template){
	template->fields[template->field_count].ie.id = 63;
	template->fields[template->field_count].ie.length = 16;
	template->field_count++;
	template->data_length += 16;
	template->template_length += 4;
}

//EXTENSION 13 - VLAN record (16b ints)
void ext13_fill_tm(uint16_t flags, struct ipfix_template * template){
	template->fields[template->field_count].ie.id = 58;
	template->fields[template->field_count].ie.length = 2;
	template->field_count++;
	template->data_length += 2;
	template->fields[template->field_count].ie.id = 59;
	template->fields[template->field_count].ie.length = 2;
	template->field_count++;
	template->data_length += 2;
	template->template_length += 8;
}

//EXTENSION 14 - Out packet count (32b int)
void ext14_fill_tm(uint16_t flags, struct ipfix_template * template){
	template->fields[template->field_count].ie.id = 24;
	template->fields[template->field_count].ie.length = 4;
	template->field_count++;
	template->data_length += 4;
	template->template_length += 4;
}

//EXTENSION 15 - Out packet count (64b int)
void ext15_fill_tm(uint16_t flags, struct ipfix_template * template){
	template->fields[template->field_count].ie.id = 24;
	template->fields[template->field_count].ie.length = 8;
	template->field_count++;
	template->data_length += 8;
	template->template_length += 4;
}

//EXTENSION 16 - Out bytes count (32b int)
void ext16_fill_tm(uint16_t flags, struct ipfix_template * template){
	template->fields[template->field_count].ie.id = 23;
	template->fields[template->field_count].ie.length = 4;
	template->field_count++;
	template->data_length += 4;
	template->template_length += 4;
}

//EXTENSION 17 - Out bytes count (64b int)
void ext17_fill_tm(uint16_t flags, struct ipfix_template * template){
	template->fields[template->field_count].ie.id = 23;
	template->fields[template->field_count].ie.length = 8;
	template->field_count++;
	template->data_length += 8;
	template->template_length += 4;

}

//EXTENSION 18 - Aggr flows (32b int)
void ext18_fill_tm(uint16_t flags, struct ipfix_template * template){
	template->fields[template->field_count].ie.id = 3;
	template->fields[template->field_count].ie.length = 4;
	template->field_count++;
	template->data_length += 4;
	template->template_length += 4;

}

//EXTENSION 19 - Aggr flows (64b int)
void ext19_fill_tm(uint16_t flags, struct ipfix_template * template){
	template->fields[template->field_count].ie.id = 3;
	template->fields[template->field_count].ie.length = 8;
	template->field_count++;
	template->data_length += 8;
	template->template_length += 4;

}

//EXTENSION 20 - in src mac, out dst mac (64b int)
void ext20_fill_tm(uint16_t flags, struct ipfix_template * template){
	template->fields[template->field_count].ie.id = 56;
	template->fields[template->field_count].ie.length = 6;
	template->field_count++;
	template->data_length += 6;
	template->fields[template->field_count].ie.id = 57;
	template->fields[template->field_count].ie.length = 6;
	template->field_count++;
	template->data_length += 6;
	template->template_length += 8;

}

//EXTENSION 21 - in dst mac, out src mac (64b int)
void ext21_fill_tm(uint16_t flags, struct ipfix_template * template){
	template->fields[template->field_count].ie.id = 80;
	template->fields[template->field_count].ie.length = 6;
	template->field_count++;
	template->data_length += 6;
	template->fields[template->field_count].ie.id = 81;
	template->fields[template->field_count].ie.length = 6;
	template->field_count++;
	template->data_length += 6;
	template->template_length += 8;
}

//EXTENSION 22 - MPLS (32b ints)
void ext22_fill_tm(uint16_t flags, struct ipfix_template * template){
	int i;
	for(i=0;i<10;i++){
		template->fields[template->field_count].ie.id = 70 + i;
		template->fields[template->field_count].ie.length = 3;
		template->field_count++;
		template->data_length += 3;
	}
	template->template_length += 40;
}

//EXTENSION 23 - Router ipv4
void ext23_fill_tm(uint16_t flags, struct ipfix_template * template){
	MSG_WARNING(msg_module, "There is no element for router IP address (this extension is ignored)");
}


//EXTENSION 24 - Router ipv6
void ext24_fill_tm(uint16_t flags, struct ipfix_template * template){
	MSG_WARNING(msg_module, "There is no element for router IP address (this extension is ignored)");
}

//EXTENSION 25 - Router source id
void ext25_fill_tm(uint16_t flags, struct ipfix_template * template){
	MSG_INFO(msg_module, "There is no element for router source ID (filled as reserved 38 and 39 elements)");
	template->fields[template->field_count].ie.id = 38;
	template->fields[template->field_count].ie.length = 1;
	template->field_count++;
	template->data_length += 1;
	template->fields[template->field_count].ie.id = 39;
	template->fields[template->field_count].ie.length = 1;
	template->field_count++;
	template->data_length += 1;
	template->template_length += 8;
}

//EXTENSION 26 - BGP previous/next adjacent AS
void ext26_fill_tm(uint16_t flags, struct ipfix_template * template){
	// Next AS
	template->fields[template->field_count].ie.id = 128;
	template->fields[template->field_count].ie.length = 4;
	template->field_count++;
	template->data_length += 4;
	// Previous AS
	template->fields[template->field_count].ie.id = 129;
	template->fields[template->field_count].ie.length = 4;
	template->field_count++;
	template->data_length += 4;
	template->template_length += 8;
}

//EXTENSION 27 - Time flow received [ms] i.e. collectionTimeMiliseconds
void ext27_fill_tm(uint16_t flags, struct ipfix_template * template){
	template->fields[template->field_count].ie.id = 258;
	template->fields[template->field_count].ie.length = 8;
	template->field_count++;
	template->data_length += 8;
	template->template_length += 4;
}
