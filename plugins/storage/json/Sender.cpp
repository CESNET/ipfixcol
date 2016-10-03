#include "Sender.h"

#include <stdexcept>
#include <sys/time.h>

static const char *msg_module = "json sender";

#define DEFAULT_IP		"127.0.0.1"
#define DEFAULT_PORT	"4739"
#define DEFAULT_TYPE	"UDP"

Sender::Sender(const pugi::xpath_node &config)
{
	std::string ip    = config.node().child_value("ip");
	std::string port  = config.node().child_value("port");
	std::string proto = config.node().child_value("protocol");

	/* Check IP address */
	if (ip.empty()) {
		MSG_WARNING(msg_module, "IP address not set, using default: %s", DEFAULT_IP);
		ip = DEFAULT_IP;
	}

	/* Check port number */
	if (port.empty()) {
		MSG_WARNING(msg_module, "Port number not set, using default: %s", DEFAULT_PORT);
		port = DEFAULT_PORT;
	}

	/* Check connection type */
	if (proto.empty()) {
		MSG_WARNING(msg_module, "Protocol not set, using default: %s", DEFAULT_TYPE);
		proto = DEFAULT_TYPE;
	}

	/* Create sender */
	sender = siso_create();
	if (sender == NULL) {
		throw std::runtime_error("Memory error - cannot create sender object");
	}

	/* Connect it */
	if (siso_create_connection(sender, ip.c_str(), port.c_str(), proto.c_str()) != SISO_OK) {
		std::string error = siso_get_last_err(sender);
		siso_destroy(sender);
		throw std::runtime_error(error);
	}

	gettimeofday(&connection_time, NULL);
}

Sender::~Sender()
{
	siso_destroy(sender);
}

void Sender::ProcessDataRecord(const std::string &record)
{
	if (siso_is_connected(sender) == 0) {
		// Not connected -> try to reconnect
		struct timeval current_time;
		gettimeofday(&current_time, NULL);

		// Try only one reconnection per second
		if (connection_time.tv_sec >= current_time.tv_sec) {
			return;
		}

		connection_time = current_time;
		if (siso_reconnect(sender) == SISO_OK) {
			MSG_INFO(msg_module, "Successfully reconnected.", NULL);
		} else {
			MSG_WARNING(msg_module, "Reconnection failed.", NULL);
			return;
		}
	}

	if (siso_send(sender, record.c_str(), record.length()) != SISO_OK) {
		MSG_ERROR(msg_module, "Failed to send JSON data (%s). Connection closed.",
			siso_get_last_err(sender));
	}
}
