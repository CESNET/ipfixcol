/**
 * \file storage/forwarding/templates.h
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Template manager for forwarding plugin (header file)
 *
 * Copyright (C) 2016 CESNET, z.s.p.o.
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

#ifndef TEMPLATES_H
#define TEMPLATES_H

#include <ipfixcol.h>
#include <stdint.h>

/**
 * \defgroup templates Templates manager for the forwarding plugin
 * \ingroup forwardingStoragePlugin
 *
 * @{
 */

/**
 * Functions for remapping Template IDs of multiple independent Flow Sources
 * to new IDs shared among the Flow Sources with the same Observation Domain ID.
 *
 * How to use:
 *   -# tmplts_create();
 *   -# Process an IPFIX message (keep an order of Sets in the message):
 *      For EACH template in a Template Set:
 *	    - tmplts_process_template();
 *      For Data Sets:
 *      - tmplts_remap_data_set();
 *   -# End of the message:
 *      If no templates were in the message, goto the 4. step. Otherwise create
 *      manually Template withdrawal Sets for Normal and Options Templates using
 *      tmplts_withdraw_ids() for both types of templates.
 *   -# New message? Go to the 2. step
 *   -# tmplts_destroy();
 *
 * Warning:
 * It's necessary to call the function tmplts_withdraw_ids() after(!)
 * processing an IPFIX message that has some templates, because some templates
 * could be withdrawed and until IDs (provided by the function) are retrieved
 * by a user, templates remain in the manager and block IDs for new templates.
 * In the worst case it can cause that there will not be not enough free shared
 * IDs for new templates.
 *
 * !!!! IMPLEMENTATION NOTE !!!!
 * For indentification of a flow source (i.e. an exporter) is used structure
 * "input_info". A pointer to this structure is used almost always as a key,
 * so it should be easy to change it for different data type.
 *
 */

/** Prototype */
typedef struct _fwd_tmplt_mgr fwd_tmplt_mgr_t;

/** 
 * \enum TMPLT_MGR_ACTION
 * \brief Action after template processing 
 */
enum TMPLT_MGR_ACTION {
	TMPLT_ACT_INVALID, /**< Invalid operation                        */
	TMPLT_ACT_PASS,    /**< Pass a template (possibly new ID )       */
	TMPLT_ACT_DROP     /**< Drop a template                          */
};

/**
 * \brief (Options) Template
 *
 * Internal representation of an IPFIX template shared among multiple
 * flow sources with same Observation domain ID (ODID).
 */
typedef struct fwd_tmplt_s {
	/** Template ID                                                    */
	uint16_t id;
	/** Type of the template (TM_TEMPLATE or TM_OPTIONS_TEMPLATE)      */
	int type;

	/** Raw template data                                              */
	struct ipfix_template_record *rec;
	/** Data length                                                    */
	size_t length;

	/** Reference counter (number of sources that use this template)   */
	unsigned int ref_cnt;
} fwd_tmplt_t;


/**
 * \brief Create a template manager
 * \return Pointer or NULL
 */
fwd_tmplt_mgr_t *tmplts_create();

/**
 * \brief Destroy a template manager
 */
void tmplts_destroy(fwd_tmplt_mgr_t *mgr);

/**
 * \brief Process a template record
 *
 * Parse the record and modify intenal storage of template(s).
 * \param[in,out] mgr Template manager
 * \param[in] src    Input info from the IPFIX message
 * \param[in] rec    Template record
 * \param[in] type   Type of the template (TM_TEMPLATE or TM_OPTIONS_TEMPLATE)
 * \param[in] length Length of the template record
 * \param[out] new_id New ID (only valid when the return code is TMPLT_ACT_PASS)
 * \return Typy of operation with the template record.
 */
enum TMPLT_MGR_ACTION tmplts_process_template(fwd_tmplt_mgr_t *mgr,
	const struct input_info *src, const struct ipfix_template_record *rec,
	int type, size_t length, uint16_t *new_id);

/**
 * \brief Get new Set ID of a Data Set
 *
 * Find mapping for flow source & ODID & original Data Set ID to new Data Set ID
 * (corresponding to equivalent template) shared among all flow sources
 * \param[in] mgr    Template manager
 * \param[in] src    Input info from the IPFIX message
 * \param[in] header Data Set header
 * \return On error returns 0 (usually caused by unknown mapping of original
 *   Set ID of the Data Set). Otherwise returns new Data Set ID (> 256).
 */
uint16_t tmplts_remap_data_set(fwd_tmplt_mgr_t *mgr,
	const struct input_info *src, const struct ipfix_set_header *header);

/**
 * \brief Remove a flow source and its mapping from the template manager
 *
 * After this function, use tmplts_get_withdrawal_template() to get IDs
 * of templates to withdraw.
 * \param[in,out] mgr  Template manager
 * \param[in]     src  Input info from the IPFIX message
 * \return On success returns 0. Otherwise returns non-zero value.
 */
int tmplts_remove_source(fwd_tmplt_mgr_t *mgr, const struct input_info *src);

/**
 * \brief Get a Template IDs of templates to withdraw
 *
 * \param[in,out] mgr Template manager
 * \param[in]  odid Observation Domain ID
 * \param[in]  type Type of the template (TM_TEMPLATE or TM_OPTIONS_TEMPLATE)
 * \param[out] cnt  A number of returned IDs
 * \return On success returns a pointer to an array of Template IDs to withdraw
 *   and fills the \p cnt. Otherwise returns NULL and a value of the \p cnt is
 *   undefined.
 * \warning User MUST free the array by calling function free.
 */
uint16_t *tmplts_withdraw_ids(fwd_tmplt_mgr_t *mgr, uint32_t odid, int type,
	uint16_t *cnt);

/**
 * \brief Get templates defined by an ODID and a type
 * \param[in]  mgr  Template manager
 * \param[in]  odid Observation Domain
 * \param[in]  type Type of the templates (TM_TEMPLATE or TM_OPTIONS_TEMPLATE)
 * \param[out] cnt  A number of returned templates
 * \return On success returns a pointer to an array of pointers to the templates
 *   and fills the \p cnt. Otherwise returns NULL and a value of the \p cnt is
 *   undefined.
 * \warning User MUST free the array by calling function free.
 */
fwd_tmplt_t **tmplts_get_templates(fwd_tmplt_mgr_t *mgr, uint32_t odid,
	int type, uint16_t *cnt);

/**
 * \brief Get all Observation Domain IDs (ODIDs)
 * \param[in]  mgr Template manager
 * \param[out] cnt A number of returned ODIDs
 * \return On success returns a pointer to an array of the ODIDs and fills
 *   the \p cnt. Otherwise returns NULL and a value of the \p cnt is undefined.
 * \warning User MUST free the array by calling function free.
 */
uint32_t *tmplts_get_odids(const fwd_tmplt_mgr_t *mgr, uint32_t *cnt);


#endif // TEMPLATES_H

/**@}*/
