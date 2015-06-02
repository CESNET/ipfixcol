/**
 * \file Profile.cpp
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
#include <stdexcept>

#include "Profile.h"
#include "Channel.h"

#include <iostream>

Profile::Profile(std::string name)
	: m_name{name}
{
}

Profile::~Profile()
{
	for (auto& ch: m_channels) {
		delete ch;
	}
	
	for (auto& p: m_children) {
		delete p;
	}
}

void Profile::destroy()
{
	for (Channel *c : m_channels) {
		c->destroy();
	}

	for (Profile *p : m_children) {
		p->destroy();
	}

	if (m_parent != nullptr) {
		m_parent->removeProfile(this);
	}
}

void Profile::addChannel(Channel *channel, bool loadingXml)
{
	for (Channel *c : m_channels) {
		if (c->getName() == channel->getName()) {
			throw std::invalid_argument("Name " + channel->getName() + " is already used in profile " + m_name);
		}
	}

	channel->setProfile(this);
	m_channels.push_back(channel);

	if (!loadingXml) {
		channel->setNode(m_node.append_child("channel"));
	}
}

void Profile::addProfile(Profile* child, bool loadingXml)
{
	for (Profile *p : m_children) {
		if (p->getName() == child->getName()) {
			throw std::invalid_argument("Name " + child->getName() + " is already used in profile " + m_name);
		}
	}

	child->setParent(this);
	m_children.push_back(child);

	if (!loadingXml) {
		child->setNode(m_node.append_child("profile"));
	}
}

void Profile::removeProfile(Profile *child)
{
	auto it = std::find(m_children.begin(), m_children.end(), child);

	if (it != m_children.end()) {
		m_children.erase(it);
	}

	m_node.remove_child(child->getNode());
}

void Profile::removeChannel(Channel *channel)
{
	auto it = std::find(m_channels.begin(), m_channels.end(), channel);

	if (it != m_channels.end()) {
		m_channels.erase(it);
	}

	m_node.remove_child(channel->getNode());
}

void Profile::updatePathName()
{
	if (m_parent) {
		m_pathName = m_parent->getPathName() + m_name + "/";
	} else {
		m_pathName = "";
	}

	for (auto& ch: m_channels) {
		ch->updatePathName();
	}

	for (auto& p: m_children) {
		p->updatePathName();
	}
}

void Profile::updateNodeData()
{
	m_node.attribute("name") = m_name.c_str();
}

void Profile::setNode(pugi::xml_node node)
{
	m_node = node;

	if (!m_node.attribute("name")) {
		m_node.append_attribute("name");
	}

	updateNodeData();
}
