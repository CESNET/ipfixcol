/**
 * \file Channel.cpp
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
#include <sstream>
#include "Channel.h"

/* Numer of channels (ID for new channels) */
channel_id_t Channel::channels_cnt = 1;

/* Identifier for verbose macros */
static const char *msg_module = "profiles";

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

/**
 * Constructor
 */
Channel::Channel(std::string name)
: m_id{channels_cnt++}, m_name{name}, m_pathName{name}
{
}

/**
 * Destructor
 */
Channel::~Channel()
{
	/* Delete filter */
	if (m_filter) {
		filter_free_profile(m_filter);
	}
}

/**
 * Set channel sources
 */
void Channel::setSources(std::string sources)
{	
	std::istringstream iss(sources);
	std::string channel;
	
	/* TOP channel, ignore any source specification */
	if (!m_profile->getParent()) {
		if (sources != "*") {
			MSG_INFO(msg_module, "Ignoring source specification on top channel %s", m_name.c_str());
		}
		return;
	}
	
	/* Process each source in comma separated list */
	while (std::getline(iss, channel, ',')) {
		/* Remove whitespaces */
		trim(channel);
		
		/* Process data from all channels */
		if (channel == "*") {
			for (auto& ch: m_profile->getParent()->getChannels()) {
				ch->addListener(this);
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
			MSG_ERROR(msg_module, "Channel %s: no %s channel in parent profile %s", m_name.c_str(), channel.c_str(), m_profile->getParent()->getName().c_str());
			throw std::runtime_error(std::string(""));
		}
		
		/* Subscribe as a listener */
		src->addListener(this);
	}
}

/**
 * Add new listener
 */
void Channel::addListener(Channel* listener)
{
	m_listeners.insert(listener);
	listener->m_sources.insert(this);
}

void Channel::removeListener(Channel *child)
{
	/* Find listener */
	channelsSet::iterator listen_it = std::find(m_listeners.begin(),
		m_listeners.end(), child);

	/* Remove it */
	if (listen_it != m_listeners.end()) {
		m_listeners.erase(listen_it);
	}

	/* Find this channel in listener's sources */
	channelsSet::iterator source_it = std::find(child->m_sources.begin(),
		child->m_sources.end(), this);

	/* Remove it */
	if (source_it != child->m_sources.end()) {
		child->m_sources.erase(source_it);
	}
}

void Channel::removeListener(channel_id_t id)
{
	/* Find listener */
	channelsSet::iterator listen_it = std::find_if(m_listeners.begin(),
		m_listeners.end(), [id](Channel *ch) { return ch->getId() == id;});

	if (listen_it == m_listeners.end()) {
		return;
	}

	/* Find this channel in listener's sources */
	Channel *listener = *listen_it;
	channelsSet::iterator source_it = std::find(listener->m_sources.begin(),
		listener->m_sources.end(), this);

	/* Remove both of them */
	if (source_it != listener->m_sources.end()) {
		listener->m_sources.erase(source_it);
	}

	m_listeners.erase(listen_it);
}

/**
 * Set channel profile
 */
void Channel::setProfile(Profile* profile)
{
	m_profile = profile;
}

/**
 * Set channel filter
 */
void Channel::setFilter(filter_profile* filter)
{
	m_filter = filter;
}

/**
 * Update path name
 */
void Channel::updatePathName()
{
	if (m_profile) {
		m_pathName = m_profile->getPathName() + "channels/";
	} else {
		m_pathName = "";
	}
}

/**
 * Match channel filter to data record
 */
void Channel::match(ipfix_message* msg, metadata* mdata, std::vector<Channel *>& channels)
{
	if (m_filter && !filter_fits_node(m_filter->root, msg, &(mdata->record))) {
		return;
	}
	
	/* Mark channel into metadata */
	channels.push_back(this);
	
	/* Process all listeners */
	for (auto& child: m_listeners) {
		child->match(msg, mdata, channels);
	}
}

void Channel::match(struct match_data *data)
{
	if (m_filter && !filter_fits_node(m_filter->root, data->msg, &(data->mdata->record))) {
		return;
	}

	if (data->channelsMax == 0) {
		data->channelsMax = 5;
		data->channels = (void**) calloc(data->channelsMax, sizeof(void*));
		if (data->channels == NULL) {
			MSG_ERROR(msg_module, "Unable to allocate memory (%s:%d)", __FILE__, __LINE__);
			return;
		}
	} else if (data->channelsCounter == data->channelsMax) {
		data->channels = (void**) realloc(data->channels, sizeof(void*) * data->channelsMax * 2);
		if (data->channels == NULL) {
			MSG_ERROR(msg_module, "Unable to allocate memory (%s:%d)", __FILE__, __LINE__);
			return;
		}

		memset(&(data->channels[data->channelsMax]), 0, sizeof(void*) * data->channelsMax);

		data->channelsMax *= 2;
	}

	data->channels[data->channelsCounter++] = (void*) this;


	for (auto& child: m_listeners) {
		child->match(data);
	}
}
