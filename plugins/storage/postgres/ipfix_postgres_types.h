/**
 * \file ipfix_postgres_types.h
 * \author Michal Srb <michal.srb@cesnet.cz>
 * \brief Auxiliary structure for translating IPFIX data types to PostgreSQL data types
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


#ifndef IPFIX_POSTGRES_TYPES
#define IPFIX_POSTGRES_TYPES


/* for usage in switch statement */
enum ipfix_types {
	UINT8,
	UINT16,
	UINT32,
	UINT64,
	INT8,
	INT16,
	INT32,
	INT64,
	STRING,
	IPV4ADDR,
	IPV6ADDR,
	MACADDR,
	OCTETARRAY,
	DATETIMESECONDS,
	DATETIMEMILLISECONDS,
	DATETIMEMICROSECONDS,
	DATETIMENANOSECONDS,
	BOOLEAN,
	FLOAT32,
	FLOAT64
};


struct ipfix_postgres_types {
	char *ipfix_data_type;       /* IPFIX data type */
	char *postgres_data_type;    /* corresponding PostgreSQL data type */
	enum ipfix_types internal_type;
};



#define NUMBER_OF_TYPES 20

const struct ipfix_postgres_types types[] = {
	/*IPFIX type/PostgreSQL type/auxiliary member - for internal usage in switch statement */
	{ "unsigned8", "smallint", UINT8 },
	{ "unsigned16", "integer", UINT16 },
	{ "unsigned32", "bigint", UINT32 },
	{ "unsigned64", "decimal", UINT64 },
	{ "signed8", "smallint", INT8 },
	{ "signed16", "smallint", INT16 },
	{ "signed32", "integer", INT32 },
	{ "signed64", "bigint", INT64 },
	{ "string", "decimal", STRING },
	{ "boolean", "decimal", BOOLEAN },
	{ "ipv4Address", "inet", IPV4ADDR },
	{ "ipv6Address", "inet", IPV6ADDR },
	{ "macAddress", "macaddr", MACADDR },
	{ "octetArray", "bytea", OCTETARRAY },
	{ "dateTimeSeconds", "timestamp", DATETIMESECONDS },
	{ "dateTimeMilliseconds", "timestamp", DATETIMEMILLISECONDS },
	{ "dateTimeMicroseconds", "timestamp", DATETIMEMICROSECONDS },
	{ "dateTimeNanoseconds", "timestamp", DATETIMENANOSECONDS },
	{ "flaot32", "float", FLOAT32 },
	{ "float64", "float", FLOAT64 },
};

#endif
