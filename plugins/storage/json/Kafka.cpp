#include "Kafka.h"

#include <stdexcept>
#include <sys/time.h>
#include <unistd.h>

static const char *msg_module = "json kafka";

static void msg_delivered (rd_kafka_t *rk, void *payload, size_t len, 
        rd_kafka_resp_err_t err, void *opaque, void *msg_opaque)
{
    if (err) {
        MSG_ERROR(msg_module, "Message delivery failed: %s",
            rd_kafka_err2str(err));
    }
}

Kafka::Kafka(const pugi::xpath_node &config)
{
    rd_kafka_conf_t *conf; /* Temporary configuration object */
    char errstr[512];

    std::string ip    = config.node().child_value("ip");
    std::string port  = config.node().child_value("port");
    std::string partitions_str = config.node().child_value("partitions");
    _topic = config.node().child_value("topic");

    /* Check IP address */
    if (ip.empty()) {
        throw std::invalid_argument("IP address not set");
    }

    /* Check port number */
    if (port.empty()) {
        throw std::invalid_argument("Port number not set");
    }

    /* Check topic */
    if (_topic.empty()) {
        throw std::invalid_argument("Topic not set");
    }

    /* Check partition */
    if (partitions_str.empty()) {
        throw std::invalid_argument("Number of partitions not set");
    } else {
        _partitions = atoi(partitions_str.c_str()); // TODO fixme
    }

    // create kafka configuration
    conf = rd_kafka_conf_new();

    // set delivery callback
    rd_kafka_conf_set_dr_cb(conf, msg_delivered);

    // create new producer
    _rk = rd_kafka_new(RD_KAFKA_PRODUCER, conf, errstr, sizeof(errstr));
    if (!_rk) {
        throw std::runtime_error(
            std::string("Failed to create new producer: ") + errstr);
    }

    // set brokers
    if (rd_kafka_brokers_add(_rk, (ip + ":" + port).c_str()) == 0) {
        throw std::runtime_error("No valid brokers specified");
    }

    // create new topic
    _rkt = rd_kafka_topic_new(_rk, _topic.c_str(), NULL);
    if (!_rkt) {
        rd_kafka_destroy(_rk);
        throw std::runtime_error(std::string("Failed to create topic: ") 
            + rd_kafka_err2str(rd_kafka_errno2err(errno)));
    }

}

Kafka::~Kafka()
{
    MSG_INFO(msg_module, "Waiting for Kafka output to finish sending");
    while (rd_kafka_outq_len(_rk) > 0) {
        rd_kafka_poll(_rk, 100);
    }

    // destroy topic
    rd_kafka_topic_destroy(_rkt);
    // destroy the producer
    rd_kafka_destroy(_rk);

    MSG_INFO(msg_module, "Kafka plugin finished");
}

void Kafka::ProcessDataRecord(const std::string &record)
{
    while (rd_kafka_produce(_rkt, _current_partition++ % _partitions,
        RD_KAFKA_MSG_F_COPY, (void *) record.c_str(), record.length(),
        NULL, 0, NULL) != 0) {

        switch (errno) {
        case ENOBUFS:
            MSG_WARNING(msg_module, "maximum number of outstanding messages"
                " (%u) has been reached: 'queue.buffering.max.messages'",
                rd_kafka_outq_len(_rk));

            // wait a while for the queue to be processed
            usleep(200000);
            rd_kafka_poll(_rk, 0);
            break;
        case EMSGSIZE:
            MSG_ERROR(msg_module, "Message is larged than configured max size:"
                " 'messages.max.bytes'");
            return;
        case ESRCH: 
            throw std::runtime_error("Requested 'partition' is unknown in the "
                "Kafka cluster.");
            break;
        case ENOENT:
            throw std::runtime_error("Topic is unknown in the Kafka cluster.");
            break;
        default:
            MSG_ERROR(msg_module, "Unknown error while producing a message to "
                "Kafka");
            break;
        }
    }

    rd_kafka_poll(_rk, 0);
}
