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
#include <strings.h>

#include <ipfixcol.h>
#include <stdint.h>

#include "filter_wrapper.h"
#include "ffilter.h"
#include "literals.h"

#define toGenEnId(gen, en, id) (((uint64_t)gen & 0xffff) << 48 |\
    ((uint64_t)en & 0xffffffff) << 16 | (uint16_t)id)

#define toEnId(en, id) (((uint64_t)en & 0xffffffff) << 16 | (uint16_t)id)

/* Check if IPv4 is mapped inside IPv6 */
// If it is possible, replace with standard IN6_IS_ADDR_V4MAPPED()
# define IS_V4_IN_V6(a) \
	((((const uint32_t *) (a))[0] == 0)      \
	&& (((const uint32_t *) (a))[1] == 0)    \
	&& (((const uint32_t *) (a))[2] == htonl (0xffff)))

static const char *msg_module = "ipx_filter";

enum nff_control_e {
    CTL_NA = 0x00,
    CTL_V4V6IP = 0x01,
    CTL_HEADER_ITEM = 0x02,
    CTL_CALCULATED_ITEM = 0x04,
    CTL_FLAGS = 0x08,
    CTL_CONST_ITEM = 0x10,
    CTL_FPAIR = 0x8000,
};

enum nff_calculated_e {
    CALC_PPS = 1,
    CALC_DURATION,
    CALC_BPS,
    CALC_BPP,
    CALC_MPLS,
    CALC_MPLS_EOS,
    CALC_MPLS_EXP
};

enum nff_header_e {
    HD_ODID = 1,
    HD_SRCADDR,
    HD_DSTADDR,
    HD_SRCPORT,
    HD_DSTPORT
};

enum nff_constant_e {
    CONST_INET = 0,
    CONST_INET6,
    CONST_END
};

const char constants[10][CONST_END] = {
    [CONST_INET] = {"4"},
    [CONST_INET6] = {"6"},
};

struct ipx_filter {
    ff_t *filter;	//internal filter representation
    void* buffer;	//buffer
};

/**
 * \brief Structure of ipfix message and record pointers
 *
 * Used to pass ipfix_message and record as one argument
 * Needed due to char* parameter in ff_data_func_t type
 */
typedef struct nff_msg_rec_s {
    struct ipfix_message* msg;
    struct ipfix_record* rec;
} nff_msg_rec_t;

/**
 * \struct nff_item_s
 * \brief Data structure that holds extra keywords and their numerical synonyms (some with extra flags)
 *
 * Pair field MUST be followed by adjacent fields, map is NULL terminated !
 */
typedef struct nff_item_s {
    const char* name;
    union {
        uint64_t en_id;
        uint64_t data;
    };
}nff_item_t;

/* To be done
typedef struct nff_item2_s {
    const char* name;
    nff_control_e flags;
    uint32_t en;
    uint16_t id;
    struct nff_item2_s *pair1;
    struct nff_item2_s *pair2;
}nff_item2_t;
*/

void unpackEnId(uint64_t from, uint16_t *gen, uint32_t* en, uint16_t* id)
{
    *gen = (uint16_t)(from >> 48);
    *en = (uint32_t)(from >> 16);
    *id = (uint16_t)(from);

    return;
}

/* This map of strings and ids determines which (hopefully) synonyms of nfdump filter keywords are supported */
static struct nff_item_s nff_ipff_map[] = {
    /* items contains name as inputted to filter and mapping to iana ipfix enterprise and element_id */
    {"odid",          {toGenEnId(CTL_HEADER_ITEM, 0, HD_ODID)}},
    {"exporterip",    {toGenEnId(CTL_HEADER_ITEM, 0, HD_SRCADDR)}},
    {"collectorip",   {toGenEnId(CTL_HEADER_ITEM, 0, HD_DSTADDR)}},
    {"exporterport",  {toGenEnId(CTL_HEADER_ITEM, 0, HD_SRCPORT)}},
    {"collectorport", {toGenEnId(CTL_HEADER_ITEM, 0, HD_DSTPORT)}},

    {"inet",      {toGenEnId(CTL_CONST_ITEM, 60, CONST_INET)}},
    {"inet6",     {toGenEnId(CTL_CONST_ITEM, 60, CONST_INET6)}},
    {"ipv4",      {toGenEnId(CTL_CONST_ITEM, 60, CONST_INET)}},
    {"ipv6",      {toGenEnId(CTL_CONST_ITEM, 60, CONST_INET6)}},

    {"proto",     {toEnId(0, 4)}},
    {"first",     {toEnId(0, 152)}},
    {"last",      {toEnId(0, 153)}},

    /* for functionality reasons there are extra flags in mapping part CTL_FPAIR
     * stands for item that maps to two other elements and mapping contain
     * offsets relative to itself where target items lie in map*/
    {"ip",        {toGenEnId(CTL_FPAIR, 1, 2)}},

