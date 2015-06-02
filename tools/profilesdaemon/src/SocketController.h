
#ifndef SOCKETCONTROLER_H
#define	SOCKETCONTROLER_H

#include <string>
#include <thread>
#include "Profiles.h"
#include "SuperEasyJSON/json.h"

extern "C" {
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/signal.h>
#include <netinet/in.h>
}

class Profiles;

class SocketController{
public:
	SocketController(std::string controllerSocket, std::string portNumber);
	~SocketController();

	void run();
	void stop();

	void setProfiles(Profiles *profiles);
	void sendConfigToAll();

private:
	bool sendData(int index, void *data, uint16_t length);
	void sendConfigToCollector(int index);
	void listenForCollectors();
	void setupSignalHandler();
	void prepareConfigForSending();

	int initControllerSocket(std::string path);
	int initCollectorsSocket(std::string port);

	std::string processMessage(std::string message);
	std::string processRequest(json::Object request);

	Profiles *m_profiles{nullptr};

	int m_collectorsSocket;
	int m_controllerSocket;
	bool m_done{true};

	std::vector<int> m_activeCollectors{};
	std::string m_actualConfig{};
	uint32_t m_actualConfigLength{};
	std::thread m_thread;
};

#endif	/* SOCKETCONTROLER_H */
