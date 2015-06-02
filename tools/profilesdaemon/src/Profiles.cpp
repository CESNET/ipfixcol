#include "Profiles.h"
#include "Profile.h"
#include "Channel.h"
#include "verbose.h"

#include <iostream>
#include <stdexcept>
#include <sstream>

std::string Profiles::getXmlConfig()
{
	std::ostringstream stream;
	m_doc.save(stream, "\t", pugi::format_indent | pugi::format_no_declaration);

	return stream.str();
}

Profiles::Profiles(std::string config)
	: m_xmlfile{config}
{
	pugi::xml_parse_result result = m_doc.load_file(config.c_str());

	if (!result) {
		throw std::invalid_argument(result.description());
	}

	rootProfile = processProfile(nullptr, m_doc.select_single_node("/profile").node());
}

Profiles::~Profiles()
{
	delete rootProfile;
}

Profile *Profiles::addProfile(std::string profile)
{
	Profile *newProfile{nullptr};
	Profile *parent = nameToParentProfile(profile);

	if (parent != nullptr) {
		std::string name = nameFromPath(profile);

		newProfile = new Profile(name);
		parent->addProfile(newProfile);
	}

	return newProfile;
}

Channel *Profiles::addChannel(std::string channel)
{
	Channel *newChannel{nullptr};
	Profile *profile = nameToParentProfile(channel);

	if (profile != nullptr) {
		std::string name = nameFromPath(channel);

		newChannel = new Channel(name);
		profile->addChannel(newChannel);
	}

	return newChannel;
}

bool Profiles::removeChannel(std::string path)
{
	Channel *channel = nameToChannel(path);

	if (channel == nullptr) {
		return false;
	}

	channel->destroy();
	delete channel;
	return true;
}

bool Profiles::removeProfile(std::string path)
{
	Profile *profile = nameToProfile(path);

	if (profile == nullptr) {
		return false;
	}

	profile->destroy();
	delete profile;
	return true;
}

bool Profiles::saveChanges()
{
	m_doc.save_file(m_xmlfile.c_str(), "\t", pugi::format_indent | pugi::format_no_declaration);
	m_collectors->sendConfigToAll();
	return true;
}

Channel *Profiles::nameToChannel(std::string path)
{
	Profile *profile = nameToParentProfile(path);
	if (profile == nullptr) {
		return nullptr;
	}

	std::string name = nameFromPath(path);
	for (Channel *c : profile->getChannels()) {
		if (c->getName() == name) {
			return c;
		}
	}

	return nullptr;
}

Profile *Profiles::nameToProfile(std::string path)
{
	Profile::profilesVec children{rootProfile};

	while (true) {
		size_t slash = path.find_first_of('/');

		std::string name = (slash == path.npos) ? path : path.substr(0, slash);
		bool found = false;
		for (Profile *p : children) {
			if (p->getName() == name) {
				if (slash == path.npos) {
					return p;
				}

				children = p->getChildren();
				path = path.substr(slash + 1);

				found = true;
				break;
			}
		}

		if (!found) {
			return nullptr;
		}
	}
}

Profile *Profiles::nameToParentProfile(std::string name)
{
	return nameToProfile(name.substr(0, name.find_last_of('/')));
}

Channel *Profiles::processChannel(Profile *profile, pugi::xml_node config)
{
	std::string name = config.attribute("name").value();

	if (name.empty()) {
		throw std::invalid_argument("Missing channel name");
	}
	
	/* Create new channel */
	Channel *channel = new Channel(name);
	channel->setProfile(profile);
	channel->setFilter(config.child_value("filter"));
	channel->setSources(config.child_value("sources"));
	channel->setNode(config);
	
	return channel;
}

Profile *Profiles::processProfile(Profile *parent, pugi::xml_node config)
{
	std::string name = config.attribute("name").value();

	if (name.empty()) {
		throw std::runtime_error("Missing profile name");
	}
	
	/* Create new profile */
	Profile *profile = new Profile(name);
	profile->setParent(parent);
	profile->setNode(config);

	pugi::xpath_node_set profiles = config.select_nodes("profile");
	pugi::xpath_node_set channels = config.select_nodes("channel");

	for (auto& node: channels) {
		Channel *channel = processChannel(profile, node.node());
		profile->addChannel(channel, true);
	}

	for (auto& node: profiles) {
		Profile *child = processProfile(profile, node.node());
		profile->addProfile(child, true);
	}

	return profile;
}

std::string Profiles::nameFromPath(std::string path)
{
	return path.substr(path.find_last_of('/') + 1);
}

void Profiles::setCollectors(SocketController *collectors)
{
	m_collectors = collectors;
}

Profile *Profiles::getProfile(std::string path)
{
	return nameToProfile(path);
}

Profile *Profiles::getParentProfile(std::string path)
{
	return nameToParentProfile(path);
}

Channel *Profiles::getChannel(std::string path)
{
	return nameToChannel(path);
}

void Profiles::resetLastError()
{
	m_lastError.clear();
}

std::string Profiles::getLastError()
{
	return m_lastError;
}