    /* CTL_V4V6IP flag allows filter to try to swtch to another equivalent field
     * when IPv4 item is not present in flow */
    {"srcip",     {toGenEnId(CTL_V4V6IP, 0, 8)}},
    {"dstip",     {toGenEnId(CTL_V4V6IP, 0, 12)}},

    //synonym of IP
    {"net",       {toGenEnId(CTL_FPAIR, 1, 2)}},
    {"srcnet",    {toGenEnId(CTL_V4V6IP, 0, 8)}},
    {"dstnet",    {toGenEnId(CTL_V4V6IP, 0, 12)}},
    //synonym of IP
    {"host",      {toGenEnId(CTL_FPAIR, 1, 2)}},
    {"srchost",   {toGenEnId(CTL_V4V6IP, 0, 8)}},
    {"dsthost",   {toGenEnId(CTL_V4V6IP, 0, 12)}},

    {"mask",      {toGenEnId(CTL_FPAIR, 1, 2)}},
    {"srcmask",   {toGenEnId(CTL_V4V6IP, 0, 9)}},
    {"dstmask",   {toGenEnId(CTL_V4V6IP, 0, 13)}},

    //Direct specific mapping for IP src/dst ips
    {"ipv4",      {toGenEnId(CTL_FPAIR, 1, 2)}},
    {"srcipv4",   {toEnId(0, 8)}},
    {"dstipv4",   {toEnId(0, 12)}},
    {"ipv6",      {toGenEnId(CTL_FPAIR, 1, 2)}},
    {"srcipv6",   {toEnId(0, 27)}},
    {"dstipv6",   {toEnId(0, 28)}},

    {"if",        {toGenEnId(CTL_FPAIR, 1, 2)}},
    {"inif",      {toEnId(0, 10)}},
    {"outif",     {toEnId(0, 14)}},

    {"port",      {toGenEnId(CTL_FPAIR, 1, 2)}},
    {"srcport",   {toEnId(0, 7)}},
    {"dstport",   {toEnId(0, 11)}},

    {"icmp-type", {toEnId(0, 176)}},
    {"icmp-code", {toEnId(0, 177)}},

    {"engine-type", {toEnId(0, 38)}},
    {"engine-id",   {toEnId(0, 39)}},
//	{"sysid", toEnId(0, 177)},

    {"as",        {toGenEnId(CTL_FPAIR, 1, 2)}},
    {"srcas",     {toEnId(0, 16)}},
    {"dstas",     {toEnId(0, 17)}},

    {"nextas",    {toEnId(0, 128)}}, //maps  to BGPNEXTADJACENTAS
    {"prevas",    {toEnId(0, 129)}}, //similar as above


    {"vlan",      {toGenEnId(CTL_FPAIR, 1, 2)}},
    {"srcvlan",   {toEnId(0, 58)}},
    {"dstvlan",   {toEnId(0, 59)}},
    /* CTL_FLAGS Marks this to be evaluated like flag in case no operator
     * is supplied */
    {"flags",     {toGenEnId(CTL_FLAGS, 0, 6)}},

    {"nextip",    {toGenEnId(CTL_V4V6IP, 0, 15)}},

    {"bgpnextip", {toGenEnId(CTL_V4V6IP, 0, 18)}},

    {"routerip",  {toEnId(0, 130)}},

    {"mac",       {toGenEnId(CTL_FPAIR, 1, 2)}},
    {"inmac",     {toGenEnId(CTL_FPAIR, 4, 5)}},
    {"outmac",    {toGenEnId(CTL_FPAIR, 5, 6)}},
    {"srcmac",    {toGenEnId(CTL_FPAIR, 2, 4)}},
    {"dstmac",    {toGenEnId(CTL_FPAIR, 2, 4)}},
    {"insrcmac",  {toEnId(0, 56)}},
    {"indstmac",  {toEnId(0, 80)}},
    {"outsrcmac", {toEnId(0, 81)}},
    {"outdstmac", {toEnId(0, 57)}},


    {"mplslabel1",  {toGenEnId(CTL_CALCULATED_ITEM, 70, CALC_MPLS)}},
    {"mplslabel2",  {toGenEnId(CTL_CALCULATED_ITEM, 71, CALC_MPLS)}},
    {"mplslabel3",  {toGenEnId(CTL_CALCULATED_ITEM, 72, CALC_MPLS)}},
    {"mplslabel4",  {toGenEnId(CTL_CALCULATED_ITEM, 73, CALC_MPLS)}},
    {"mplslabel5",  {toGenEnId(CTL_CALCULATED_ITEM, 74, CALC_MPLS)}},
    {"mplslabel6",  {toGenEnId(CTL_CALCULATED_ITEM, 75, CALC_MPLS)}},
    {"mplslabel7",  {toGenEnId(CTL_CALCULATED_ITEM, 76, CALC_MPLS)}},
    {"mplslabel8",  {toGenEnId(CTL_CALCULATED_ITEM, 77, CALC_MPLS)}},
    {"mplslabel9",  {toGenEnId(CTL_CALCULATED_ITEM, 78, CALC_MPLS)}},
    {"mplslabel10", {toGenEnId(CTL_CALCULATED_ITEM, 79, CALC_MPLS)}},

