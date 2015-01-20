/**
 * \file Organization.h
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

#ifndef ORGANIZATION_H
#define	ORGANIZATION_H

#include "profiler.h"
#include "filter.h"

#define IPV4_LEN 4
#define IPV6_LEN 16

class Rule;

/* shortcuts */
using profileVec = std::vector<struct filter_profile *>;
using ruleVec = std::vector<Rule *>;

/**
 * Class representing organization
 */
class Organization {
public:
    void print();
    /**
     * \brief Constructor
     * 
     * \param[in] id Organization's ID
     */
    Organization(uint32_t id);
    
    /**
     * \brief Destructor
     */
    virtual ~Organization();
    
    /**
     * \brief Get organization ID
     * \return ID
     */
    uint32_t id() { return m_id; }
    
    /**
     * \brief Add new rule
     * 
     * \param[in] doc xml document
     * \param[in] root rule root node
     */
    void addRule(xmlDoc *doc, xmlNode *root);
    
    /**
     * \brief Add new profile
     * 
     * \param[in] pdata data for scanner and parser
     * \param[in] root profile root node
     */
    void addProfile(struct filter_parser_data *pdata, xmlNode *root);
    
    /**
     * \brief Find matching rule for given record
     * 
     * \param[in] msg IPFIX message
     * \param[in] data IPFIX data record
     * \return matching rule (or nullptr)
     */
    Rule *matchingRule(struct ipfix_message *msg, struct ipfix_record *data);
    
    /**
     * \brief Find matching profiles
     * 
     * \param[in] msg IPFIX message
     * \param[in] data data IPFIX data record
     * \return matching profiles
     */
    profileVec matchingProfiles(struct ipfix_message *msg, struct ipfix_record *data);
    
private:
    /**
     * \brief Free xmlChar buffer
     */
    void freeAuxChar();
    
    /**
     * \brief Parse flex/bison filter
     * 
     * \param[in] pdata Parser's data
     * \return 0 on success
     */
    int parseFilter(struct filter_parser_data *pdata);
    
    uint32_t m_id{};        /**< Organization's ID */
    xmlChar *m_auxChar{};   /**< xmlChar buffer */
    
    ruleVec m_rules{};      /**< List of rules */
    profileVec m_profiles{};/**< list of profiles */
};


/**
 * Class representing 1 rule identifying organization
 */
class Rule {
public:
    void print();
    /**
     * \brief Constructor
     * 
     * \param[in] id Rule's ID
     */
    Rule(uint32_t id);
    
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
     * \brief Set data src/dst address prefix
     * 
     * \param[in] prefix IP prefix
     */
    void setPrefix(char *prefix);
    
    /**
     * \brief Match IPFIX data record
     * 
     * \param msg IPFIX message
     * \param data IPFIX data record
     * \return true when rule matches given data record
     */
    bool matchRecord(struct ipfix_message *msg, struct ipfix_record *data);
    
    /**
     * \brief Get rule ID
     * 
     * \return rule ID
     */
    uint32_t id() { return m_id; }
    
private:
    /**
     * \brief Find out IP version
     * 
     * \param[in] ip IP address
     * \return AF_INET or AF_INET6
     */
    static int ipVersion(std::string &ip);
    
    /**
     * \brief Match prefix
     * \param[in] data IPFIX data record
     * \param[in] field field field identifier
     * \return true when prefix matches data record
     */
    bool matchPrefix(struct ipfix_record *data, int field);
    
    uint32_t m_id;              /**< rule ID */
    
    bool m_hasSource{false};    /**< true when source address is set */
    bool m_hasOdid{false};      /**< true when ODID is set */
    bool m_hasPrefix{false};    /**< true when prefix is set */

    int m_sourceIPv{};          /**< packet source IP version */
    int m_prefixIPv{};          /**< prefix IP version */
    uint8_t m_source[IPV6_LEN]{}; /**< source address */
    uint8_t m_prefix[IPV6_LEN]{}; /**< prefix */
    uint16_t m_prefixLen{};     /**< prefix length */
    uint32_t m_odid{};          /**< Observation domain ID */
};

#endif	/* ORGANIZATION_H */

