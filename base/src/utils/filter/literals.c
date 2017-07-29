//
// Created by istoffa on 7/28/17.
//

#include "literals.h"

/* IANA protocol list subset*/
const struct nff_literal_s nff_proto_id_map[]={
    { "ICMP",	1 },
    { "IGMP",	2 },
    { "IPv4",	4 },
    { "TCP",	6 },
    { "UDP",	17 },
    { "RDP",	27 },
    { "IPv6",	41 },
    { "RSVP",	46 },
    { "IPv6-ICMP",	58 },
    { "ICMP6",	58 },
    { "EIGRP",	88 },
    { "ETHERIP",	97 },
    { "IPX-in-IP",	111 },
    { "L2TP",	115 },
    { "ISIS-over-IPv4",	124 },
    { "SPS",	130 },
    { "SCTP",	132 },
    { "UDPLite",	136 },
    // terminator
    { "", 	0U }
};

/* IANA assigned port names subset */
const struct nff_literal_s nff_port_map[]={
    { "tcpmux",	1 },
    { "echo",	7 },
    { "discard",	9 },
    { "systat",	11 },
    { "daytime",	13 },
    { "msp",	18 },
    { "ftp-data",	20 },
    { "ftp",	21 },
    { "ssh",	22 },
    { "telnet",	23 },
    { "smtp",	25 },
    { "time",	37 },
    { "rap",	38 },
    { "rlp",	39 },
    { "graphics",	41 },
    { "name",	42 },
    { "nameserver",	42 },
    { "nicname",	43 },
    { "http",	80 },
    { "https",	443 },
    // terminator
    { "", 	0U }
};

const struct nff_literal_s * nff_get_protocol_map()
{
    return &nff_proto_id_map[0];
}

const struct nff_literal_s * nff_get_port_map()
{
    return &nff_port_map[0];
}