    {"mplsexp",   {toGenEnId(CTL_CALCULATED_ITEM, 0, CALC_MPLS_EXP)}},
    {"mplseos",   {toGenEnId(CTL_CALCULATED_ITEM, 0, CALC_MPLS_EOS)}},

    {"packets",   {toEnId(0, 2)}},

    {"bytes",     {toEnId(0, 1)}},

    {"flows",     {toEnId(0, 3)}},

    {"tos",       {toEnId(0, 5)}},
    {"srctos",    {toEnId(0, 5)}},
    {"dsttos",    {toEnId(0, 55)}},

    /* CTL_CALCULATED_ITEM marks specific elements, enumerated ie_id mappings
     * are for calculated virtual fields */
    {"pps",       {toGenEnId(CTL_CALCULATED_ITEM, 0, CALC_PPS)}},

    {"duration",  {toGenEnId(CTL_CALCULATED_ITEM, 0, CALC_DURATION)}},

    {"bps",       {toGenEnId(CTL_CALCULATED_ITEM, 0, CALC_BPS)}},

    {"bpp",       {toGenEnId(CTL_CALCULATED_ITEM, 0, CALC_BPP)}},

//Not verified, for
//	{"asa event", toEnId(0, 230)},
//	{"asa xevent", toEnId(0, 233)},
/*
	{"xip", toGenEnId(CTL_FPAIR, 1, 2)},
		{"src xip", toGenEnId(CTL_V4V6IP, 0, 225)},
		{"dst xip", toGenEnId(CTL_V4V6IP, 0, 226)},

	{"xport", toGenEnId(CTL_FPAIR, 1, 2)},
		{"src xport", toEnId(0, 227)},
		{"dst xport", toEnId(0, 228)},
*/
    {"natevent",  {toEnId(0, 230)}},

    {"nip",       {toGenEnId(CTL_FPAIR, 1, 2)}},
    {"srcnip",    {toGenEnId(CTL_V4V6IP, 0, 225)}},
    {"dstnip",    {toGenEnId(CTL_V4V6IP, 0, 226)}},

    {"nport",     {toGenEnId(CTL_FPAIR, 1, 2)}},
    {"srcnport",  {toEnId(0, 227)}},
    {"dstnport",  {toEnId(0, 228)}},

    {"vrfid",     {toGenEnId(CTL_FPAIR, 1, 2)}},
    {"ingressvrfid", {toEnId(0, 234)}},
    {"egressvrfid",  {toEnId(0, 235)}},

    //{"tstart", toEnId(0, 152)},
    //{"tend", toEnId(0, 153)},

    /* Array is null terminated */
    { NULL, {0U}},
};


/**
 * \brief specify_ipv switches ipfix information element to equivalent from ipv4
 * to ipv6 and vice versa
 *
 * \param i Element Id
 * \return Nonzero on success
 */
int specify_ipv(uint16_t *i)
{
    switch(*i)
    {
        //src ip
    case 8: *i = 27; break;
    case 27: *i = 8; break;
        //dst ip
    case 12: *i = 28; break;
    case 28: *i = 12; break;
        //src mask
    case 9: *i = 29; break;
    case 29: *i = 9; break;
        //dst mask
    case 13: *i = 30; break;
    case 30: *i = 13; break;
        //nexthop ip
    case 15: *i = 62; break;
    case 62: *i = 15; break;
        //bgpnext ip
    case 18: *i = 63; break;
    case 63: *i = 18; break;
        //router ip
    case 130: *i = 131; break;
    case 131: *i = 130; break;
        //src xlate ip
    case 225: *i = 281; break;
    case 281: *i = 225; break;
        //dst xlate ip
    case 226: *i = 282; break;
    case 282: *i = 226; break;
    default:
        return 0;
    }
    return 1;
}

/**
 * \brief Get a value of an unsigned integer (stored in big endian order a.k.a.
 *   network byte order)
 *
 * The \p value is read from a data \p field and converted from
 * the appropriate byte order to host byte order.
 * \param[in]  field  Pointer to the data field (in "network byte order")
 * \param[in]  size   Size of the data field (min: 1 byte, max: 8 bytes)
 * \param[out] value  Pointer to a variable for the result
 * \return On success returns 0 and fills the \p value.
 *   Otherwise (usually the incorrect \p size of the field) returns
 *   a non-zero value and the \p value is not filled.
 */
