/**
 * \file translator.h
 * \author Imrich Stoffa <xstoff02@stud.fit.vutbr.cz>
 * \brief Conversion of IPFIX to LNF format (header file)
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

#ifndef LS_TRANSLATOR_H
#define LS_TRANSLATOR_H

#include <libnf.h>

#define MAX_TABLE 65

struct ipfix_lnf_map;

typedef int (*lnf_data_translator)(uint8_t*, uint16_t*, uint16_t*, uint8_t* ,struct ipfix_lnf_map*);

struct ipfix_lnf_map
{
	uint32_t en;
	uint16_t ie;
	uint8_t lnf_id;
	lnf_data_translator func;	
};

struct ipfix_lnf_map tr_table[MAX_TABLE];

int tr_general(uint8_t *src_data, uint16_t* offset, uint16_t* length, uint8_t* buffer, struct ipfix_lnf_map* item_info);
int tr_address(uint8_t *src_data, uint16_t* offset, uint16_t* length, uint8_t* buffer, struct ipfix_lnf_map* item_info);
int tr_datetime(uint8_t *src_data, uint16_t* offset, uint16_t* length, uint8_t* buffer, struct ipfix_lnf_map* item_info);
int tr_mpls(uint8_t *src_data, uint16_t* offset, uint16_t* length, uint8_t* buffer, struct ipfix_lnf_map* item_info);

int ipfix_lnf_map_compare(const void* pkey, const void* pelem);

uint16_t real_length(uint8_t* src_data, uint16_t* offset, uint16_t length);



#endif //LS_TRANSLATOR_H
