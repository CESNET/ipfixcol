/**
 * \file storage.h
 * \author Radek Krejci <rkrejci@cesnet.cz>
 * \brief IPFIX Collector Storage API.
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

#ifndef IPFIXCOL_STORAGE_H_
#define IPFIXCOL_STORAGE_H_

#include "ipfix.h"
#include "input.h"
#include "templates.h"
#include "api.h"

#define MSG_MAX_LENGTH          65535
#define MSG_MAX_OTEMPL_SETS     1024
#define MSG_MAX_TEMPL_SETS      1024
#define MSG_MAX_DATA_COUPLES    1023

/**
 * \defgroup storageAPI Storage Plugins API
 * \ingroup publicAPIs
 *
 * These functions specifies a communication interface between ipficol core,
 * namely a data manager handling specific Observation Domain ID, and storage
 * plugins. More precisely, each storage plugin communicates with a separated
 * thread of the data manager.
 *
 * \image html arch_scheme_core_comm.png "ipfixcol internal communication"
 * \image latex arch_scheme_core_comm.png "ipfixcol internal communication" width=10cm
 *
 * @{
 */

/**
 * \struct data_template_couple
 * \brief This structure connects Data set from the IPFIX packet with the
 * template structure describing the Data set
 *
 */
struct data_template_couple{
	/**< Address of the Data Set in the packet */
	struct ipfix_data_set *data_set;
	/**< Template structure corresponding to this Data set */
	struct ipfix_template *data_template;
};

enum PLUGIN_STATUS {
	PLUGIN_DATA,    /**< Don't react on this message */
	PLUGIN_START,   /**< Start reading */
	PLUGIN_STOP     /**< Stop reading */
};

/**
 * \struct ipfix_record
 * \brief Structure for one data record with it's length and template
 */
struct ipfix_record {
	void *record;       /**< Data record */
	uint16_t length;    /**< Record's length */
	struct ipfix_template *templ;   /**< Record's template */
};

struct __attribute__((packed)) metadata {
	struct ipfix_record record;     /**< IPFIX data record */
	uint16_t srcCountry;			/**< Source country code */
	uint16_t dstCountry;			/**< Destination country code */
	uint32_t srcAS;                 /**< Source AS */
	uint32_t dstAS;                 /**< Destination AS */
	void **channels;				/**< Array of channels assigned to this record */
	char srcName[32];
	char dstName[32];
};

/**
 * \struct ipfix_message
 * \brief Structure covering main parts of the IPFIX packet by pointers into it.
 */
struct __attribute__((__packed__)) ipfix_message {
	/** IPFIX header*/
	struct ipfix_header               *pkt_header;
	/** Input source information */
	struct input_info                 *input_info;
	/** Source status (new, opened, closed) */
	enum SOURCE_STATUS                source_status;
	enum PLUGIN_STATUS                plugin_status;
	int plugin_id;
	/** Number of data records in message */
	uint16_t                          data_records_count;
	/** Number of template records in message */
	uint16_t                          templ_records_count;
	/** Number of options template records in message */
	uint16_t                          opt_templ_records_count;
	/** List of Template Sets in the packet */
	struct ipfix_template_set         *templ_set[MSG_MAX_TEMPL_SETS];
	/** List of Options Template Sets in the packet */
	struct ipfix_options_template_set *opt_templ_set[MSG_MAX_OTEMPL_SETS];
	/** List of Data Sets (with a link to corresponding template) in the packet */
	struct data_template_couple       data_couple[MSG_MAX_DATA_COUPLES];
	/** Pointer to the live profile */
	void *live_profile;
	/** List of metadata structures */
	struct metadata *metadata;
};

/**
 * \brief Storage plugin initialization function.
 *
 * The function is called just once before any other storage API's function.
 *
 * \param[in]  params  String with specific parameters for the storage plugin.
 * \param[out] config  Plugin-specific configuration structure. ipfixcol is not
 * involved in the config's structure, it is just passed to the following calls
 * of storage API's functions.
 * \return 0 on success, nonzero else.
 */
API int storage_init (char *params, void **config);

/**
 * \brief Pass IPFIX data with supplemental structures from ipfixcol core into
 * the storage plugin.
 *
 * The way of data processing is completely up to the specific storage plugin.
 * The basic usage is to store all data in a specific format, but also various
 * processing (statistics, etc.) can be done by storage plugin. In general any
 * processing with IPFIX data can be done by the storage plugin.
 *
 * \param[in] config     Plugin-specific configuration data prepared by init
 * function.
 * \param[in] ipfix_msg  Covering structure including IPFIX data as well as
 * supplementary structures for better/faster parsing of IPFIX data.
 * \param[in] templates  The list of preprocessed templates for possible
 * better/faster data processing.
 * \return 0 on success, nonzero else.
 */
API int store_packet (void *config, const struct ipfix_message *ipfix_msg,
		const struct ipfix_template_mgr *template_mgr);

/**
 * \brief Announce willing to store currently processing data.
 *
 * This way ipfixcol announces willing to store immediately as much data as
 * possible. The impulse to this action is taken from the user and broadcasted
 * by ipfixcol to all storage plugins. The real response of the storage plugin
 * is completely up to the specific plugin.
 *
 * \param[in] config  Plugin-specific configuration data prepared by init
 * function.
 * \return 0 on success, nonzero else.
 */
API int store_now (const void *config);

/**
 * \brief Storage plugin "destructor".
 *
 * Clean up all used plugin-specific structures and memory allocations. This
 * function is used only once as a last function call of the specific storage
 * plugin.
 *
 * \param[in,out] config  Plugin-specific configuration data prepared by init
 * \return 0 on success and config is changed to NULL, nonzero else.
 */
API int storage_close (void **config);

/**@}*/

#endif /* IPFIXCOL_STORAGE_H_ */