static inline int
convert_uint_be(const void *field, size_t size, uint64_t *value)
{
    switch (size) {
    case 8:
        *value = be64toh(*(const uint64_t *) field);
        return 0;

    case 4:
        *value = ntohl(*(const uint32_t *) field);
        return 0;

    case 2:
        *value = ntohs(*(const uint16_t *) field);
        return 0;

    case 1:
        *value = *(const uint8_t *) field;
        return 0;

    default:
        // Other sizes (3,5,6,7)
        break;
    }

    if (size == 0 || size > 8) {
        return 1;
    }

    uint64_t new_value = 0;
    memcpy(&(((uint8_t *) &new_value)[8U - size]), field, size);

    *value = be64toh(new_value);
    return 0;
}

/**
 * \brief Get a unsigned integer value of a field
 * \param[in]  record Recodd
 * \param[in]  templ  Template
 * \param[in]  id     Field ID
 * \param[out] res    Result value (filled only on success)
 * \return On success returns 0. Otherwise returns a non-zero value.
 */
static inline int
get_unsigned(uint8_t *record, struct ipfix_template *templ, uint16_t id,
    ff_uint64_t *res)
{
    uint8_t *data_ptr;
    int data_len;

    data_ptr = data_record_get_field(record, templ, 0, id, &data_len);
    if (!data_ptr) {
        return 1;
    }

    const size_t size = (size_t) data_len;
    return (convert_uint_be(data_ptr, size, res) == 0) ? 0 : 1;
}

/**
 * \brief Type of timestamp
 */
enum datetime {
    /**
     * The type represents a time value expressed with second-level precision.
     */
    DATETIME_SECONDS,
    /**
     * The type represents a time value expressed with millisecond-level
     * precision.
     */
    DATETIME_MILLISECONDS,
    /**
     * The type represents a time value expressed with microsecond-level
     * precision.
     */
    DATETIME_MICROSECONDS,
    /**
     * The type represents a time value expressed with nanosecond-level
     * precision.
     */
    DATETIME_NANOSECONDS,
};

/**
 * \def IPX_CONVERT_EPOCHS_DIFF
 * \brief Time difference between NTP and UNIX epoch in seconds
 *
 * NTP epoch (1 January 1900, 00:00h) vs. UNIX epoch (1 January 1970 00:00h)
 * i.e. ((70 years * 365 days) + 17 leap-years) * 86400 seconds per day
 */
#define IPX_CONVERT_EPOCHS_DIFF (2208988800ULL)

/**
 * \brief Get a value of a low precision timestamp (stored in big endian order
 *   a.k.a. network byte order)
 *
 * The \p value is read from a data \p field, converted from
 * "network byte order" to host byte order and transformed to a corresponding
 * data type.
 * \param[in]  field  Pointer to the data field (in "network byte order")
 * \param[in]  size   Size of the data field (in bytes)
 * \param[in]  type   Type of the timestamp (see the remark)
 * \param[out] value  Pointer to a variable for the result (Number of
 *   milliseconds since the UNIX epoch)
 * \remark The parameter \p type can be only one of the following types:
 *   #DATETIME_SECONDS, #DATETIME_MILLISECONDS, #DATETIME_MICROSECONDS,
 *   #DATETIME_NANOSECONDS
 * \warning The \p size of the \p field MUST be 4 bytes
 *   (#DATETIME_SECONDS) or 8 bytes (#DATETIME_MILLISECONDS,
 *   #DATETIME_MICROSECONDS, #DATETIME_NANOSECONDS)
 * \warning Wraparound for dates after 8 February 2036 is not implemented.
 * \return On success returns 0 and fills the \p value.
 *   Otherwise (usually the incorrect \p size of the field) returns
 *   a non-zero value and the \p value is not filled.
 */
