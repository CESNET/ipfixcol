
#ifndef FORWARDING_H
#define FORWARDING_H

#include "../../preprocessor.h"
#include <siso.h>

#define DEFAULT_PORT "4739"
#define DEFAULT_PROTOCOL "UDP"

enum distribution_type {
	DT_TO_ALL,
	DT_ROUND_ROBIN
};

/* Forwarded template record */
struct forwarding_template_record {
	uint32_t last_sent;
	uint32_t packets;
	int type, length;
	uint32_t odid;
	struct ipfix_template_record *record;
};

/* Plugin configuration */
typedef struct forwarding_config {
	const char *default_port, *default_protocol;
	int records_cnt, records_max;
	struct forwarding_template_record **records;
	int senders_cnt, senders_max, sender_index;
	sisoconf **senders;
	struct udp_conf udp;
	bool udp_connection;
	int distribution;
} forwarding;

struct forwarding_process {
	uint8_t *msg;
	int offset, type, length;
	uint32_t odid;
	forwarding *conf;
};

#endif // FORWARDING_H
