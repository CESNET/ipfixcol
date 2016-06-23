/**
 * \file unirec.h
 * \author Petr Velan <petr.velan@cesnet.cz>
 * \author Erik Sabik <xsabik02@stud.fit.vutbr.cz>
 * \brief Header file of plugin for converting IPFIX data to UniRec format
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

#ifndef IPFIX2UNIREC_H_
#define IPFIX2UNIREC_H_

#include "fast_hash_table.h"



#define INIT_DYNAMIC_ARR_SIZE 8
#define INIT_STATIC_AND_DYNAMIC_ARR_SIZE 64
#define INIT_OUTPUT_BUFFER_SIZE 1024
#define MAX_DYNAMIC_FIELD_SIZE 512
#define FIELDS_HT_ROW_FIELDSCOUNT_MULTIPLAYER 8
#define FIELDS_HT_KEYSIZE 8 // ipfix id + en + padding in bytes (2 + 4 + 2)
#define FIELDS_HT_STASH_SIZE 4

#define UNIREC_DATA_TYPES_COUNT 15 ///< Count of UniRec data types
#define UNIREC_DEFAULT_LENGTH_OF_DATA_FORMAT 1024 /// Length of string of a template

#define DEFAULT_TIMEOUT 0 /**< No waiting */

// Path to unirec elements config file
const char *UNIREC_ELEMENTS_FILE = DATAROOTDIR "/ipfixcol/unirec-elements.txt";

enum unirecFieldEnum {
   UNIREC_FIELD_OTHER,
   UNIREC_FIELD_IP,
   UNIREC_FIELD_PACKET,
   UNIREC_FIELD_TS,
   UNIREC_FIELD_DBF,
   UNIREC_FIELD_LBF
};


enum ODID_get_methods {
   ODID_JOINFLOWS_METHOD,
   ODID_MANAGER_METHOD
};

typedef struct ipfixElement {
   uint16_t id;
   uint32_t en;
} ipfixElement;

typedef struct unirecField {
   char *name;
   int ur_id;
   int8_t type;			/**< Used for faster processing, possible value are in `unirefFieldEnum` */
   int8_t unirec_type;		/**< Used for generating data format string */
   int8_t size;
   int8_t required;
   int8_t *required_ar;		/**< Array of interfaces numbers where this element is required */
   int8_t *included_ar;		/**< Array of interfaces numbers where this element is included */
   uint16_t *offset_ar;
   struct unirecField *next;
   struct unirecField *nextIfc;
   void *value;				/**< Pointer to value of the field */
   uint16_t valueSize;			/**< Size of the value */
   uint8_t valueFilled;		/**< Is the value filled? */




   /**< Number of ipfix elements */
   int ipfixCount;
   /**< https://tools.ietf.org/html/rfc5101#section-3.2 with masked size and EN for IANA elements */
   ipfixElement ipfix[1];
} unirecField;


/**
 * \struct interface
 *
 * \brief UniRec storage plugin specific "config" structure
 */
typedef struct ifc_config {
   int				number;	/**< Number of output interface */
   char				*format;
   char				*unirec_data_format;
   int				timeout; /**< Timeout of write operation on TRAP interface.
                                Time in microseconds or 0 for no waiting or
                                -1 for unlimited waiting.*/

   unirecField			*fields;
   char 				*buffer;	/**< UniRec ouput buffer */
   int				bufferSize;		/**< UniRec ouput buffer size */
   int 				bufferDynSize;
   int				bufferAllocSize;
   int				dynamicPartOffset;	/**< Offset of current position in dynamic part of record (sum of dynamic field sizes) */
   int				bufferOffset;
   uint8_t				requiredCount;	/**< Count of all required Unirec fields */
   uint8_t				requiredFilled;	/**< Count of filled required Unirec fields */
   uint16_t 			bufferStaticSize;
   uint8_t 			dynamic;
   uint16_t 			dynCount;
   uint16_t 			dynArAlloc;
   unirecField 			**dynAr;

   unirecField			*special_field_odid; /**< Pointer to special field ODID */
   unirecField			*special_field_link_bit_field; /**< Pointer to special field LINK_BIT_FIELD */
} ifc_config;

/**
 * \struct config
 *
 * \brief Main UniRec storage plugin config structure
 */
typedef struct unirec_config {
   int ifc_count;		/**< Interface count */
   unirecField *fields;	/**< Array of Unirec fields from every interfaces */
   ifc_config *ifc;	/**< Array of interface config structures */
   trap_ctx_t *trap_ctx_ptr;
        uint8_t trap_init;
   trap_ifc_spec_t ifc_spec;
        char *ifc_buff_switch;
   uint64_t *ifc_buff_timeout;
   uint16_t odid;
    unirecField *LBF_field;
    uint8_t ODID_get_method;
   uint8_t SF_DATA;
   fht_table_t *ht_fields;
} unirec_config;



#endif /* IPFIX2UNIREC_H_ */