static inline int
convert_datetime_lp_be(const void *field, size_t size, enum datetime type,
    uint64_t *value)
{
    // One second to milliseconds
    const uint64_t S1E3 = 1000ULL;

    if ((size != sizeof(uint64_t) || type == DATETIME_SECONDS)
        && (size != sizeof(uint32_t) || type != DATETIME_SECONDS)) {
        return 1;
    }

    switch (type) {
    case DATETIME_SECONDS:
        *value = ntohl(*(const uint32_t *) field) * S1E3;
        return 0;

    case DATETIME_MILLISECONDS:
        *value = be64toh(*(const uint64_t *) field);
        return 0;

    case DATETIME_MICROSECONDS:
    case DATETIME_NANOSECONDS: {
        // Conversion from NTP 64bit timestamp to UNIX timestamp
        const uint32_t (*parts)[2] = (const uint32_t (*)[2]) field;
        uint64_t result;

        // Seconds
        result = (ntohl((*parts)[0]) - IPX_CONVERT_EPOCHS_DIFF) * S1E3;

        /*
         * Fraction of second (1 / 2^32)
         * The "value" uses 1/1000 sec as unit of subsecond fractions and NTP
         * uses 1/(2^32) sec as its unit of fractional time.
         * Conversion is easy: First, multiply by 1e3, then divide by 2^32.
         * Warning: Calculation must use 64bit variable!!!
         */
        uint64_t fraction = ntohl((*parts)[1]);
        if (type == DATETIME_MICROSECONDS) {
            fraction &= 0xFFFFF800UL; // Make sure that last 11 bits are zeros
        }

        result += (fraction * S1E3) >> 32;
        *value = result;
    }

        return 0;
    default:
        return 1;
    }
}

/** Auxiliary structure for a field with a timestamp */
struct time_field {
    uint16_t id;          /**< Field ID              */
    enum datetime type;   /**< Type of the timestamp */
};

/**
 * \brief Get a one of 4 timestamps (in milliseconds)
 * \param[in]  record Record data
 * \param[in]  templ  Record template
 * \param[in]  fields Array of field IDs
 * \param[out] res    Result timestamp (filled only on success)
 * \return On success returns 0. Otherwise returns a non-zero value.
 */
static inline int
get_timestamp(uint8_t *record, struct ipfix_template *templ,
    const struct time_field fields[4], uint64_t *res)
{
    uint8_t *data_ptr;
    int data_len;
    int idx;

    for (idx = 0; idx < 4; ++idx) {
        const uint16_t field_id = fields[idx].id;
        data_ptr = data_record_get_field(record, templ, 0, field_id, &data_len);
        if (!data_ptr) {
            continue;
        }

        // Field found
        const enum datetime type = fields[idx].type;
        const size_t size = (size_t) data_len;
        return (convert_datetime_lp_be(data_ptr, size, type, res) == 0)
            ? 0 : 1;
    }

    // Not found
    return 1;
}

/**
 * \brief Get duration of a flow (in milliseconds)
 * \param[in]  record Record data
 * \param[in]  templ  Record template
 * \param[out] res    Result (filled only on success)
 * \return On success returns 0. Otherwise returns a non-zero value.
 */
static inline int
get_duration(uint8_t *record, struct ipfix_template *templ, ff_uint64_t *res) {
    static const struct time_field first_fields[] = {
        {152, DATETIME_MILLISECONDS},
        {150, DATETIME_SECONDS},
        {154, DATETIME_MICROSECONDS},
        {156, DATETIME_NANOSECONDS}
    };

    static const struct time_field last_fields[] = {
        {153, DATETIME_MILLISECONDS},
        {151, DATETIME_SECONDS},
        {155, DATETIME_MICROSECONDS},
        {157, DATETIME_NANOSECONDS}
    };

    uint64_t ts_start;
    uint64_t ts_end;

    // Get timestamp
    if (get_timestamp(record, templ, first_fields, &ts_start) != 0
        || get_timestamp(record, templ, last_fields, &ts_end) != 0) {
        return 1;
    }

    if (ts_end < ts_start) {
        return 1;
    }

    // Store the result
    *res = ts_end - ts_start;
    return 0;
}

/**
 * \brief set_external_ids
 * \param[in] item
 * \param     lvalue
 * \return number of ids set
 */
int set_external_ids(nff_item_t *item, ff_lvalue_t *lvalue)
{
    uint16_t gen, of2;
    uint32_t of1;
    unpackEnId(item->en_id, &gen, &of1, &of2);

    int ids = 0;

    if (gen & CTL_FPAIR) {
        ids += set_external_ids(item+of1, lvalue);
        ids += set_external_ids(item+of2, lvalue);
        return ids;
    }

    if (gen & CTL_FLAGS) {
        lvalue->options |= FF_OPTS_FLAGS;
    }

    while(ids < (int)sizeof(lvalue->id) / sizeof(lvalue->id[0]) && lvalue->id[ids].index) {
        ids++;
    }
    if (ids < (int)sizeof(lvalue->id) / sizeof(lvalue->id[0])) {
        lvalue->id[ids].index = item->en_id;
    }

    return ids;
}


