/**
 * \file input.h
 * \author Radek Krejci <rkrejci@cesnet.cz>
 * \brief IPFIX Collector Input plugin API.
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


#ifndef IPFIXCOL_INPUT_H_
#define IPFIXCOL_INPUT_H_

/**
 * \enum SOURCE_TYPE
 * \brief Type of the source of the input data.
 *
 * The type distinguish several general type of \link #input_info input
 * information structure\endlink (like \link #input_info_network network\endlink
 * or \link #input_info_file file\endlink).
 */
typedef enum SOURCE_TYPE {
	SOURCE_TYPE_UDP,          /**< IPFIX over UDP */
	SOURCE_TYPE_TCP,          /**< IPFIX over TCP */
	SOURCE_TYPE_TCPTLS,       /**< IPFIX over TCP secured with TLS */
	SOURCE_TYPE_SCTP,         /**< IPFIX over SCTP */
 	SOURCE_TYPE_NF5,          /**< NetFlow v5 */
	SOURCE_TYPE_NF9,          /**< NetFlow v9 */
	SOURCE_TYPE_IPFIX_FILE,   /**< IPFIX File Format */
	SOURCE_TYPE_COUNT         /**< number of defined SOURCE_TYPEs */
} SOURCE_TYPE;

/**
 * \struct input_info
 * \brief  General input information structure used to distinguish the real
 * input information type.
 */
struct input_info {
	SOURCE_TYPE type;         /**< type of source defined by enum #SOURCE_TYPE */
};

/**
 * \struct input_info_network
 * \brief Input information structure specific for network based data sources.
 */
struct input_info_network {
	SOURCE_TYPE type;         /**< type of source - #SOURCE_TYPE_UDP,
	                           * #SOURCE_TYPE_TCP, #SOURCE_TYPE_TCPTLS,
	                           * #SOURCE_TYPE_SCTP, #SOURCE_TYPE_NF5,
	                           * #SOURCE_TYPE_NF9 */
	uint8_t l3_proto;         /**< IP protocol byte */
	union {
		struct in6_addr ipv6;
		struct in_addr ipv4;
	} src_addr;               /**< source IP address */
	union {
		struct in6_addr ipv6;
		struct in_addr ipv4;
	} dst_addr;               /**< destination IP address*/
	uint16_t src_port;        /**< source transport port */
	uint16_t dst_port;        /**< destination transport port */
	void *exporter_cert;      /**< X.509 certificate used by exporter when
	                           * using TLS/DTLS */
	void *collector_cert;     /**< X.509 certificate used by collector when
	                           * using TLS/DTLS */
};

/**
 * \struct input_info_file
 * \brief Input information structure specific for file based data sources.
 */
struct input_info_file {
	SOURCE_TYPE type;         /**< type of source - #SOURCE_TYPE_IPFIX_FILE */
	char *name;               /**< name of the input file */
};

/**
 * \brief Input plugin initialization function.
 *
 * The function is called just once before any other input API's function.
 *
 * @param[in]  params  String with specific parameters for the input plugin.
 * @param[out] config  Plugin-specific configuration structure. ipfixcol is not
 * involved in the config's structure, it is just passed to the following calls
 * of input API's functions.
 * @return 0 on success, nonzero else.
 */
int input_init(char *params, void **config);

/**
 * \brief Pass input data from the input plugin into the ipfixcol core.
 *
 * Each input plugin HAS TO pass data to the ipfixcol in the form of memory
 * block containing IPFIX packet. It means that if input plugin reads data in
 * different format than IPFIX (e.g., NetFlow), the data MUST be transformed
 * into the IPFIX packet format. Memory allocated by the input plugin for the
 * data are freed by ipfixcol core.
 *
 * Together with the data, input plugin further
 * passes information structure of the data source. Input plugin can count on
 * the fact that this information data will be only read and not changed by
 * ipfixcol core.
 *
 * @param[in] config  Plugin-specific configuration data prepared by init
 * function.
 * @param[out] info   Information structure describing the source of the data.
 * @param[out] packet Flow information data in the form of IPFIX packet.
 * @return 0 on success, nonzero else.
 */
int get_packet(void *config, struct input_info** info, char **packet);

/**
 * \brief Input plugin "destructor".
 *
 * Clean up all used plugin-specific structures and memory allocations. This
 * function is used only once as a last function call of the specific input
 * plugin.
 *
 * @param[in,out] config  Plugin-specific configuration data prepared by init
 * @return 0 on success and config is changed to NULL, nonzero else.
 */
int input_close(void **config);

#endif /* IPFIXCOL_INPUT_H_ */
