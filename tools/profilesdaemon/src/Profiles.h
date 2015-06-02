#ifndef PROFILES_H
#define	PROFILES_H


#include "SocketController.h"
#include "pugixml/pugixml.hpp"

#include "Channel.h"
#include "Profile.h"

class SocketController;

class Profiles {
public:
	Profiles(std::string config);
	~Profiles();

	Profile *addProfile(std::string profile);
	Channel *addChannel(std::string channel);

	bool removeProfile(std::string profile);
	bool removeChannel(std::string channel);

	bool saveChanges();

	void setCollectors(SocketController *collectors);

	Profile *getProfile(std::string path);
	Channel *getChannel(std::string path);
	Profile *getParentProfile(std::string path);

	void resetLastError();
	std::string getLastError();

	std::string getXmlConfig();
private:

	Channel *processChannel(Profile *profile, pugi::xml_node config);
	Profile *processProfile(Profile *parent, pugi::xml_node config);

	Profile *nameToProfile(std::string profile);
	Channel *nameToChannel(std::string channel);

	Profile *nameToParentProfile(std::string name);

	std::string nameFromPath(std::string path);


	SocketController *m_collectors{};
	Profile *rootProfile{};
	pugi::xml_document m_doc;
	std::string m_xmlfile{};
	std::string m_lastError{};
};

#endif	/* PROFILES_H */
