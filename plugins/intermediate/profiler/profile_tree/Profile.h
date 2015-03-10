/**
 * \file Profile.h
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

#ifndef PROFILE_H
#define	PROFILE_H

#include "profiler.h"

#include <string>
#include <vector>

class Channel;

/**
 * \brief Class representing profile
 */
class Profile {
	/* Shortcuts */
	using channelsVec = std::vector<Channel *>;
	using profilesVec = std::vector<Profile *>;
public:
	/**
	 * \brief Constructor
	 *
	 * \param[in] name profile name
	 */
	Profile(std::string name);
	
	/**
	 * \brief Destructor
	 */
	~Profile();

	/**
	 * \brief Add new child profile
	 *
	 * \param[in] child child profile
	 */
	void addProfile(Profile *child);
	
	/**
	 * \brief Remove child profile
	 *
	 * \param[in] id id of profile
	 */
	void removeProfile(profile_id_t id);

	/**
	 * \brief Remove channel
	 *
	 * \param[in] id channel id
	 */
	void removeChannel(channel_id_t id);

	/**
	 * \brief Add new channel to profile
	 *
	 * \param[in] channel new channel
	 */
	void addChannel(Channel *channel);

	/**
	 * \brief Get profile's ID
	 *
	 * \return profile's ID (unique within all profiles)
	 */
	profile_id_t getId() { return m_id; }
	
	/**
	 * \brief Get profile's name
	 *
	 * \return profile's name from startup configuration
	 */
	std::string getName() { return m_name; }
	
	/**
	 * \brief Get vector of all profile's channels
	 *
	 * \return vector of channels
	 */
	channelsVec getChannels() { return m_channels; }
	
	/**
	 * \brief Get vector of all profile's child profiles
	 *
	 * \return vector of profiles
	 */
	profilesVec getChildren() { return m_children; }
	
	/**
	 * \brief Get parent profile
	 *
	 * \return parent profile
	 */
	Profile *getParent() { return m_parent; }

	/**
	 * \brief Set parent profile
	 *
	 * \param[in] parent parent profile
	 */
	void setParent(Profile *parent) { m_parent = parent; }
	
	/**
	 * \brief Match profile with data record (== with it's channels)
	 *
	 * \param[in] msg IPFIX message
	 * \param[in] mdata Data record's metadata
	 * \param[out] profiles	list of matching profiles and channels
	 */
	void match(struct ipfix_message *msg, struct metadata *mdata, std::vector<couple_id_t>& profiles);
private:

	Profile *m_parent;		/**< Parent profile */

	profile_id_t m_id;		/**< Profile ID */
	std::string m_name;		/**< Profile name */
	profilesVec m_children;	/**< Children */
	channelsVec m_channels;	/**< Channels */
	
	static profile_id_t profiles_cnt;	/**< Total number of profiles */
};

#endif	/* PROFILE_H */

