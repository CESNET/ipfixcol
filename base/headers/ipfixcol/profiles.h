/**
 * \file profiles.h
 * \author Michal Kozubik <kozubik@cesnet.cz>
 * \brief Functions unrelated to IPFIX data parsing
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

#ifndef PROFILES_H
#define	PROFILES_H

#include "api.h"
#include "storage.h"

/**
 * \brief Type of a profile
 */
enum PROFILE_TYPE {
	PT_UNDEF,
	PT_NORMAL,  /**< Real profile - store data           */
	PT_SHADOW   /**< Shadow profile - do not store data  */
};

/**
 * \brief Process xml file with profiles configuration
 *
 * \param[in] file path to the xml file
 * \return live profile pointer
 */
API void *profiles_process_xml(const char *file);

/**
 * \brief Get path to the profiles.xml file
 *
 * \return path to XML
 */
API const char *profiles_get_xml_path();

/* ==== PROFILE ==== */
/**
 * \brief Get profile name
 *
 * \param[in] profile
 * \return profile name
 */
API const char *profile_get_name(void *profile);

/**
 * \brief Get profile type
 *
 * \param[in] profile
 * \return profile type
 */
API enum PROFILE_TYPE profile_get_type(void *profile);

/**
 * \brief Get profile directory
 *
 * \param[in] profile
 * \return profile directory
 */
API const char *profile_get_directory(void *profile);

/**
 * \brief Get profile path
 *
 * \param[in] profile
 * \return profile path
 */
API const char *profile_get_path(void *profile);

/**
 * \brief Get number of profile children
 *
 * \param[in] profile
 * \return number of children
 */
API uint16_t profile_get_children(void *profile);

/**
 * \brief Get number of profile channels
 *
 * \param[in] profile
 * \return number of channels
 */
API uint16_t profile_get_channels(void *profile);

/**
 * \brief Get parent profile
 *
 * \param[in] profile
 * \return parent profile
 */
API void *profile_get_parent(void *profile);

/**
 * \brief Get child on given index
 *
 * \param[in] profile
 * \param[in] index child index
 * \return child profile
 */
API void *profile_get_child(void *profile, uint16_t index);

/**
 * \brief Get channel on given index
 *
 * \param[in] profile
 * \param[in] index channel index
 * \return channel
 */
API void *profile_get_channel(void *profile, uint16_t index);

/**
 * \brief Match profile with data record
 *
 * Data record is recursively matched with each channel sub-profile
 * Each matching channel is stored into an array
 * Profiles are NOT stored (they're accessible by calling
 * channel_get_profile on matched channel)
 *
 * \param[in] profile
 * \param[in] msg IPFIX message
 * \param[in] mdata Data record's metadata
 * \return list of matching channels
 */
API void **profile_match_data(void *profile, struct ipfix_message *msg, struct metadata *mdata);

/**
 * \brief Get all profiles in the tree
 *
 * \warning User must free returned array with free()
 * \param[in] profile Random profile from the tree of profiles.
 * \return On success returns null-terminated array of all profiles. Otherwise
 * returns NULL.
 */
API void **profile_get_all_profiles(void *profile);

/**
 * \brief Free profile with all it's channels and subprofiles
 *
 * \param[in] profile
 */
API void profiles_free(void *profile);

/* ==== CHANNEL ==== */
/**
 * \brief Get channel name
 *
 * \param[in] channel
 * \return channel name
 */
API const char *channel_get_name(void *channel);

/**
 * \brief Get channel path
 *
 * \param[in] channel
 * \return channel path
 */
API const char *channel_get_path(void *channel);

/**
 * \brief Get channel profile
 *
 * \param[in] channel
 * \return profile
 */
API void *channel_get_profile(void *channel);

/**
 * \brief Get number of channel listeners
 *
 * \param[in] channel
 * \return number of listening channels
 */
API uint16_t channel_get_listeners(void *channel);

/**
 * \brief Get number of data sources
 *
 * \param[in] channel
 * \return number of source channels
 */
API uint16_t channel_get_sources(void *channel);

#endif	/* PROFILES_H */

