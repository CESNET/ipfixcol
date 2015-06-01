
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include "SocketController.h"
#include "verbose.h"

#define LEN_BYTES 4
#define BACKLOG 20
#define BUFFER_SIZE 2048

static SocketController *socketController = nullptr;

void handle(int signalNumber)
{
	(void) signalNumber;

	if (socketController != nullptr) {
		socketController->stop();
	}
}

int SocketController::initControllerSocket(std::string path)
{
	struct sockaddr_un address;
	int newSocket = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (newSocket < 0) {
		throw std::runtime_error(strerror(errno));
	}

	address.sun_family = AF_UNIX;
	strcpy(address.sun_path, path.c_str());
	unlink(address.sun_path);

	if (bind(newSocket, (struct sockaddr *)&address, strlen(address.sun_path) + sizeof(address.sun_family)) < 0) {
		close(newSocket);
		throw std::runtime_error(strerror(errno));
	}

	return newSocket;
}

int SocketController::initCollectorsSocket(std::string port)
{
	int newSocket, yes = 1;
	struct addrinfo hints, *serverinfo, *p;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	if (getaddrinfo(NULL, port.c_str(), &hints, &serverinfo) != 0) {
		throw std::runtime_error(strerror(errno));
	}

	for (p = serverinfo; p != nullptr; p = p->ai_next) {
		newSocket = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (newSocket < 0) {
			MSG_WARNING(strerror(errno));
			continue;
		}

		if (setsockopt(newSocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) < 0) {
			throw std::runtime_error(strerror(errno));
		}

		if (bind(newSocket, p->ai_addr, p->ai_addrlen) < 0) {
			close(newSocket);
			MSG_WARNING(strerror(errno));
			continue;
		}

		break;
	}

	if (p == nullptr) {
		throw std::runtime_error("Failed to bind");
	}

	freeaddrinfo(serverinfo);

	if (listen(newSocket, BACKLOG) < 0) {
		close(newSocket);
		throw std::runtime_error(strerror(errno));
	}

	return newSocket;

}

SocketController::SocketController(std::string controllerSocket, std::string portNumber)
{
	m_controllerSocket = initControllerSocket(controllerSocket);
	m_collectorsSocket = initCollectorsSocket(portNumber);
}

void SocketController::stop()
{
	m_done = true;

	if (m_thread.joinable()) {
		m_thread.join();
	}
}

void SocketController::sendConfigToAll()
{
	m_actualConfig = m_profiles->getXmlConfig();
	m_actualConfigLength = htonl(m_actualConfig.length());

	for (uint16_t i = 0; i < m_activeCollectors.size(); ++i) {
		sendConfigToCollector(i);
	}
}

bool SocketController::sendData(int index, void *data, uint16_t length)
{
	if (send(m_activeCollectors[index], data, length, 0) != length) {
		if (errno != EINTR) {
			MSG_ERROR(strerror(errno));
			MSG_ERROR("Closing connection with collector");

			close(m_activeCollectors[index]);
			m_activeCollectors.erase(m_activeCollectors.begin() + index);
			return false;
		}
	}

	return true;
}

void SocketController::sendConfigToCollector(int index)
{
	if (sendData(index, (void*) &m_actualConfigLength, LEN_BYTES)) {
		sendData(index, (void*) m_actualConfig.c_str(), m_actualConfig.length());
	}
}

void SocketController::listenForCollectors()
{
	struct sockaddr_storage client;
	socklen_t addr_size = sizeof(client);
	int newSocket;

	while (!m_done) {
		newSocket = accept(m_collectorsSocket, (struct sockaddr*) &client, &addr_size);
		if (newSocket < 0) {
			MSG_ERROR(strerror(errno));
			continue;
		}

		m_activeCollectors.push_back(newSocket);

		if (m_actualConfig.empty()) {
			m_actualConfig = m_profiles->getXmlConfig();
		}
		sendConfigToCollector(m_activeCollectors.size() - 1);
	}
}

void SocketController::run()
{
	m_done = false;
	m_thread = std::move(std::thread(&SocketController::listenForCollectors, this));

	char buffer[BUFFER_SIZE];
	struct sockaddr_un clientAddress;
	socklen_t addressLength = sizeof(struct sockaddr_un);

	socketController = this;
	signal(SIGINT, handle);

	while (!m_done) {
		int numberOfBytes = recvfrom(m_controllerSocket, buffer, BUFFER_SIZE, 0,
								 (struct sockaddr *) &clientAddress, &addressLength);
		if (numberOfBytes < 0) {
			MSG_ERROR("%s", strerror(errno));
			continue;
		}

		MSG_DEBUG("Message from controller: %s", buffer);

		std::string response = processMessage(buffer);

		MSG_DEBUG("Sending response: %s", response.c_str());

		if (sendto(m_controllerSocket, response.c_str(), response.length(), 0,
				   (struct sockaddr *) &clientAddress, addressLength) != (uint16_t) response.length()) {
			MSG_ERROR("%s", strerror(errno));
		}
	}
}

std::string SocketController::processMessage(std::string message)
{
	std::cout << message << "\n";
	json::Object response;
	response["status"] = "OK";

	json::Value jMessage = json::Deserialize(message);
	if (jMessage.GetType() == json::NULLVal || !jMessage.HasKey("requests")) {
		response["status"]  = "Error";
		response["message"] = "Invalid request";
		return json::Serialize(response);
	}

	json::Array requests = jMessage.ToArray();
	std::string errorMessage{};

	for (json::Value &request : requests) {

		errorMessage = processRequest(request.ToObject());

		if (!errorMessage.empty()) {
			response["status"]  = "Error";
			response["message"] = errorMessage;
			break;
		}
	}

	return json::Serialize(response);
}

std::string SocketController::processRequest(json::Object request)
{
	std::string type = request["type"];
	std::string path = request["path"];

	if (type.empty()) {
		return "Missing request type";
	}

	if (path.empty()) {
		return type + ": missing path";
	}

	m_profiles->resetLastError();

	Channel *channel{};

	if (type == "addProfile") {
		m_profiles->addProfile(path);

	} else if (type == "addChannel") {
		channel = m_profiles->addChannel(path);

	} else if (type == "removeProfile") {
		m_profiles->removeProfile(path);

	} else if (type == "removeChannel") {
		m_profiles->removeChannel(path);

	} else if (type == "editChannel") {
		channel = m_profiles->getChannel(path);

	} else {
		return "Unknown request type: " + type;
	}

	/* Add or modify channel */
	if (channel) {

		if (request.HasKey("name")) {
			/* Check whether the new channel name is unique within profile */
			for (Channel *c: channel->getProfile()->getChannels()) {
				if (c->getName() == request["name"] && c != channel) {
					return "Channel with name '" + c->getName() + "' already exists in profile '" + c->getProfile()->getName();
				}
			}

			channel->setName(request["name"]);
		}

		if (request.HasKey("filter")) {
			channel->setFilter(request["filter"]);
		}

		if (request.HasKey("sources")) {
			std::string sources = json::Serialize(request["sources"]);

			// '["some", "channel", "sources"]' -> 'some, channel, sources'
			sources = sources.substr(sources.find_first_of('['), sources.find_last_of(']'));
			sources.erase(std::remove(sources.begin(), sources.end(), '"'), sources.end());

			channel->setSources(sources);
		}
	}

	return m_profiles->getLastError();
}

void SocketController::setProfiles(Profiles *profiles)
{
	m_profiles = profiles;
	profiles->setCollectors(this);
}
