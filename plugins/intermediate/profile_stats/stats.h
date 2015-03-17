#ifndef STATS_H
#define STATS_H

#include <string>
#include <map>

/* Default stats interval */
#define DEFAULT_INTERVAL 300

/* Fields identifiers */
#define TRAFFIC_ID	1
#define PACKETS_ID	2
#define PROTOCOL_ID 4

/* Number of groups and fields per groups */
#define GROUPS 3
#define PROTOCOLS_PER_GROUP 5

/* Statistics groups */
enum st_group {
	FLOWS,
	PACKETS,
	TRAFFIC,
};

/* Statistics protocols */
enum st_protocol {
	ST_TOTAL,
	ST_TCP,
	ST_UDP,
	ST_ICMP,
	ST_OTHER
};

/* IPFIX protocol identifiers*/
enum ipfix_groups {
	IG_ICMP = 1,
	IG_TCP = 6,
	IG_UDP = 17,
	IG_ICMPv6 = 58,
};

#endif // STATS_H