/* callback from ffilter to lookup field */
ff_error_t ipf_lookup_func(ff_t *filter, const char *fieldstr, ff_lvalue_t *lvalue)
{
    /* fieldstr is set - try to find field id and relevant function */
    nff_item_t* item = NULL;
    const ipfix_element_t * elem;
    for (int x = 0; nff_ipff_map[x].name != NULL; x++) {
        if (!strcmp(fieldstr, nff_ipff_map[x].name)) {
            item = &nff_ipff_map[x];
            break;
        }
    }

    if (item == NULL) {	// Alias not found
        // Try to find an IPFIX field with this name
        const ipfix_element_result_t elemr = get_element_by_name(fieldstr, false);
        if (elemr.result == NULL) {
            ff_set_error(filter, "\"%s\" element item not found in ipfix names", fieldstr);
            return FF_ERR_OTHER_MSG;
        }

        lvalue->id[0].index = toEnId(elemr.result->en, elemr.result->id);
        lvalue->id[1].index = 0;
        elem = elemr.result;
    } else {
        lvalue->id[1].index = 0;

        uint16_t gen, id;
        uint32_t enterprise;

        set_external_ids(item, lvalue);
        unpackEnId(lvalue->id[0].index, &gen, &enterprise, &id);

        // This sets bad type when header or metadata items are selected
        if (gen & CTL_CALCULATED_ITEM) {
            lvalue->type = FF_TYPE_UNSIGNED;
            return FF_OK;
        } else if (gen & CTL_HEADER_ITEM) {
            switch (id) {
            case HD_ODID:
                lvalue->type = FF_TYPE_UNSIGNED_BIG;
                break;
            case HD_SRCADDR:
            case HD_DSTADDR:
                lvalue->type = FF_TYPE_ADDR;
                break;
            case HD_SRCPORT:
            case HD_DSTPORT:
                lvalue->type = FF_TYPE_UNSIGNED;
                break;
            default:
                ff_set_error(filter, "Cannot find IPFIX header element with ID '%d' "
                    "(not implemented)", id);
            }
            return FF_OK;
        } else if (gen & CTL_CONST_ITEM) {
            // Cleanup ID, constant is still field, it just has default value
            lvalue->id[0].index = toEnId(0, enterprise);
            // Set option to constant
            lvalue->options = FF_OPTS_CONST;
            // Select implicit value for given field
            lvalue->literal = constants[id];

            elem = get_element_by_id(enterprise, 0);
            if (!elem) {
                ff_set_error(filter, "Cannot find IPFIX element with ID '%d' "
                    "EN '0', required by constant \"%s\"", enterprise, fieldstr);
            }
        } else {
            // Nothing unusual
            elem = get_element_by_id(id, enterprise);
            if (!elem) {
                ff_set_error(filter, "Cannot find IPFIX element with ID '%d' "
                    "EN '%d', required by \"%s\"", id, enterprise, fieldstr);
            }
        }
    }

    if (elem == NULL) {
        return FF_ERR_OTHER_MSG;
    }
    // Map data types to internal types of ffilter
    switch(elem->type){

    case ET_BOOLEAN:
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

    case ET_IPV4_ADDRESS:
    case ET_IPV6_ADDRESS:
        lvalue->type = FF_TYPE_ADDR;
        break;

    case ET_DATE_TIME_SECONDS:
    case ET_DATE_TIME_MICROSECONDS:
    case ET_DATE_TIME_NANOSECONDS:
    case ET_FLOAT_32:
    case ET_OCTET_ARRAY:
    case ET_BASIC_LIST:
    case ET_SUB_TEMPLATE_LIST:
    case ET_SUB_TEMPLATE_MULTILIST:
    case ET_UNASSIGNED:
    default:
        lvalue->type = FF_TYPE_UNSUPPORTED;
        ff_set_error(filter, "IPFIX field \"%s\" has unsupported format", fieldstr);
        return FF_ERR_OTHER_MSG;
    }
    return FF_OK;
}

