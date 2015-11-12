#include <ipfixcol.h>
#include "lnfstore.h"
#include "translator.h"

static const char* msg_module = "lnfstore_translator";

struct ipfix_lnf_map tr_table[]= 
{
	{0, 1, LNF_FLD_DOCTETS, tr_general },
	{0, 2, LNF_FLD_DPKTS, tr_general },
	{0, 3, LNF_FLD_AGGR_FLOWS, tr_general },
	{0, 4, LNF_FLD_PROT, tr_general },
	{0, 5, LNF_FLD_TOS, tr_general },
	{0, 6, LNF_FLD_TCP_FLAGS, tr_general },
	{0, 7, LNF_FLD_SRCPORT, tr_general },
	{0, 8, LNF_FLD_SRCADDR, tr_address },
	{0, 9, LNF_FLD_SRC_MASK, tr_general },
	{0, 10, LNF_FLD_INPUT, tr_general },
	{0, 11, LNF_FLD_DSTPORT, tr_general },
	{0, 12, LNF_FLD_DSTADDR, tr_address },
	{0, 13, LNF_FLD_DST_MASK, tr_general },
	{0, 14, LNF_FLD_OUTPUT, tr_general },
	{0, 15, LNF_FLD_IP_NEXTHOP, tr_address },
	{0, 16, LNF_FLD_SRCAS, tr_general },
	{0, 17, LNF_FLD_DSTAS, tr_general },
	{0, 18, LNF_FLD_BGP_NEXTHOP, tr_address },
	{0, 21, LNF_FLD_LAST, tr_datetime },
	{0, 22, LNF_FLD_FIRST, tr_datetime },
	{0, 23, LNF_FLD_OUT_BYTES, tr_general },
	{0, 24, LNF_FLD_OUT_PKTS, tr_general },
	{0, 27, LNF_FLD_SRCADDR, tr_address },
	{0, 28, LNF_FLD_DSTADDR, tr_address },
	{0, 29, LNF_FLD_SRC_MASK, tr_general },
	{0, 30, LNF_FLD_DST_MASK, tr_general },
	{0, 32, LNF_FLD_DSTPORT, tr_general  }, // LNF_FLD_ specific id missing, DSTPORT overlaps
	{0, 38, LNF_FLD_ENGINE_TYPE, tr_general },
	{0, 39, LNF_FLD_ENGINE_ID, tr_general },
	{0, 55, LNF_FLD_DST_TOS, tr_general },
	{0, 56, LNF_FLD_IN_SRC_MAC, tr_general },
	{0, 57, LNF_FLD_OUT_DST_MAC, tr_general },
	{0, 58, LNF_FLD_SRC_VLAN, tr_general },
	{0, 59, LNF_FLD_DST_VLAN, tr_general },
	{0, 61, LNF_FLD_DIR, tr_general },
	{0, 62, LNF_FLD_IP_NEXTHOP, tr_address },
	{0, 63, LNF_FLD_BGP_NEXTHOP, tr_general },
	{0, 70, LNF_FLD_MPLS_LABEL, tr_mpls }, //this refers to base of stack
	{0, 71, LNF_FLD_MPLS_LABEL, tr_mpls },
	{0, 72, LNF_FLD_MPLS_LABEL, tr_mpls },
	{0, 73, LNF_FLD_MPLS_LABEL, tr_mpls },
	{0, 74, LNF_FLD_MPLS_LABEL, tr_mpls },
	{0, 75, LNF_FLD_MPLS_LABEL, tr_mpls },
	{0, 76, LNF_FLD_MPLS_LABEL, tr_mpls },
	{0, 77, LNF_FLD_MPLS_LABEL, tr_mpls },
	{0, 78, LNF_FLD_MPLS_LABEL, tr_mpls },
	{0, 79, LNF_FLD_MPLS_LABEL, tr_mpls },
	{0, 80, LNF_FLD_OUT_SRC_MAC, tr_general },
	{0, 81, LNF_FLD_IN_DST_MAC, tr_general },
	{0, 89, LNF_FLD_FWD_STATUS, tr_general },
	{0, 128, LNF_FLD_BGPNEXTADJACENTAS, tr_general },
	{0, 129, LNF_FLD_BGPPREVADJACENTAS, tr_general },
	{0, 148, LNF_FLD_CONN_ID, tr_general },
	{0, 150, LNF_FLD_FIRST, tr_datetime },
	{0, 151, LNF_FLD_LAST, tr_datetime },
	{0, 152, LNF_FLD_FIRST, tr_datetime },
	{0, 153, LNF_FLD_LAST, tr_datetime },
	{0, 154, LNF_FLD_FIRST, tr_datetime },
	{0, 155, LNF_FLD_LAST, tr_datetime },
	{0, 176, LNF_FLD_ICMP_TYPE, tr_general },
	{0, 177, LNF_FLD_ICMP_CODE, tr_general },
	{0, 225, LNF_FLD_XLATE_SRC_IP, tr_address },
	{0, 226, LNF_FLD_XLATE_DST_IP, tr_address },
	{0, 227, LNF_FLD_XLATE_SRC_PORT, tr_general },
	{0, 228, LNF_FLD_XLATE_DST_PORT, tr_general },
	{0, 230, LNF_FLD_EVENT_FLAG, tr_general }, //not sure
	{0, 233, /*LNF_FLD_FW_XEVENT*/ LNF_FLD_ZERO_, tr_general },
	{0, 234, LNF_FLD_INGRESS_VRFID, tr_general },
	{0, 235, LNF_FLD_EGRESS_VRFID, tr_general },
	{0, 258, LNF_FLD_RECEIVED, tr_general },
	{0, 281, LNF_FLD_XLATE_SRC_IP, tr_address }, //?
	{0, 282, LNF_FLD_XLATE_DST_IP, tr_address }, //?
	{0, 0, LNF_FLD_ZERO_, NULL}
};

