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
	TOTAL,
	TCP,
	UDP,
	ICMP,
	OTHER
};

/* IPFIX protocol identifiers*/
enum ipfix_groups {
	IG_ICMP = 1,
	IG_TCP = 6,
	IG_UDP = 17,
};

/**
 * Stats data per ODID
 */
struct stats_data {
	uint64_t last;		/**< Time of last update */
	std::string file;	/**< Path to RRD file */
	uint64_t fields[GROUPS][PROTOCOLS_PER_GROUP];	/**< Stats fields per group */
};

/**
 * \struct plugin_conf
 *
 * Plugin configuration
 */
struct plugin_conf {
	std::string path;		/**< Path to RRD files */
	uint32_t interval;      /**< Statistics interval */
	void *ip_config;		/**< intermediate process config */
	std::string templ;		/**< RRD template */
	std::map<uint32_t, stats_data*> stats;	/**< RRD stats per ODID */
};


#endif // STATS_H

