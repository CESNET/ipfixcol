/**
 * \file profiles/filter_wrapper.c
 * \author Imrich Štoffa <xstoff02@stud.fit.vutbr.cz>
 * \brief Wrapper of future intermediate plugin for IPFIX data filtering
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

#define _XOPEN_SOURCE

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ipfixcol.h>
#include <stdint.h>

#include "filter_wrapper.h"
#include "ffilter.h"

#define toGenEnId(gen, en, id) (((uint64_t)gen & 0xffff) << 48 | ((uint64_t)en & 0xffffffff) << 16 | (uint64_t)id)
#define toEnId(en, id) (((uint64_t)en & 0xffffffff) << 16 | (uint64_t)id)

static const char *msg_module = "profiler";

enum generic_ip_id{
	gen_nogeneric = 0,
	gen_ip = 1,
	gen_head_if,
	gen_aggregated
/*	gen_srcip,
	gen_dstip,
	gen_nexthopip,
	gen_bgpnexthop,
	gen_iprouter,
	gen_xlateip,
	gen_xlatesrcip,
	gen_xlatedstip,
*/
};

void loadEnId(uint64_t from, uint16_t *gen, uint32_t* en, uint16_t* id)
{
	*gen = (uint16_t)(from >> 48);
	*en = (uint32_t)(from >> 16);
	*id = (uint16_t)(from);

        return;
}

//Pair field MUST be followed by adjacent fields, map is NULL terminated
struct nff_item_s nff_ipff_map[]={

	//IP records, ip address is general, implicitly set to ipv6
	{"proto", toEnId(0, 4)},

	{"ip", FPAIR},
		{"srcip", toGenEnId(gen_ip, 0, 8)},
		{"dstip", toGenEnId(gen_ip, 0, 12)},
	//synonym of IP
	{"host", FPAIR},
		{"srcip", toGenEnId(gen_ip, 0, 8)},
		{"dstip", toGenEnId(gen_ip, 0, 12)},
/*
	//direct specific mapping
	{"ipv4", FPAIR},
		{"srcipv4", toEnId(0, 8)},
		{"dstipv4", toEnId(0, 12)},
	{"ipv6", FPAIR},
		{"srcipv6", toEnId(0, 27)},
		{"dstipv6", toEnId(0, 28)},
*/
/*
	{"if", FPAIR},
		{"inif", toGenEnId(gen_head_if, 0, 1)},
		{"outif", toGenEnId(gen_head_if, 0, 2)},
*/
	{"port", FPAIR},
		{"srcport", toEnId(0, 7)},
		{"dstport", toEnId(0, 11)},

	{"icmp-type", toEnId(0, 176)},
	{"icmp-code", toEnId(0, 177)},

	{"engine-type", toEnId(0, 38)},
	{"engine-id", toEnId(0, 39)},
/*	{"sysid", toEnId(0, 177)},
*/
	{"icmp-type", toEnId(0, 176)},
	{"icmp-code", toEnId(0, 177)},

	{"srcas", toEnId(0, 16)},
	{"dstas", toEnId(0, 17)},
	{"prevas", toEnId(0, 128)}, //maps  to BGPNEXTADJACENTAS
	{"nextas", toEnId(0, 129)}, //similar as above

	{"mask", FPAIR},
		{"srcmask", toGenEnId(gen_ip, 0, 9)},
		{"dstmask", toGenEnId(gen_ip, 0, 13)},

	{"vlan", FPAIR},
		{"srcvlan", toEnId(0, 58)},
		{"dstvlan", toEnId(0, 59)},

	{"flags", toEnId(0, 6)},

	{"nextip", toGenEnId(gen_ip, 0, 15)},

	{"bgpnextip", toEnId(0, 18)},

