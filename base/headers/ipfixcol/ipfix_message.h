/**
 * \file ipfix_message.h
 * \author Michal Srb <michal.srb@cesnet.cz>
 * \brief Auxiliary function for working with IPFIX messages
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

/**
 * \defgroup Auxiliary functions to work with IPFIX message
 * \ingroup ipfixmedCore
 *
 *
 * @{
 */

#ifndef IPFIX_MESSAGE_H_
#define IPFIX_MESSAGE_H_

#include "api.h"
#include "input.h"
#include "templates.h"
#include "storage.h"

/**
 * \brief Structure for moving in template fields
 */
struct __attribute__((__packed__)) ipfix_template_row {
	uint16_t id, length;
};

/**
 * \brief Create ipfix_message structure from data in memory
 *
 * \param[in] msg memory containing IPFIX message
 * \param[in] len length of the IPFIX message
 * \param[in] input_info information structure about input
 * \param[in] source_status Status of source (new, opened, closed)
 * \return ipfix_message structure on success, NULL otherwise
 */
API struct ipfix_message *message_create_from_mem(void *msg, int len, struct input_info* input_info, int source_state);

/**
 * \brief Copy IPFIX message
 *
 * \param[in] msg original IPFIX message
 * \return copy of original IPFIX message on success, NULL otherwise
 */
API struct ipfix_message *message_create_clone(struct ipfix_message *msg);


/**
 * \brief Create empty IPFIX message
 *
 * \return new instance of ipfix_message structure.
 */
API struct ipfix_message *message_create_empty();

/**
 * \brief Get data from IPFIX message.
 *
 * \param[out] dest where to copy data from message
 * \param[in] source from where to copy data from message
 * \param[in] len length of the data
 * \return 0 on success, negative value otherwise
 */
API int message_get_data(uint8_t **dest, uint8_t *source, int len);


/**
 * \brief Set data to IPFIX message.
 *
 * \param[out] dest where to write data
 * \param[in] source actual data
 * \param[in] len length of the data
 * \return 0 on success, negative value otherwise
 */
API int message_set_data(uint8_t *dest, uint8_t *source, int len);


/**
 * \brief Get pointers to start of the Data Records in specific Data Set
 *
 * \param[in] data_set data set to work with
 * \param[in] template template for data set
 * \return array of pointers to start of the Data Records in Data Set
 */
API uint8_t **get_data_records(struct ipfix_data_set *data_set, struct ipfix_template *tmplt);


/**
 * \brief Get offset where next data record starts
 *
 * \param[in] data_record data record
 * \param[in] template template for data record
 * \return offset of next data record in data set
 */
API uint16_t get_next_data_record_offset(uint8_t *data_record, struct ipfix_template *tmplt);


/**
 * \brief Dispose IPFIX message
 *
 * \param[in] msg IPFIX message to dispose
 * \return 0 on success, negative value otherwise
 */
API int message_free(struct ipfix_message *msg);

/**
 * \brief Get data from record
 *
 * \param[in] record Pointer to data record
 * \param[in] templ Data record's template
 * \param[in] enterprise Enterprise number
 * \param[in] id Field id
 * \param[out] data_length Length of returned data
 * \return Pointer to field
 */
API uint8_t *data_record_get_field(uint8_t *record, struct ipfix_template *templ, uint32_t enterprise, uint16_t id, int *data_length);

/**
 * \brief Set field value
 *
 * \param[in] record Pointer to data record
 * \param[in] templ Data record's template
 * \param[in] enterprise Enterprise number
 * \param[in] id Field id
 * \param[in] value Field value
 */
API void data_record_set_field(uint8_t *record, struct ipfix_template *templ, uint32_t enterprise, uint16_t id, uint8_t *value);

/**
 * \brief Set field value for each data record in set
 *
 * \param[in] set Data set
 * \param[in] templ Data set's template
 * \param[in] enterprise Enterprise number
 * \param[in] id Field ID
 * \param[in] value Field value
 */
API void data_set_set_field(struct ipfix_data_set *set, struct ipfix_template *templ, uint32_t enterprise, uint16_t id, uint8_t *value);

/**
 * \brief Get template record field
 *
 * \param[in] rec Template record
 * * \param[in] enterprise Enterprise number
 * \param[in] id  field id
 * \param[out] data_offset offset data record specified by this template record
 * \return pointer to inserted field
 */
API struct ipfix_template_row *template_record_get_field(struct ipfix_template_record *rec, uint32_t enterprise, uint16_t id, int *data_offset);

/**
 * \brief Get template record field
 *
 * \param[in] templ Template
 * \param[in] enterprise Enterprise number
 * \param[in] id  field id
 * \param[out] data_offset offset data record specified by this template record
 * \return pointer to inserted field
 */
API struct ipfix_template_row *template_get_field(struct ipfix_template *templ, uint32_t enterprise, uint16_t id, int *data_offset);

/**
 * \brief Compute data record's length
 *
 * \param[in] data_record Data record
 * \param[in] templ Data record's template
 * \return Length
 */
API uint16_t data_record_length(uint8_t *data_record, struct ipfix_template *templ);

/**
 * \brief Callback function for data records processing
 *
 * \param[in] rec Data record
 * \param[in] rec_len Data record's length
 * \param[in] templ Data record's template
 * \param[in] data Processing function data
 */
typedef  void (*dset_callback_f)(uint8_t *rec, int rec_len, struct ipfix_template *templ, void *data);

/**
 * \brief Callback function for (options) template records processing
 *
 * \param[in] rec (Options) template records
 * \param[in] rec_len Record's length
 * \param[in] data Processing function data
 */
typedef  void (*tset_callback_f)(uint8_t *rec, int rec_len, void *data);

/**
 * \brief Process all data records in set
 *
 * \param[in] data_set Data set
 * \param[in] templ Data set's template
 * \param[in] processor Function called for each data record
 * \param[in] proc_data Data given to function (besides data record, its's length and template)
 * \return Number of data records in set
 */
API int data_set_process_records(struct ipfix_data_set *data_set, struct ipfix_template *templ, dset_callback_f processor, void *proc_data);

/**
 * \brief Get number of records in data set
 *
 * \param[in] data_set Data set
 * \param[in] templ Data set's template
 * \return Number of data records in set
 */
API int data_set_records_count(struct ipfix_data_set *data_set, struct ipfix_template *templ);

/**
 * \brief Process all (options) template records in set
 *
 * \param[in] tset (Options) template set
 * \param[in] type Set type
 * \param[in] processor Function called for each record
 * \param[in] proc_data Data given to function
 * \return Number of template records in set
 */
API int template_set_process_records(struct ipfix_template_set *tset, int type, tset_callback_f processor, void *proc_data);

/**
 * \brief Free metadata in IPFIX message
 * 
 * \param[in] IPFIX message
 */
API void message_free_metadata(struct ipfix_message *msg);

/**
 * \brief Create copy of metadata structure
 *
 * \param[in] src Source IPFIX message
 * \return metadata
 */
API struct metadata *message_copy_metadata(struct ipfix_message *src);


#endif /* IPFIX_MESSAGE_H_ */

/**@}*/

