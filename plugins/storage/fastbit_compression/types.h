/** @file
  */
#ifndef TYPES_H
#define TYPES_H

#define NTYPES 24

typedef enum {
	IPFIX_TYPE_UNKNOWN                =  0,
	IPFIX_TYPE_octetArray             =  1,
	IPFIX_TYPE_unsigned8              =  2,
	IPFIX_TYPE_unsigned16             =  3,
	IPFIX_TYPE_unsigned32             =  4,
	IPFIX_TYPE_unsigned64             =  5,
	IPFIX_TYPE_signed8                =  6,
	IPFIX_TYPE_signed16               =  7,
	IPFIX_TYPE_signed32               =  8,
	IPFIX_TYPE_signed64               =  9,
	IPFIX_TYPE_float32                = 10,
	IPFIX_TYPE_float64                = 11,
	IPFIX_TYPE_boolean                = 12,
	IPFIX_TYPE_macAddress             = 13,
	IPFIX_TYPE_string                 = 14,
	IPFIX_TYPE_dateTimeSeconds        = 15,
	IPFIX_TYPE_dateTimeMilliseconds   = 16,
	IPFIX_TYPE_dateTimeMicroseconds   = 17,
	IPFIX_TYPE_dateTimeNanoseconds    = 18,
	IPFIX_TYPE_ipv4Address            = 19,
	IPFIX_TYPE_ipv6Address            = 20,
	IPFIX_TYPE_basicList              = 21,
	IPFIX_TYPE_subTemplateList        = 22,
	IPFIX_TYPE_subTemplateMultiList   = 23
} ipfix_type_t;

struct type_tab_item {
	ipfix_type_t ipfix_type;
	const char *ipfix_type_name;
};

typedef std::map<std::pair<uint32_t, uint16_t>, ipfix_type_t> type_cache_t;

#endif
