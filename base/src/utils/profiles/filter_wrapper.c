/**
 * \file profiles/filter.c
 * \author Michal Kozubik <kozubik@cesnet.cz>
 * \brief Intermediate plugin for IPFIX data filtering
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

#include <arpa/inet.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <ipfixcol.h>

#include "filter_wrapper.h"

static const char *msg_module = "profiler";

/* Check if IPv4 is mapped inside IPv6 */
// If it is possible, replace with standard IN6_IS_ADDR_V4MAPPED()
# define IS_V4_IN_V6(a) \
	((((const uint32_t *) (a))[0] == 0)      \
	&& (((const uint32_t *) (a))[1] == 0)    \
	&& (((const uint32_t *) (a))[2] == htonl (0xffff)))

#define toEnId (en, id) (uint64_t(en) << 16 | id )

void loadEnId(uint64_t from, uint32_t* en, uint16_t* id){
        *en = from >> 16;
        *id = from & 0xffff;
        return;
}

typedef struct nff_item_s {
        const char* name;
        uint64_t en_id;
        struct pair_item*; //Provides link to additional info for paired elements
}nff_item_t;

typedef struct nff_pair_s {
        //const char* name;
        const char* p1;
        const char* p2;
        uint64_t p1_en_id;
        uint64_t p2_en_id;
}nff_pair_t;

struct nff_pair_s nff_pair_map[]{
        { "srcip", "dstip", toEnId(0, 27), toEnId(0, 12)},
        { "srcport","dstport", toEnId(0, 7), toEnId(0, 11)},
        //{ , , , },
};

struct nff_item_s nff_ipff_map[]{
        {"bytes", toEnId(0, 1), NULL},
        {"ip", -1, &nff_pair_map[0]},
        {"packets", toEnId(0, 2), NULL},
        {"port", -1, &nff_pair_map[1]},
        { NULL, -1, NULL },
        //{ , , },
};

int nff_item_comp(nff_item_t* key, nff_item_t* elem)
{
        return strcmp(key->name, elem->name);
}


/* callback from ffilter to lookup field */
ff_error_t ipf_ff_lookup_func(ff_t *filter, const char *fieldstr, ff_lvalue_t *lvalue) {

        /* fieldstr is set - try to find field id and relevant _fget function */

        //zkus se zatím zaměřit na "bytes", "packets", "ip" (src/dst), "port" (src/dst)... pak to kdyžtak rozšíříme.. ok?
        if ( fieldstr != NULL ) {

                nff_item_t* item = NULL;
                ipfix_element_result_t elem;
                //if((item = bsearch(&filedstr, &nff_pair_map, 4, sizeof(nff_pair_t), nff_item_comp)) == NULL){
                for(x = 0; nff_ipff_map[x].name != NULL; x++){
                        if(!strcmp(fieldstr, nff_ipff_map[x].name)){
                            item = &nff_ipff_map[x];
                            break;
                        }
                }
                if(item == NULL){
                        //potrebujem prekodovat nazov pola na en a id
                        elem = element_get_by_name(filedstr, false);
                        if (elem.result == NULL){
                                return FF_ERR_UNKN;
                        }

                        lvalue->id.index = toEnId(elem.result->en, elem.result->id);
                        lvalue->id2.index = NULL;

                } else {
                        lvalue->id2.index = NULL;

                        if(item->pair_item == NULL){
                                //no pair values
                                lvalue->id.index = item->en_id;
                        } else {
                                //mark lvalue so pair items are used
                                lvalue->id.index = item->pair_item->p1_en_id;
                                lvalue->id2.index = item->pair_item->p2_en_id;
                        }
                        elem = element_get_by_id(lvalue->id.index >> 16, lvalue->id.index & 0xffff);
                }

                //Rozhodni datovy typ pre filter
                switch(elem->result.type){

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
                                return FF_UNSUP_ERR;
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
        uint16_t id;
        int len;
        loadEnId(id.index, &en, &id);

        data = data_record_get_field(ipf_rec->record, ipf_rec->templ, en, id, &len);

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
int filter_eval_node(filter_profile *pdata, struct ipfix_message *msg, struct ipfix_record *record)
{
        return ff_eval(pdata->filter, record->record);
}

//TODO: verify that this is correct
void filter_free_profile(filter_profile *profile){
        ff_free(profile->filter);
        free(profile);
}
