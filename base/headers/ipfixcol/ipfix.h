/**
 * \file ipfix.h
 * \author Radek Krejci <rkrejci@cesnet.cz>
 * \brief Structures and macros definition for IPFIX processing
 *
 * Copyright (C) 2009-2011 CESNET, z.s.p.o.
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

#ifndef IPFIX_H_
#define IPFIX_H_

/**
 * \defgroup ipfix IPFIX Structures
 * \ingroup publicAPIs
 *
 * IPFIX structures allowing access to the data
 * @{
 */

#include <stdint.h>
#include "api.h"

/** IPFIX identification (NetFlow version 10) */
#define IPFIX_VERSION 0x000a

/** Length value signaling variable-length IE */
#define VAR_IE_LENGTH 65535

/* Path to ipfix-elements.xml file */
API extern const char *ipfix_elements;

/* Terminating flag */
API extern volatile int terminating;

/**
 * \struct ipfix_header
 * \brief IPFIX header structure
 */
struct ipfix_header {
	/**
	 * Version of Flow Record format exported in this message. The value of this
	 * field is 0x000a for the current version, incrementing by one the version
	 * used in the NetFlow services export version 9.
	 */
	uint16_t version;

	/**
	 * Total length of the IPFIX Message, measured in octets, including Message
	 * Header and Set(s).
	 */
	uint16_t length;

	/**
	 * Time, in seconds, since 0000 UTC Jan 1, 1970, at which the IPFIX Message
	 * Header leaves the Exporter.
	 */
	uint32_t export_time;

	/**
	 * Incremental sequence counter modulo 2^32 of all IPFIX Data Records sent
	 * on this PR-SCTP stream from the current Observation Domain by the
	 * Exporting Process. Check the specific meaning of this field in the
	 * subsections of Section 10 when UDP or TCP is selected as the transport
	 * protocol. This value SHOULD be used by the Collecting Process to
	 * identify whether any IPFIX Data Records have been missed. Template and
	 * Options Template Records do not increase the Sequence Number.
	 */
	uint32_t sequence_number;

	/**
	 * A 32-bit identifier of the Observation Domain that is locally unique to
	 * the Exporting Process. The Exporting Process uses the Observation Domain
	 * ID to uniquely identify to the Collecting Process the Observation Domain
	 * that metered the Flows. It is RECOMMENDED that this identifier also be
	 * unique per IPFIX Device. Collecting Processes SHOULD use the Transport
	 * Session and the Observation Domain ID field to separate different export
	 * streams originating from the same Exporting Process. The Observation
	 * Domain ID SHOULD be 0 when no specific Observation Domain ID is relevant
	 * for the entire IPFIX Message, for example, when exporting the Exporting
	 * Process Statistics, or in case of a hierarchy of Collectors when
	 * aggregated Data Records are exported.
	 */
	uint32_t observation_domain_id;
};

/**
 * Length of the IPFIX header (in bytes).
 */
#define IPFIX_HEADER_LENGTH 		16

/* Flowset type identifiers */
/** Template Set ID */
#define IPFIX_TEMPLATE_FLOWSET_ID 	2
/** Options Template Set ID */
#define IPFIX_OPTION_FLOWSET_ID 	3
/** Minimal Template ID, i.e., minimal Record Set ID */
#define IPFIX_MIN_RECORD_FLOWSET_ID 256

/**
 * \struct ipfix_set_header
 * \brief Common IPFIX Set (header) structure
 */
struct __attribute__((__packed__)) ipfix_set_header {
	/**
	 * Set ID value identifies the Set.  A value of 2 is reserved for the
	 * Template Set. A value of 3 is reserved for the Option Template Set. All
	 * other values from 4 to 255 are reserved for future use. Values above 255
	 * are used for Data Sets. The Set ID values of 0 and 1 are not used for
	 * historical reasons [<a href="http://tools.ietf.org/html/rfc3954">RFC3954</a>].
	 */
	uint16_t flowset_id;

	/**
	 * Total length of the Set, in octets, including the Set Header, all
	 * records, and the optional padding.  Because an individual Set MAY contain
	 * multiple records, the Length value MUST be used to determine the position
	 * of the next Set.
	 */
	uint16_t length;

};

/**
 * \typedef template_ie
 * \brief Template's definition of an IPFIX information element.
 *
 * The type is defined as 32b value containing one of (union) Enterprise Number
 * or standard element definition containing IE ID and its length. There are
 * two #template_ie in the following scheme.
 * <pre>
 *   0                   1                   2                   3
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |E|  Information Element ident. |        Field Length           |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |                      Enterprise Number                        |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * </pre>
 */
