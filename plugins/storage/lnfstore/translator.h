#ifndef LS_TRANSLATOR_H
#define LS_TRANSLATOR_H

#include <libnf.h>

#define MAX_TABLE 72

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
