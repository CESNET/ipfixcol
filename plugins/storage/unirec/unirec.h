/**
 * \file unirec.h
 * \author Petr Velan <petr.velan@cesnet.cz>
 * \brief Header file of plugin for converting IPFIX data to UniRec format
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

#ifndef UNIREC_H_
#define UNIREC_H_

#define DEFAULT_TIMEOUT 60
#define UNIREC_ELEMENTS_FILE "/usr/share/ipfixcol/unirec-elements.txt"

typedef struct ipfixElement {
	uint16_t id;
	uint32_t en;
} ipfixElement;

typedef struct unirecField {
	char *name;
	int8_t size;
	int8_t required;
	struct unirecField *next;
	char *value;				/**< Value of the field */
	uint16_t valueSize;			/**< Size of the value */
	uint8_t valueFilled;		/**< Is the value filled? */

	/**< Number of ipfix elements */
	int ipfixCount;
	/**< https://tools.ietf.org/html/rfc5101#section-3.2 with masked size and EN for IANA elements */
	ipfixElement ipfix[1];
} unirecField;


/**
 * \struct config
 *
 * \brief UniRec storage plugin specific "config" structure
 */
typedef struct unirec_config {
	char				*format;
	char 				*host;
	char				*port;
	uint16_t			reconnectTimeout;
	time_t				lastReconnect;	/**< Timestamp of last reconnection attempt */


	unirecField			*fields;
	char 				*buffer;	/**< UniRec ouput buffer */
	int					bufferSize; /**< UniRec ouput buffer size */
	int					bufferOffset;	/**< Offset of current buffer position (data length) */

	int					socket;		/**< Connection socket */
	int 				ip;			/**< IP protocol (AF_INET, ...) */
	int 				flags;		/**< addrinfo flags */
} unirec_config;

#endif /* UNIREC_H_ */