/* getting data callback */
ff_error_t ipf_data_func(ff_t *filter, void *rec, ff_extern_id_t id, char **data, size_t *size)
{
    //assuming rec is struct ipfix_message
    struct nff_msg_rec_s* msg_pair = rec;
    int len;
    char *ipf_field;

    uint32_t en;
    uint16_t ie_id;
    uint16_t generic_set;
    unpackEnId(id.index, &generic_set, &en, &ie_id);

    if (generic_set & CTL_HEADER_ITEM) {

        struct input_info *ii; /// Input info is base type of network version
        struct input_info_network *ii_net;
        int is_network;

        ii = msg_pair->msg->input_info;
        ii_net = (struct input_info_network *) ii;
        is_network = ii->type == SOURCE_TYPE_TCP || ii->type == SOURCE_TYPE_TCPTLS
                || ii->type == SOURCE_TYPE_UDP;

        switch (ie_id) {
        case HD_ODID:
            ipf_field = (char *) & (msg_pair->msg->pkt_header->observation_domain_id);
        //TODO: WARNING! Can't find structure pkt_header, so for now I rely on observed size
            len = sizeof(uint32_t); /// THIS NEED FIXING
            break;
        case HD_SRCADDR:
            if (!is_network) {
                return FF_ERR_OTHER_MSG;
            }
            if (ii_net->l3_proto == 4) {
                // IPv4
                ipf_field = (char *) & (ii_net->src_addr.ipv4);
                len = sizeof(ii_net->src_addr.ipv4);
            } else {
                if (IS_V4_IN_V6(&ii_net->src_addr.ipv6)) {
                    // IPv4 mapped into IPv6
                    ipf_field = (char *) & (ii_net->src_addr.ipv6.s6_addr[12]);
                    len = sizeof(ii_net->src_addr.ipv4);
                } else {
                    // IPv6
                    ipf_field = (char *) & (ii_net->src_addr.ipv6);
                    len = sizeof(ii_net->src_addr.ipv6);
                }
            }
            break;
        case HD_SRCPORT:
            if (!is_network) {
                return FF_ERR_OTHER_MSG;
            }
            ipf_field = (char *) & (ii_net->src_port);
            len = sizeof(ii_net->src_port);
            break;
        case HD_DSTADDR:
            if (!is_network) {
                return FF_ERR_OTHER_MSG;
            }
            if (ii_net->l3_proto == 4) {
                // IPv4
                ipf_field = (char *) & (ii_net->dst_addr.ipv4);
                len = sizeof(ii_net->dst_addr.ipv4);
            } else {
                if (IS_V4_IN_V6(&ii_net->dst_addr.ipv6)) {
                    // IPv4 mapped into IPv6
                    ipf_field = (char *) & (ii_net->dst_addr.ipv6.s6_addr[12]);
                    len = sizeof(ii_net->dst_addr.ipv4);
                } else {
                    // IPv6
                    ipf_field = (char *) & (ii_net->dst_addr.ipv6);
                    len = sizeof(ii_net->dst_addr.ipv6);
                }
            }
            break;
        case HD_DSTPORT:
            if (!is_network) {
                return FF_ERR_OTHER_MSG;
            }
            ipf_field = (char *) & (ii_net->dst_port);
            len = sizeof(ii_net->dst_port);
            break;
        default:
            //ff_set_error(filter, "Cannot find header element with ID '%d' "
            //    "(not implemented)", ie_id);
            return FF_ERR_OTHER;
        }

        *data = ipf_field;
        *size = len;
        return FF_OK;

    } else if (generic_set & CTL_CALCULATED_ITEM) {

        ff_uint64_t flow_duration;
        ff_uint64_t tmp, tmp2;

        uint8_t *rec_data = msg_pair->rec->record;
        struct ipfix_template *rec_tmplt = msg_pair->rec->templ;

        //TODO: add mpls handlers
        switch (ie_id) {
        case CALC_PPS: // Packets per second
            if (get_duration(rec_data, rec_tmplt, &flow_duration) != 0) {
                return FF_ERR_OTHER;
            }

            if (flow_duration == 0) {
                tmp = 0;
                len = sizeof(tmp);
                break;
            }

            // Get packets (ID 2)
            if (get_unsigned(rec_data, rec_tmplt, 2, &tmp) != 0) {
                return FF_ERR_OTHER;
            }

            // Duration is in milliseconds!
            tmp = (tmp * 1000) / flow_duration;
            len = sizeof(tmp);
            break;

        case CALC_DURATION: // Flow duration
            if (get_duration(rec_data, rec_tmplt, &flow_duration) != 0) {
                return FF_ERR_OTHER;
            }

            tmp = flow_duration;
            len = sizeof(tmp);
            break;

        case CALC_BPS: // Bits per second
            if (get_duration(rec_data, rec_tmplt, &flow_duration) != 0) {
                return FF_ERR_OTHER;
            }

            if (flow_duration == 0) {
                tmp = 0;
                len = sizeof(tmp);
                break;
            }

            // Get bytes (ID 1)
            if (get_unsigned(rec_data, rec_tmplt, 1, &tmp) != 0) {
                return FF_ERR_OTHER;
            }

            // Duration is in milliseconds (1000x) and bits (8x)
            tmp = (8000 * tmp) / flow_duration;
            len = sizeof(tmp);
            break;

        case CALC_BPP: // Bytes per packet
            if (get_unsigned(rec_data, rec_tmplt, 2, &tmp2) != 0) {
                return FF_ERR_OTHER;
            }

            if (tmp2 == 0) { // No packets!
                tmp = 0;
                len = sizeof(tmp);
                break;
            }

            if (get_unsigned(rec_data, rec_tmplt, 1, &tmp) != 0) {
                return FF_ERR_OTHER;
            }

            tmp = tmp / tmp2;
            len = sizeof(tmp);
            break;

        default:
            return FF_ERR_OTHER;
        }
        // Copy calculated data to provided buffer
        memcpy(*data, &tmp, len);
        *size = len;
        return FF_OK;

    } /*else if (generic_set & CTL_CONST_ITEM) {

        ipf_field = data_record_get_field((msg_pair->rec)->record, (msg_pair->rec)->templ, 0, en, &len);
        if (ipf_field == NULL) {
            return FF_ERR_OTHER;
        }

    } */else {

        ipf_field = data_record_get_field((msg_pair->rec)->record, (msg_pair->rec)->templ, en, ie_id, &len);
        if (generic_set & CTL_V4V6IP && ipf_field == NULL) {
            if (specify_ipv(&ie_id)) {
                ipf_field = data_record_get_field((msg_pair->rec)->record, (msg_pair->rec)->templ, en, ie_id, &len);
            }
        }
        if (ipf_field == NULL) {
            return FF_ERR_OTHER;
        }

    }
    *data = ipf_field;
    *size = len;
    return FF_OK;
}