	{"routerip", toEnId(0, 130)},
/*
	{"mac", FPAIR},
		{"insrcmac", toEnId(0, 56)},
		{"outdstmac", toEnId(0, 57)},

	{"amac", FPAIR},
		{"outsrcmac", toEnId(0, 80)},
		{"indstmac", toEnId(0, 81)},

	{"inmac", FPAIR},
		{"insrcmac", toEnId(0, 56)},
		{"indstmac", toEnId(0, 81)},

	{"outmac", FPAIR},
		{"outsrcmac", toEnId(0, 80)},
		{"outdstmac", toEnId(0, 57)},
*/
	{"mplslabel1", toEnId(0, 70)},
	{"mplslabel2", toEnId(0, 71)},
	{"mplslabel3", toEnId(0, 72)},
	{"mplslabel4", toEnId(0, 73)},
	{"mplslabel5", toEnId(0, 74)},
	{"mplslabel6", toEnId(0, 75)},
	{"mplslabel7", toEnId(0, 76)},
	{"mplslabel8", toEnId(0, 77)},
	{"mplslabel9", toEnId(0, 78)},
	{"mplslabel10", toEnId(0, 79)},

	{"packets", toEnId(0, 2)},

	{"bytes", toEnId(0, 1)},

	{"flows", toEnId(0, 3)},

	{"tos", toEnId(0, 5)},
	{"dsttos", toEnId(0, 55)},


	{"pps", toGenEnId(gen_aggregated, 0, 5)},

	{"duration", toGenEnId(gen_aggregated, 0, 55)},

	{"bps", toGenEnId(gen_aggregated, 0, 6)},

	{"bpp", toGenEnId(gen_aggregated, 0, 6)},

	{"asaevent", toEnId(0, 230)},
	{"asaxevent", toEnId(0, 233)},

	{"xip", FPAIR},
		{"xsrcip", toGenEnId(gen_ip, 0, 225)},
		{"xdstip", toGenEnId(gen_ip, 0, 226)},

	{"xport", FPAIR},
		{"xsrcport", toEnId(0, 227)},
		{"xdstport", toEnId(0, 228)},

	{"natevent", toEnId(0, 230)},

/*
	{"nip", FPAIR},
		{"nsrcip", toGenEnId(gen_ip, 0, 225)},
		{"ndstip", toGenEnId(gen_ip, 0, 226)},

	{"nport", FPAIR},
		{"nsrcport", toEnId(0, 227)},
		{"ndstport", toEnId(0, 228)},
*/

	{"vrfid", FPAIR},
		{"ingressvrfid", toEnId(0, 234)},
		{"egressvrfid", toEnId(0, 235)},

	{"tstart", toEnId(0, 22)},
	{"tend", toEnId(0, 21)},

	{ NULL, -1},
};

int specify_ipv(uint16_t *i){
	switch(*i)
	{
	//srcip
	case 8: *i = 27; break;
	case 27: *i = 8; break;
	//dstip
	case 12: *i = 28; break;
	case 28: *i = 12; break;
	//mask
	case 9: *i = 13; break;
	case 13: *i = 9; break;
	//nexthopip
	case 15: *i = 62; break;
	case 62: *i = 15; break;
	//bgpnexthop
	case 18: *i = 63; break;
	case 63: *i = 18; break;
	//iprouter
	case 130: *i = 131; break;
	case 131: *i = 130; break;
	//xlatesrcip
	case 225: *i = 281; break;
	case 281: *i = 225; break;
	//xlatedstip
	case 226: *i = 282; break;
	case 282: *i = 226; break;
	default:
		return 0;
	}
	return 1;
}

