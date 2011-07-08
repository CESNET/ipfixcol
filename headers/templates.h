/**
 * \file templates.h
 * \author Radek Krejci <rkrejci@cesnet.cz>
 * \author Michal Srb <michal.srb@cesnet.cz>
 * \brief Public structures and functions (API) of the ipfixcol's Template
 * Manager
 *
 * Copyright (C) 2011 CESNET, z.s.p.o.
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

/**
 * \defgroup templateMngAPI Template Manager API
 * \ingroup publicAPIs
 *
 * These functions should be used to work with Template Manager. Template
 * Manager is unique for every Data Manager and is represented by \link
 * #ipfix_template_mgr_t ipfix_template_mgr_t structure\endlink. Its job
 * is to manage (Options) Templates.
 *
 *
 * @{
 */
#ifndef IPFIXCOL_TEMPLATES_H_
#define IPFIXCOL_TEMPLATES_H_

/**
 * \def TM_OPTIONS_TEMPLATE
 * \brief Template manager's options template number
 */
#define TM_OPTIONS_TEMPLATE 1

/**
 * \def TM_TEMPLATE
 * \brief Template manager's template number
 */
#define TM_TEMPLATE 0

/**
 * \def TM_TEMPLATE_ALL
 * \brief Template manager's templates and options templates
 */
#define TM_TEMPLATE_ALL 2

/**
 * \struct ipfix_template_t
 * \brief Structure for storing Template Record/Options Template Record
 *
 * All data in this structure are in host byte order
 */
struct ipfix_template {
	uint8_t template_type;       /**Type of Template - TM_TEMPLATE = Template,
	                              * TM_OPTIONS_TEMPLATE = Options Template */
	time_t last_transmission;    /**Time of last transmission of Template,
	                              * UDP only */
	uint16_t template_id;        /**Template ID */
	uint16_t field_count;        /**Number of fields in Template Record */
	uint16_t scope_field_count;  /**Number of scope fields */
	uint16_t template_length;    /**Length of the template */
	template_ie fields[1];       /**Template fields */
};

/**
 * \struct ipfix_template_mgr_t
 * \brief Template Manager structure.
 */
struct ipfix_template_mgr {
	struct ipfix_template **templates;/**array of pointers to Templates */
	uint16_t max_length;  /**maximum length of array */
	uint16_t counter;     /**number of templates in array */
};

/**
 * \brief Function for adding new templates.
 *
 * \param[in]  tm Data Manager specific structure for storing Templates.
 * \param[in]  template Pointer where new Template Record starts.
 * \param[in]  type Type of the Template Record. 0 = Template, 1 = Options
 * Template.
 * \return Pointer to new ipfix_template on success, NULL otherwise
 */
struct ipfix_template *tm_add_template(struct ipfix_template_mgr *tm,
										void *template, int type);

/**
 * \brief Function for updating an existing templates.
 *
 * \param[in]  tm Data Manager specific structure for storing Templates.
 * \param[in]  template Pointer where new Template Record starts.
 * \param[in]  type Type of the Template Record. 0 = Template, 1 = Options
 * Template.
 * \return updated ipfix_template on success, NULL if error occurs.
 */
struct ipfix_template *tm_update_template(struct ipfix_template_mgr *tm,
											void *template, int type);

/**
 * \brief Function for specific Template lookup.
 *
 * \param[in]  tm Data Manager specific structure for storing Templates.
 * \param[in]  template_id ID of the Template that we are interested in.
 * \return pointer on the Temaplate on success, NULL if there is no such
 * Temaplate.
 */
struct ipfix_template *tm_get_template(struct ipfix_template_mgr *tm,
                                         uint16_t template_id);

/**
 * \brief Function for removing Temaplates.
 *
 * \param[in]  tm Data Manager specific structure for storing Templates.
 * \param[in]  template_id ID of the Template that we want to remove.
 * \return 0 on success, negative value otherwise.
 */
int tm_remove_template(struct ipfix_template_mgr *tm,
                       uint16_t template_id);

/**
 * \brief Function for removing all Temaplates.
 *
 * \param[in]  tm Data Manager specific structure for storing Templates.
 * \return 0 on success, negative value otherwise.
 */
int tm_remove_all_templates(struct ipfix_template_mgr *tm);


/**
 * \brief Create new template manager and set default values
 *
 * @return struct ipfix_template_manager New template manager
 */
struct ipfix_template_mgr *tm_create();


/**
 * \brief Destroys and frees specified template manager
 *
 * @param[in] ipfix_template_mgr Template manager to be destroyed
 * @return void
 */
void tm_destroy(struct ipfix_template_mgr *tm);


#endif /* IPFIXCOL_TEMPLATES_H_ */

/**@}*/
