#ifndef KAFKA_H
#define KAFKA_H

#include "json.h"
#include <librdkafka/rdkafka.h>

class Kafka : public Output
{
public:
	Kafka(const pugi::xpath_node &config);

	~Kafka();
	void ProcessDataRecord(const std::string& record);

private:
	std::string _topic;
	int _partitions = 1;
	int _current_partition = 0;
        rd_kafka_t *_rk; /* Producer instance handle */
        rd_kafka_topic_t *_rkt; /* Topic object */
};

#endif // KAFKA_H