ff_error_t ipf_rval_map_func(ff_t *filter, const char *valstr, ff_type_t type, ff_extern_id_t id, char *buf, size_t *size)
{
    struct nff_literal_s *dict = NULL;
    char *tcp_ctl_bits = "FSRPAUECNX";
    char *hit = NULL;
    *size = 0;

    uint32_t en;
    uint16_t ie_id;
    uint16_t generic_set;
    unpackEnId(id.index, &generic_set, &en, &ie_id);

    if (en != 0 || valstr == NULL) {
        return FF_ERR_OTHER;
    }

    unsigned x;

    *size = sizeof(ff_uint64_t);
    ff_uint64_t val;

    switch (ie_id) {
    /** Protocol */
    case 4:
        dict = nff_get_protocol_map();
        break;

    /** Translate tcpControlFlags */
    case 6:
        if (strlen(valstr) > 9) {
            return FF_ERR_OTHER;
        }

        for (x = val = 0; x < strlen(valstr); x++) {
            if ((hit = strchr(tcp_ctl_bits, valstr[x])) == NULL) {
                return FF_ERR_OTHER;
            }
            val |= 1 << (hit - tcp_ctl_bits);
            /* If X was in string set all flags */
            if (*hit == 'X') {
                val = 1 << (hit - tcp_ctl_bits);
                val--;
            }
        }
        memcpy(buf, &val, sizeof(val));
        return FF_OK;
        break;

    /** Src/dst ports */
    case 7:
    case 11:
        dict = nff_get_port_map();
        break;
    default:
        return FF_ERR_UNSUP;
    }

    // Universal processing for literals
    nff_literal_t *item = NULL;

    for (int x = 0; dict[x].name != NULL; x++) {
        if (!strcasecmp(valstr, dict[x].name)) {
            item = &dict[x];
            break;
        }
    }

    if (item != NULL) {
        memcpy(buf, &item->value, sizeof(item->value));
        *size = sizeof(item->value);
        return FF_OK;
    }

    return FF_ERR_OTHER;
}

/* Constructor */
ipx_filter_t *ipx_filter_create()
{
    ipx_filter_t *filter = NULL;

    if ((filter = calloc(1, sizeof(ipx_filter_t))) == NULL) {
        return NULL;
    }

    if ((filter->buffer = malloc(FF_MAX_STRING * sizeof(char))) == NULL) {
        free(filter);
        return NULL;
    }

    return filter;
}

/* Memory release function */
void ipx_filter_free(ipx_filter_t *filter)
{
    if (filter != NULL) {
        ff_free(filter->filter);
        free(filter->buffer);
    }
    free(filter);
}

int ipx_filter_parse(ipx_filter_t *filter, char* filter_str)
{
    int retval = 0;
    ff_options_t *opts = NULL;

    if (ff_options_init(&opts) == FF_ERR_NOMEM) {
        ff_set_error(filter->filter, "Memory allocation for options failed");
        return 1;
    }

    opts->ff_lookup_func = ipf_lookup_func;
    opts->ff_data_func = ipf_data_func;
    opts->ff_rval_map_func = ipf_rval_map_func;

    if (ff_init(&filter->filter, filter_str, opts) != FF_OK) {
        retval = 1;
    }

    ff_options_free(opts);
    return retval;
}

/* Evaulate expresion tree */
int ipx_filter_eval(ipx_filter_t *filter, struct ipfix_message *msg, struct ipfix_record *record)
{
    struct nff_msg_rec_s pack;
    pack.msg = msg;
    pack.rec = record;
    /* Necesarry to pass both msg and record to ff_eval, passed structure that contains both */
    return ff_eval(filter->filter, &pack);
}

char *ipx_filter_get_error(ipx_filter_t *filter)
{
    ff_error(filter->filter, (char *) filter->buffer, FF_MAX_STRING);
    ((char *) filter->buffer)[FF_MAX_STRING - 1] = 0;

    return (char *) filter->buffer;
}