/**
 * \union template_ie_u
 * \brief Structure for template's definition of an IPFIX information element
 */
typedef union template_ie_u {
	struct {
		/**
		 * A numeric value that represents the type of Information Element.
		 * Refer to [<a href="http://tools.ietf.org/html/rfc5102">RFC5102</a>].
		 *
		 * The first (highest) bit is Enterprise bit.  This is the first bit of
		 * the Field Specifier.  If
		 * this bit is zero, the Information Element Identifier identifies an
		 * IETF-specified Information Element, and the four-octet Enterprise
		 * Number field MUST NOT be present.  If this bit is one, the
		 * Information Element identifier identifies an enterprise-specific
		 * Information Element, and the Enterprise Number filed MUST be
		 * present.
		 */
		uint16_t id;

		/**
		 * The length of the corresponding encoded Information Element, in
		 * octets. The value 65535 is reserved for variable-length Information
		 * Elements.
		 */
		uint16_t length;
	} ie;
	/**
	 * IANA enterprise number [<a href="http://www.iana.org/assignments/enterprise-numbers">
	 * IANA Private Enterprise Numbers registry</a>] of the authority defining
	 * the Information Element identifier in this Template Record.
	 */
	uint32_t enterprise_number;
} template_ie;


/**
 * \struct ipfix_template_record
 * \brief Structure of the IPFIX Template record
 */
struct __attribute__((__packed__)) ipfix_template_record {
	/**
	 * Each of the newly generated Template Records is given a unique Template
	 * ID. This uniqueness is local to the Transport Session and Observation
	 * Domain that generated the Template ID.  Template IDs 0-255 are reserved
	 * for Template Sets, Options Template Sets, and other reserved Sets yet to
	 * be created. Template IDs of Data Sets are numbered from 256 to 65535.
	 * There are no constraints regarding the order of the Template ID
	 * allocation.
	 */
	uint16_t template_id;

	/**
	 * Number of fields in this Template Record.
	 */
	uint16_t count;

	/**
	 * Field Specifier(s)
	 */
	template_ie fields[1];
};

/**
 * \struct ipfix_template_set
 * \brief IPFIX Template Set structure
 */
struct __attribute__((__packed__)) ipfix_template_set {
	struct ipfix_set_header header; /**< Common IPFIX Set header */

	/**
	 * The first of templates records in this Template Set. Real size of the
	 * record is unknown here due to a variable count of fields in each record.
	 */
	struct ipfix_template_record first_record;
};

/**
 * \struct ipfix_options_template_record
 * \brief Structure of the IPFIX Options Template record
 */
struct __attribute__((__packed__)) ipfix_options_template_record {
	/**
	 * Each of the newly generated Template Records is given a unique Template
	 * ID. This uniqueness is local to the Transport Session and Observation
	 * Domain that generated the Template ID. Template IDs 0-255 are reserved
	 * for Template Sets, Options Template Sets, and other reserved Sets yet to
	 * be created. Template IDs of Data Sets are numbered from 256 to 65535.
	 * There are no constraints regarding the order of the Template ID
	 * allocation.
	 */
	uint16_t template_id;

	/**
	 * Number of all fields in this Options Template Record, including the
	 * Scope Fields.
	 */
	uint16_t count;

	/**
	 * Number of scope fields in this Options Template Record. The Scope Fields
	 * are normal Fields except that they are interpreted as scope at the
	 * Collector. The Scope Field Count MUST NOT be zero.
	 */
	uint16_t scope_field_count;

	/**
	 * Field Specifier(s)
	 */
	template_ie fields[1];
};

/**
 * \struct ipfix_options_template_set
 * \brief IPFIX Options Template Set structure
 */
struct ipfix_options_template_set {
	struct ipfix_set_header header; /**< Common IPFIX Set header */

	/**
	 * The first of templatee records in this Options Template Set. Real size of
	 * the record is unknown here due to a variable count of fields in each
	 * record.
	 */
	struct ipfix_options_template_record first_record;
};

/**
 * \struct ipfix_data_set
 * \brief IPFIX Data records Set structure
 *
 * The Data Records are sent in Data Sets. It consists only of one or more Field
 * Values. The Template ID to which the Field Values belong is encoded in the
 * Set Header field "Set ID", i.e., "Set ID" = "Template ID".
 */
struct ipfix_data_set {
	struct ipfix_set_header header; /**< Common IPFIX Set header */
	uint8_t records[1]; /**< Start of Data records */
};


/**@}*/

#endif /* IPFIX_H_ */