/* callback from ffilter to lookup field */
ff_error_t ipf_ff_lookup_func(ff_t *filter, const char *fieldstr, ff_lvalue_t *lvalue) {

	/* fieldstr is set - try to find field id and relevant _fget function */

	if (fieldstr != NULL) {

                nff_item_t* item = NULL;
		ipfix_element_t* elem;
		//if((item = bsearch(&fieldstr, &nff_pair_map, 4, sizeof(nff_pair_t), nff_item_comp)) == NULL){
		for(int x = 0; nff_ipff_map[x].name != NULL; x++){
                        if(!strcmp(fieldstr, nff_ipff_map[x].name)){
				item = &nff_ipff_map[x];
				break;
                        }
                }
                if(item == NULL){
                        //potrebujem prekodovat nazov pola na en a id
			const ipfix_element_result_t elemr = get_element_by_name(fieldstr, false);
			if (elemr.result == NULL){
                                return FF_ERR_UNKN;
                        }

			lvalue->id.index = toEnId(elemr.result->en, elemr.result->id);
			lvalue->id2.index = 0;
			elem = &elemr.result;
                } else {
			lvalue->id2.index = 0;

			if(item->en_id != FPAIR){
                                //no pair values
                                lvalue->id.index = item->en_id;
                        } else {
                                //mark lvalue so pair items are used
				lvalue->id.index = (item+1)->en_id;
				lvalue->id2.index = (item+2)->en_id;
                        }
			elem = get_element_by_id(lvalue->id.index >> 16, lvalue->id.index & 0xffff);
                }

                //Rozhodni datovy typ pre filter
		//TODO: solve conflicting types
		switch(elem->type){

                        case ET_OCTET_ARRAY:
                        case ET_UNSIGNED_8:
                        case ET_UNSIGNED_16:
                        case ET_UNSIGNED_32:
                        case ET_UNSIGNED_64:
                                lvalue->type = FF_TYPE_UNSIGNED_BIG;
                                break;

                        case ET_SIGNED_8:
                        case ET_SIGNED_16:
                        case ET_SIGNED_32:
                        case ET_SIGNED_64:
                                lvalue->type = FF_TYPE_SIGNED_BIG;
                                break;

                        case ET_FLOAT_32:
                                return FF_ERR_UNSUP;
                                break;
                        case ET_FLOAT_64:
                                lvalue->type = FF_TYPE_DOUBLE;
                                break;

                        case ET_MAC_ADDRESS:
                                lvalue->type = FF_TYPE_MAC;
                                break;

                        case ET_STRING:
                                lvalue->type = FF_TYPE_STRING;
                                break;

                        case ET_DATE_TIME_MILLISECONDS:
				lvalue->type = FF_TYPE_TIMESTAMP;
                                break;
                        case ET_DATE_TIME_SECONDS:
                        case ET_DATE_TIME_MICROSECONDS:
                        case ET_DATE_TIME_NANOSECONDS:
				return FF_ERR_UNSUP;
                                break;

                        case ET_IPV4_ADDRESS:
                        case ET_IPV6_ADDRESS:
                                lvalue->type = FF_TYPE_ADDR;
                                break;

                        case ET_BASIC_LIST:
                        case ET_SUB_TEMPLATE_LIST:
                        case ET_SUB_TEMPLATE_MULTILIST:
                        case ET_BOOLEAN:
                        case ET_UNASSIGNED:
                        default:
                                return FF_ERR_UNSUP;
                }
                return FF_OK;
        }
        return FF_ERR_OTHER;
}


/* getting data callback */
ff_error_t ipf_ff_data_func(ff_t *filter, void *rec, ff_extern_id_t id, char *data, size_t *size) {
        //assuming rec is struct ipfix_message
        struct ipfix_record* ipf_rec = rec;
        uint32_t en;
	uint16_t ie_id;
	uint16_t generic_set;
        int len;
	loadEnId(id.index, &generic_set, &en, &ie_id);

	if(generic_set == gen_head_if){
		//dig through header data
		//proceed directly to return
		return FF_ERR_OTHER;
	}else if(generic_set == gen_aggregated){
		//godlike code to handle this mess
		return FF_ERR_OTHER;
	}

	data = data_record_get_field(ipf_rec->record, ipf_rec->templ, en, ie_id, &len);
	if (data == NULL && generic_set == gen_ip) {
		if (specify_ipv(&ie_id)) {
			data = data_record_get_field(ipf_rec->record, ipf_rec->templ, en, ie_id, &len);
		}
	}
        if(data == NULL){
            return FF_ERR_OTHER;
        }

        *size = len;
        return FF_OK;
}

/**
 * \brief Check whether value in data record fits with node expression
 *
 * \param[in] node Filter tree node
 * \param[in] msg IPFIX message (filter may contain field from message header)
 * \param[in] record IPFIX data record
 * \return 1 if data record's field fits -1 on error
 */
int filter_eval_node(struct filter_profile *pdata, struct ipfix_message *msg, struct ipfix_record *record)
{
        return ff_eval(pdata->filter, record->record);
}

void filter_free_profile(struct filter_profile *profile){
        ff_free(profile->filter);
        free(profile);
}
