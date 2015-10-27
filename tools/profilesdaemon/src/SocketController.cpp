
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include "SocketController.h"
#include "verbose.h"

#define LEN_BYTES 4
#define BACKLOG 20
#define BUFFER_SIZE 2048

static SocketController *socketController = nullptr;

void handler(int signalNumber)
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
		throw std::runtime_error(std::string("socket(): ") + strerror(errno));
	}

	address.sun_family = AF_UNIX;
	/* Make string zero terminated and copy safely */
	address.sun_path[sizeof(address.sun_path) - 1] = '\0';
	strncpy(address.sun_path, path.c_str(), sizeof(address.sun_path) - 1);
	unlink(address.sun_path);

	int len = strlen(address.sun_path) + sizeof(address.sun_family);
	if (bind(newSocket, (struct sockaddr *)&address, len) < 0) {
		close(newSocket);
		MSG_ERROR("bind() to %s: %s", address.sun_path, strerror(errno));
		throw std::runtime_error(std::string("bind() failed"));
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
		throw std::runtime_error(std::string("getaddrinfo(): ") + strerror(errno));
	}

	for (p = serverinfo; p != nullptr; p = p->ai_next) {
		newSocket = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (newSocket < 0) {
			MSG_WARNING("socket(): %s", strerror(errno));
			continue;
		}

		if (setsockopt(newSocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) < 0) {
			throw std::runtime_error(std::string("setsockopt(): ") + strerror(errno));
		}

		if (bind(newSocket, p->ai_addr, p->ai_addrlen) < 0) {
			close(newSocket);
			MSG_WARNING("bind(): %s", strerror(errno));
			continue;
		}

		break;
	}

	if (p == nullptr) {
		throw std::runtime_error("bind() failed");
	}

	freeaddrinfo(serverinfo);

	if (listen(newSocket, BACKLOG) < 0) {
		close(newSocket);
		throw std::runtime_error(std::string("listen(): ") + strerror(errno));
	}

	return newSocket;
}

SocketController::SocketController(std::string controllerSocket, std::string portNumber)
{
	MSG_DEBUG("Initializing controller socket");
	m_controllerSocket = initControllerSocket(controllerSocket);

	MSG_DEBUG("Initializing socket for collectors");
	m_collectorsSocket = initCollectorsSocket(portNumber);
}

SocketController::~SocketController()
{
	stop();
	close(m_collectorsSocket);
	close(m_controllerSocket);

	for (uint16_t i = 0; i < m_activeCollectors.size(); ++i) {
		close(m_activeCollectors[i]);
	}
}

void SocketController::stop()
{
	if (!m_done) {
		MSG_DEBUG("Stopping socket controller");
		m_done = true;
	}
}

void SocketController::sendConfigToAll()
{
	MSG_DEBUG("Sending config to %d collector(s)", m_activeCollectors.size());
	prepareConfigForSending();

	for (uint16_t i = 0; i < m_activeCollectors.size(); ++i) {
		sendConfigToCollector(i);
	}
}

bool SocketController::sendData(int index, void *data, uint16_t length)
{
	if (send(m_activeCollectors[index], data, length, 0) != length) {
		if (errno != EINTR) {
			MSG_ERROR("send(): %s", strerror(errno));
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

void SocketController::prepareConfigForSending()
{
	m_actualConfig = m_profiles->getXmlConfig();
	m_actualConfigLength = htonl(m_actualConfig.length());
}

void SocketController::listenForCollectors()
{
	struct sockaddr_storage client;
	socklen_t addr_size = sizeof(client);
	int newSocket;

	while (!m_done) {
		newSocket = accept(m_collectorsSocket, (struct sockaddr*) &client, &addr_size);
		if (newSocket < 0) {
			if (errno == EINTR) {
				continue;
			}

			MSG_ERROR(strerror(errno));
			continue;
		}

		m_activeCollectors.push_back(newSocket);

		if (m_actualConfig.empty()) {
			prepareConfigForSending();
		}
		MSG_DEBUG("Sending config to new collector");
		sendConfigToCollector(m_activeCollectors.size() - 1);
	}
}

void SocketController::setupSignalHandler()
{
	socketController = this;

	struct sigaction newAction;
	newAction.sa_handler = handler;
	newAction.sa_flags = 0;

	sigemptyset(&newAction.sa_mask);
	sigaction(SIGINT, &newAction, NULL);
}

void SocketController::run()
{
	m_done = false;
	m_thread = std::move(std::thread(&SocketController::listenForCollectors, this));

	char buffer[BUFFER_SIZE];
	struct sockaddr_un clientAddress;
	socklen_t addressLength = sizeof(struct sockaddr_un);

	setupSignalHandler();

	while (!m_done) {
		int numberOfBytes = recvfrom(m_controllerSocket, buffer, BUFFER_SIZE, 0,
								 (struct sockaddr *) &clientAddress, &addressLength);

		if (numberOfBytes < 0) {
			if (errno == EINTR) {
				continue;
			}

			MSG_ERROR("recvfrom(): %s", strerror(errno));
			continue;
		}

		buffer[numberOfBytes] = '\0';

		MSG_DEBUG("Message from controller: %s", buffer);

		std::string response = processMessage(buffer);

		MSG_DEBUG("Sending response: %s to %s", response.c_str(), clientAddress.sun_path);

		if (sendto(m_controllerSocket, response.c_str(), response.length(), 0,
				   (struct sockaddr *) &clientAddress, addressLength) != (uint16_t) response.length()) {
			MSG_ERROR("sendto(): %s", strerror(errno));
		}
	}

	pthread_kill(m_thread.native_handle(), SIGINT);
	m_thread.join();

	MSG_DEBUG("Socket controller stopped");
}

std::string SocketController::processMessage(std::string message)
{
	json::Object response;
	json::Array messages;
	response["status"] = "OK";

	json::Value jMessage = json::Deserialize(message);

	if (jMessage.GetType() != json::ObjectVal) {
		messages.push_back("Invalid message");
		response["status"]   = "Error";
		response["messages"] = messages;
		return json::Serialize(response);
	}

	if (jMessage.HasKey("requests")) {
		if (jMessage["requests"].GetType() != json::ArrayVal) {
			response["status"] = "Error";
			messages.push_back("'requests' element has to be an array");
		} else {

			json::Array requests = jMessage["requests"];
			std::string errorMessage{};

			for (json::Value &request : requests) {

				try {
					errorMessage = processRequest(request.ToObject());
				} catch (std::exception &e) {
					errorMessage = e.what();
				}

				if (!errorMessage.empty()) {
					response["status"]  = "Error";
					messages.push_back(errorMessage);
				}
			}
		}
	}

	if (jMessage.HasKey("save") && response["status"] == "OK") {
		json::Value jSave = jMessage["save"];

		if (jSave.GetType() == json::BoolVal && jSave.ToBool()) {
			m_profiles->saveChanges();
		} else {
			messages.push_back("Changes not saved, invalid value of 'save' element");
		}
	}

	if (messages.size() > 0) {
		response["messages"] = messages;
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
		return "Unknown request type " + type;
	}

	/* Add or modify channel */
	if (channel) {

		if (request.HasKey("name")) {
			/* Check whether the new channel name is unique within profile */
			for (Channel *c: channel->getProfile()->getChannels()) {
				if (c->getName() == request["name"] && c != channel) {
					return "Channel with name " + c->getName() + " already exists in profile " + c->getProfile()->getName();
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
