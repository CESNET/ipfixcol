/**
 * \file Rule.h
 * \author Michal Kozubik <kozubik@cesnet.cz>
 * \brief Class for profiler intermediate plugin
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

#ifndef RULE_H
#define	RULE_H

#define IPV4_LEN 4
#define IPV6_LEN 16

#include <string>

/**
 * Class representing 1 rule identifying organization
 */
class Rule {
public:
    void print(); /* DEBUG */
    /**
     * \brief Constructor
     * 
     * \param[in] id Rule's ID
     */
    Rule(uint32_t id);
	
	/**
	 * \brief Destructor
     */
	~Rule();
    
    /**
     * \brief Set ODID value
     * 
     * \param[in] odid ODID value
     */
    void setOdid(char *odid);
    
    /**
     * \brief Set packet source address
     * 
     * \param[in] ip packet source IP address
     */
    void setSource(char *ip);
    
	/**
	 * \brief Set data filter
	 * 
     * \param[in] filter data filter
     */
	void setDataFilter(struct filter_profile *filter);
	
    /**
     * \brief Match IPFIX data record
     * 
     * \param msg IPFIX message
     * \param data IPFIX data record
     * \return true when rule matches given data record
     */
    bool matchRecord(struct ipfix_message *msg, struct ipfix_record *data) const;
    
    /**
     * \brief Get rule ID
     * 
     * \return rule ID
     */
    uint32_t id() const { return m_id; }
	
	/**
	 * \brief Check whether rule is valid
	 * 
     * \return true on success
     */
	bool isValid() const;
    
private:
    /**
     * \brief Find out IP version
     * 
     * \param[in] ip IP address
     * \return AF_INET or AF_INET6
     */
    static int ipVersion(std::string &ip);
	
	struct filter_profile *m_filter{}; /**< dataFilter profile */
	
    uint32_t m_id;					/**< rule ID */
    
    bool m_hasSource{false};		/**< true when source address is set */
    bool m_hasOdid{false};			/**< true when ODID is set */
	
    uint8_t m_sourceIPv{};			/**< packet source IP version */
    uint8_t m_source[IPV6_LEN]{};	/**< source address */
    uint32_t m_odid{};				/**< Observation domain ID */
};

#endif	/* RULE_H */
