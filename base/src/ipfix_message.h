/**
 * \file ipfix_message.h
 * \author Michal Srb <michal.srb@cesnet.cz>
 * \brief Auxiliary function for working with IPFIX messages
 *
 * Copyright (C) 2012 CESNET, z.s.p.o.
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
 * \defgroup Auxiliary functions to work with IPFIX message
 * \ingroup ipfixmedCore
 *
 *
 * @{
 */

#ifndef IPFIX_MESSAGE_H_
#define IPFIX_MESSAGE_H_

#include <ipfixcol.h>

/**
 * \brief Create ipfix_message structure from data in memory
 *
 * \param[in] msg memory containing IPFIX message
 * \param[in] len length of the IPFIX message
 * \param[in] input_info information structure about input
 * \return ipfix_message structure on success, NULL otherwise
 */
struct ipfix_message *message_create_from_mem(void *msg, int len, struct input_info* input_info);


/**
 * \brief Set corresponding templates for data records in IPFIX message.
 *
 * \param[in] msg IPFIX message
 * \param[in] tm template manager with corresponding templates
 * \return 0 on success, negative value otherwise
 */
int message_set_templates(struct ipfix_message *msg, struct ipfix_template_mgr *tm);


/**
 * \brief Copy IPFIX message
 *
 * \param[in] msg original IPFIX message
 * \return copy of original IPFIX message on success, NULL otherwise
 */
struct ipfix_message *message_create_clone(struct ipfix_message *msg);


/**
 * \brief Create empty IPFIX message
 *
 * \return new instance of ipfix_message structure.
 */
struct ipfix_message *message_create_empty();

/**
 * \brief Get data from IPFIX message.
 *
 * \param[out] dest where to copy data from message
 * \param[in] source from where to copy data from message
 * \param[in] len length of the data
 * \return 0 on success, negative value otherwise
 */
int message_get_data(uint8_t **dest, uint8_t *source, int len);


/**
 * \brief Set data to IPFIX message.
 *
 * \param[out] dest where to write data
 * \param[in] source actual data
 * \param[in] len length of the data
 * \return 0 on success, negative value otherwise
 */
int message_set_data(uint8_t *dest, uint8_t *source, int len);


/**
 * \brief Get pointers to start of the Data Records in specific Data Set
 *
 * \param[in] data_set data set to work with
 * \param[in] template template for data set
 * \return array of pointers to start of the Data Records in Data Set
 */
uint8_t **get_data_records(struct ipfix_data_set *data_set, struct ipfix_template *template);


/**
 * \brief Get offset where next data record starts
 *
 * \param[in] data_record data record
 * \param[in] template template for data record
 * \return offset of next data record in data set
 */
uint16_t get_next_data_record_offset(uint8_t *data_record, struct ipfix_template *template);


/**
 * \brief Dispose IPFIX message
 *
 * \param[in] msg IPFIX message to dispose
 * \return 0 on success, negative value otherwise
 */
int message_free(struct ipfix_message *msg);


#endif /* IPFIX_MESSAGE_H_ */

/**@}*/