int ipfix_lnf_map_compare(const void* pkey, const void* pelem)
{
	const struct ipfix_lnf_map *key, *elem;
	key = pkey;
	elem = pelem;

	if( (((uint64_t)key->en) << 16 | key->ie) < (((uint64_t)elem->en) << 16 | elem->ie)){
		return -1;
	}
	if( (((uint64_t)key->en) << 16 | key->ie) == (((uint64_t)elem->en) << 16 | elem->ie)){
		return 0;
	}
	if( (((uint64_t)key->en) << 16 | key->ie) > (((uint64_t)elem->en) << 16 | elem->ie)){
		return 1;
	}
}


uint16_t real_length(uint8_t* src_data, uint16_t* offset, uint16_t length)
{
	if(length != VAR_IE_LENGTH){
		return length;
	}
	length = src_data[*offset];
	(*offset) += 1;
	if(length == 255){
		length = ntohs(readui16(src_data+*offset));
		(*offset) += 2;
	}
	return length;
}


int tr_general(uint8_t *src_data, uint16_t* offset, uint16_t* length, uint8_t* buffer, struct ipfix_lnf_map* item_info)
{
	switch(*length)
	{
	case 1:
		readui8(buffer) = readui8(src_data + *offset);
	break;
	case 2:
		readui16(buffer) = ntohs(readui16(src_data + *offset));
	break;
	case 4:
		readui32(buffer) = ntohl(readui32(src_data + *offset));
	break;
	case 8:
		readui64(buffer) = be64toh(readui64(src_data + *offset));
	break;
	case VAR_IE_LENGTH:
		*length = real_length(src_data, offset, *length);
	default:
		//Assume data type to be octetArray -> no endian conversion
		memcpy(buffer, src_data + *offset, *length);
	}
	if(/*length check comes here*/0){
		//invalid length
		return 1;
	}
	return 0;
}

int tr_address(uint8_t *src_data, uint16_t* offset, uint16_t *length, uint8_t* buffer, struct ipfix_lnf_map* item_info)
{
	switch(*length)
	{
	case 4: 
		memset(buffer, 0x0, sizeof(lnf_ip_t));
		((lnf_ip_t*)buffer)->data[3] = readui32(src_data + *offset);
	break;
	case 16:
		memcpy(buffer, src_data + *offset, 16);
	break;
	default:
		return 1;
	}
	return 0;
}

int tr_datetime(uint8_t *src_data, uint16_t* offset, uint16_t *length, uint8_t* buffer, struct ipfix_lnf_map* item_info)
{
	//lnf requires timestamp in milliseconds
	uint16_t ie = item_info->ie;
	switch(ie)
	{
		//FlowEnd/StartSysUpTime
	case 21: case 22:
			readui64(buffer) = ntohl(readui32(src_data + *offset));
	break;
		//iana ipfix flowStart/EndSeconds
	case 150: case 151:
			readui64(buffer) = be64toh(readui64(src_data + *offset))*1000;
	break;
		//flowStart/EndMilliseconds
	case 152: case 153:
			readui64(buffer) = be64toh(readui64(src_data + *offset));
	break;
		//flowStart/EndMicroseconds
	case 154: case 155:
			readui64(buffer) = be64toh(readui64(src_data + *offset))/1000;
	break;
		//flowStart/EndNanoseconds
	case 156: case 157:
			readui64(buffer) = be64toh(readui64(src_data + *offset))/1000000;
	break;
	default:
		return 1;
	}
	return 0;
	
}

int tr_mpls(uint8_t *src_data, uint16_t* offset, uint16_t *length, uint8_t* buffer, struct ipfix_lnf_map* item_info)
{
	//Assume all mpls labels are saved as block in ipfix msg, so between 
	//succesive calls of tr_mpls buffer remains unchaged
	
	static int lc = 0; //label count
	//First base mpls label resets memory of this function
	if(item_info->en == 0 && item_info->ie == 70){
		lc=0;
		memset(buffer, 0x0, sizeof(lnf_mpls_t));
	}

	/** data scheme
	 *					0		1		2		3		4		5		6		7
	 *	xth	qword	|	_	|	label 2xth data		||	_	|	label 2xth-1 data	|
	 *	
	 **/
	unsigned spec_offs = lc/2 + (lc%2) ? 5 : 1; 
	memcpy(buffer + spec_offs, src_data + *offset, 3);

	return 0;
}



