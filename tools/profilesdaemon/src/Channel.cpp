/**
 * \file Channel.cpp
 * \author Michal Kozubik <kozubik@cesnet.cz>
 * \brief
 *
 * Copyright (C) 2015 CESNET, z.s.p.o.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of the Company nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * ALTERNATIVELY, provided that this notice is retained in full, this
 * product may be distributed under the terms of the GNU General Public
 * License (GPL) version 2 or later, in which case the provisions
 * of the GPL apply INSTEAD OF those given above.
 *
 * This software is provided ``as is, and any express or implied
 * warranties, including, but not limited to, the implied warranties of
 * merchantability and fitness for a particular purpose are disclaimed.
 * In no event shall the company or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even
 * if advised of the possibility of such damage.
 *
 */

#include <algorithm>
#include <sstream>
#include <stdexcept>
#include "Channel.h"
#include "verbose.h"

#include <iostream>

/**
 * Trim string
 */
void trim(std::string& str)
{
	std::string::size_type pos = str.find_last_not_of(' ');
	
	if (pos != std::string::npos) {
		str.erase(pos + 1);
		pos = str.find_first_not_of(' ');
		
		if (pos != std::string::npos) {
			str.erase(0, pos);
		}
	} else {
		str.erase(str.begin(), str.end());
	}
}

Channel::Channel(std::string name)
	: m_name{name}, m_pathName{name}
{
}

Channel::~Channel()
{
}

void Channel::destroy()
{
	for (Channel *c : m_listeners) {
		c->removeSource(this);
	}

	for (Channel *c : m_sources) {
		c->removeListener(this);
	}

	m_profile->removeChannel(this);
}

void Channel::updateNodeData()
{
	std::string sources{};

	if (!m_sources.empty()) {
		for (Channel *c : m_sources) {
			sources += c->getName() + ",";
		}

		sources[sources.length() - 1] = '\0';
	}

	m_node.child("sources").text() = sources.c_str();
	m_node.child("filter").text() = m_filter.c_str();
	m_node.attribute("name") = m_name.c_str();
}

void Channel::setSources(std::string sources)
{	
	std::istringstream iss(sources);
	std::string channel;
	
	/* TOP channel, ignore any source specification */
	if (!m_profile->getParent()) {
		return;
	}
	
	/* Process each source in comma separated list */
	while (std::getline(iss, channel, ',')) {
		/* Remove whitespaces */
		trim(channel);
		if (channel.empty()) {
			continue;
		}
		
		/* Process data from all channels */
		if (channel == "*") {
			for (auto& ch: m_profile->getParent()->getChannels()) {
				ch->addListener(this);
				m_sources.insert(ch);
			}
			continue;
		}
		
		/* Find channel in parent profile */
		Channel *src{};
		for (auto& ch: m_profile->getParent()->getChannels()) {
			if (ch->getName() == channel) {
				src = ch;
				break;
			}
		}
		
		/* Source not found */
		if (!src) {
			throw std::runtime_error("Channel " + m_name + ": no " + channel + " channel in parent profile " + m_profile->getParent()->getName());
		}
		
		m_sources.insert(src);
		src->addListener(this);
	}
}

void Channel::addSource(Channel *channel)
{
	if (m_sources.find(channel) != m_sources.end()) {
		return;
	}

	m_sources.insert(channel);
	updateNodeData();
}

void Channel::removeSource(Channel *channel)
{
	if (m_sources.find(channel) == m_sources.end()) {
		return;
	}

	m_sources.erase(channel);
	updateNodeData();
}

void Channel::addListener(Channel* listener)
{
	m_listeners.insert(listener);
}

void Channel::removeListener(Channel *child)
{
	channelsSet::iterator it = std::find(m_listeners.begin(), m_listeners.end(), child);

	if (it != m_listeners.end()) {
		m_listeners.erase(it);
	}
}

void Channel::setProfile(Profile* profile)
{
	m_profile = profile;
}

void Channel::setFilter(std::string filter)
{
	m_filter = filter;
	updateNodeData();
}

void Channel::setName(std::string name)
{
	m_name = name;
	updateNodeData();
}

void Channel::setNode(pugi::xml_node node)
{
	m_node = node;

	if (!m_node.child("sources")) {
		m_node.append_child("sources");
	}

	if (!m_node.child("filter")) {
		m_node.append_child("filter");
	}

	if (!m_node.attribute("name")) {
		m_node.append_attribute("name");
	}

	updateNodeData();
}

void Channel::updatePathName()
{
	if (m_profile) {
		m_pathName = m_profile->getPathName() + "channels/";
	} else {
		m_pathName = "";
	}
}
