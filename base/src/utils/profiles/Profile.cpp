/**
 * \file Profile.cpp
 * \author Michal Kozubik <kozubik@cesnet.cz>
 * \brief intermediate plugin for profiling data
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

#include "Profile.h"
#include "Channel.h"

/* Numer of profiles (ID for new profiles) */
profile_id_t Profile::profiles_cnt = 1;

/**
 * Constructor
 */
Profile::Profile(std::string name, PROFILE_TYPE type)
: m_id{profiles_cnt++}, m_name{name}, m_type{type}
{
}

/**
 * Destructor
 */
Profile::~Profile()
{
	/* Remove channels */
	for (auto& ch: m_channels) {
		delete ch;
	}
	
	/* Remove children */
	for (auto& p: m_children) {
		delete p;
	}
}

/**
 * Add new channel
 */
void Profile::addChannel(Channel* channel)
{
	m_channels.push_back(channel);
}

/**
 * Add child profile
 */
void Profile::addProfile(Profile* child)
{
	m_children.push_back(child);
}

/**
 * Remove child profile
 */
void Profile::removeProfile(profile_id_t id)
{
	/* Find profile */
	profilesVec::iterator it = std::find_if(m_children.begin(), m_children.end(),
										   [id](Profile *p) { return p->getId() == id;});

	/* Remove it */
	if (it != m_children.end()) {
		m_children.erase(it);
	}
}

/**
 * Remove channel
 */
void Profile::removeChannel(channel_id_t id)
{
	/* Find channel */
	channelsVec::iterator it = std::find_if(m_channels.begin(), m_channels.end(),
											[id](Channel *ch) { return ch->getId() == id;});

	/* Remove it */
	if (it == m_channels.end()) {
		return;
	}

	/* Unsubscribe channel from it's sources */
	Channel *ch = *it;
	for (auto& src: ch->getSources()) {
		src->removeListener(ch);
	}

	// TODO: clear source list and unsubscribe listeners?
	m_channels.erase(it);
}

/**
 * \brief Update path name
 */
void Profile::updatePathName()
{
	if (m_parent) {
		m_pathName = m_parent->getPathName() + m_name + "/";
	} else {
		m_pathName = "";
	}

	/* Update name of channels */
	for (auto& ch: m_channels) {
		ch->updatePathName();
	}

	/* Update name of children */
	for (auto& p: m_children) {
		p->updatePathName();
	}
}

/**
 * Match profile
 */
void Profile::match(ipfix_message* msg, metadata* mdata, std::vector<Channel *>& channels)
{	
	for (auto& channel: m_channels) {
		channel->match(msg, mdata, channels);
	}
}

void Profile::match(struct match_data *data)
{
	for (auto& channel: m_channels) {
		channel->match(data);
	}
}
