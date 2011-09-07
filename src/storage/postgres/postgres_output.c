/**
 * \file postgres_output.c
 * \author Michal Srb <michal.srb@cesnet.cz>
 * \brief Plugin for storing IPFIX data in PostgreSQL database.
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
 * \defgroup postgresOutput Plugin for storing IPFIX data in PostgreSQL database.
 * \ingroup storagePlugins
 *
 * \todo
 *
 * @{
 */

#include <stdio.h>
#include <libpq-fe.h>
#include <unistd.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <assert.h>
#include <arpa/inet.h>
#include <inttypes.h>
#include <endian.h>

#include "commlbr.h"
#include "ipfixcol.h"
#include "ipfix_entities.h"
#include "ipfix_postgres_types.h"


/* default database name, used if not specified otherwise */
#define DEFAULT_CONFIG_DBNAME "ipfix_data"
/* prefix for every table that will be created in database */
#define TABLE_NAME_PREFIX     "Template"
/* default length of the SQL commands */
#define SQL_COMMAND_LENGTH 1024;


/**
 * \struct postgres_config
 *
 * \brief PostgreSQL storage plugin specific "config" structure
 */
struct postgres_config {
	PGconn *conn;                   /** database connection */
	uint16_t *table_names;          /** list of tables in database, every table corresponds to a template */
	uint16_t table_counter;
	uint16_t table_size;
};


/**
 * \brief TODO
 *
 * \param[in] ipfix_type
 * \return
 */
static char *get_postgres_data_type(const char *ipfix_type)
{
	int i = 0;

	if (ipfix_type == NULL) {
		return NULL;
	}

	while (i < NUMBER_OF_TYPES) {
		if (!strcmp(types[i].ipfix_data_type, ipfix_type)) {
			return types[i].postgres_data_type;
		}

		i++;
	}

	/* no such type */
	return NULL;
}


/**
 * \brief TODO
 *
 * \param[in] ipfix_type
 * \return
 */
static int ipfix_type_to_internal(const char *ipfix_type)
{
	int i = 0;

	if (ipfix_type == NULL) {
		return -1;
	}

	while (i < NUMBER_OF_TYPES) {
		if (!strcmp(types[i].ipfix_data_type, ipfix_type)) {
			return types[i].internal_type;
		}

		i++;
	}

	/* no such type */
	return -1;
}


/**
 * \brief TODO
 *
 * \param[in] ie_id
 * \return
 */
static char *get_ie_type(uint16_t ie_id)
{
	if (ie_id > NUMBER_OF_IPFIX_ENTITIES) {
		/* this is invalid Information Element ID */
		return NULL;
	}

	assert((ie_id == ipfix_entities[ie_id].id) ? 1 : 0);

	return ipfix_entities[ie_id].type;
}


/**
 * \brief TODO
 *
 * \param[in] ie_id
 * \return
 */
static char *get_ie_name(uint16_t ie_id)
{
	if (ie_id > NUMBER_OF_IPFIX_ENTITIES) {
		/* this is invalid Information Element ID */
		return NULL;
	}

	assert((ie_id == ipfix_entities[ie_id].id) ? 1 : 0);

	return ipfix_entities[ie_id].name;
}


/**
 * \brief Create new table in DB
 *
 * \param[in] config TODO
 * \param[in] template
 * \return 0 on success
 */
static int create_table(struct postgres_config *config, struct ipfix_template *template)
{
	PGresult *res;
	char *sql_command;
	uint16_t sql_command_len = SQL_COMMAND_LENGTH;
	char *tmp_sql_command;
	uint16_t u;
	int index;
	uint16_t length;
	char *postgres_type;
	char *ipfix_type;
	char *column_name;
	uint8_t *fields;


	sql_command = (char *) malloc(sql_command_len);
	if (!sql_command) {
		VERBOSE(CL_VERBOSE_OFF, "Out of memory (%s:%d)", __FILE__, __LINE__);
		return -1;
	}
	memset(sql_command, 0, sql_command_len);

	tmp_sql_command = (char *) malloc(sql_command_len);
	if (!tmp_sql_command) {
		VERBOSE(CL_VERBOSE_OFF, "Out of memory (%s:%d)", __FILE__, __LINE__);
		goto err_sql_command;
	}
	memset(tmp_sql_command, 0, sql_command_len);

	/* create SQL command */
	snprintf(sql_command, sql_command_len, "%s%u%s", "CREATE TABLE \"Template", template->template_id, "\" (");

	/* get columns from template */
	index = 0;
	fields = (uint8_t *) template->fields;
	for (u = 0; u < template->field_count; u++) {

		length = *((uint16_t *) (fields+index+2)); // TODO - variable length
		column_name = get_ie_name(*((uint16_t *) (fields+index)));
		if (*((uint16_t *) template->fields+index) >> 15) {
			/* this is Enterprise ID, we will store such data in column
			 * of type "bytea" (array of bytes without specific meaning) */
			postgres_type = "bytea";
			index += 8; /* IE ID (2) + length (2) + Enterprise Number (4) */
		} else {
			/* regular element */
			ipfix_type = get_ie_type(*((uint16_t *) (fields+index)));
			index += 4;
			postgres_type = get_postgres_data_type(ipfix_type);
		}


		snprintf(tmp_sql_command, sql_command_len, "%s \"%s\" %s,", sql_command,
				                                 column_name, postgres_type);
		snprintf(sql_command, sql_command_len, "%s", tmp_sql_command);  /* TODO - memcpy? */
	}

	/* get rid of comma on the end of the command */
	sql_command[strlen(sql_command)-1] = '\0';

	snprintf(tmp_sql_command, sql_command_len, "%s)", sql_command);

	free(sql_command);
	sql_command = tmp_sql_command;

	// fprintf(stderr, "DEBUG: %s\n", sql_command);

	/* execute the command */
	res = PQexec(config->conn, sql_command);
	if (PQresultStatus(res) != PGRES_COMMAND_OK) {
		VERBOSE(CL_VERBOSE_OFF, "PostgreSQL: %s", PQerrorMessage(config->conn));
		PQclear(res);
		goto err_sql_command;
	}
	PQclear(res);

	return 0; /* table successfully created */


err_sql_command:
	free(sql_command);
	return -1;
}


/**
 * \brief TODO
 *
 * \param[in] config
 * \param[in] table_name
 * \param[in] couple
 * \return 0 on success
 */
static int insert_into(struct postgres_config *conf, const char *table_name, const struct data_template_couple *couple)
{
	PGresult *res;
	uint16_t data_index;
	uint16_t template_index;
	uint16_t u;
	uint16_t u2;
	uint16_t ie_id;
	uint16_t length;
	struct ipfix_data_set *data_set = couple->data_set;
	struct ipfix_template *template = couple->template;
	char *ipfix_type;
	char *sql_command;
	uint16_t sql_command_max_len = SQL_COMMAND_LENGTH;
	uint16_t sql_len;  /* current length of the SQL command */
	uint8_t *fields;
	uint8_t extra_bytes;


	char *bytea_str;
	uint8_t uint8;
	uint16_t uint16;
	uint32_t uint32;
	uint64_t uint64;
	int8_t int8;
	int16_t int16;
	int32_t int32;
	int64_t int64;
	float float32;
	double float64;
	char ip_addr[INET6_ADDRSTRLEN];
	uint8_t mac_addr[6];
	char *string;


	sql_command = (char *) malloc(sql_command_max_len);
	if (!sql_command) {
		VERBOSE(CL_VERBOSE_OFF, "Out of memory (%s:%d)", __FILE__, __LINE__);
		return -1;
	}
	memset(sql_command, 0, sql_command_max_len);


	fields = (uint8_t *) template->fields;



	data_index = 0;
	template_index = 0;

	while (data_index < (ntohs(data_set->header.length) - template->data_length-1)) {
		sql_len = 0;
		sql_len += snprintf(sql_command, sql_command_max_len,
		                 "INSERT INTO \"%s\" VALUES (", table_name);
	for (u = 0; u < template->field_count; u++) {

		ie_id = *((uint16_t *) (fields+template_index));
		length = *((uint16_t *) (fields+template_index+2));

		if (ie_id >> 15) {
			/* there is an Enterprise Number */
			bytea_str = (char *) malloc(length * 2);  /* will contain hexadecimal presentation of data */
			if (!bytea_str) {
				VERBOSE(CL_VERBOSE_OFF, "Out of memory (%s:%d)", __FILE__, __LINE__);
				goto err_sql_command;
			}
			memset(bytea_str, 0, length * 2);

			for (u2 = 0; u2 < length; u2++) {
				/* FIXME soooo sloooow */
				uint8 = *((uint8_t *) data_set->records+data_index);
				sql_len = snprintf(sql_command+sql_len,
				        sql_command_max_len-sql_len, "%x", uint8);
			}
			data_index += length;

		} else {
			/* no Enterprise number */

			ipfix_type = get_ie_type(ie_id);
			template_index += 4;

			/* next column value -> put comma in SQL command */
			if (u > 0) {
				sql_len += snprintf(sql_command+sql_len, sql_command_max_len-sql_len,
				                    "%c", ',');
			}

			switch(ipfix_type_to_internal(ipfix_type)) {
			case (UINT8):
				uint8 = *((uint8_t *) (data_set->records+data_index));
				data_index += 1;
				sql_len += snprintf(sql_command+sql_len, sql_command_max_len-sql_len,
				                    "%u", uint8);
				break;

			case (UINT16):
				uint16 = *((uint16_t *) (data_set->records+data_index));

				if ((extra_bytes = sizeof(uint16_t) - length) != 0) {
					/* exporter used fewer bytes to encode information element */
					uint16 = (uint16 << (extra_bytes * 8)) >> (extra_bytes * 8);

					/* now we need to convert given value to host byte order */
					switch (length) {
					case sizeof(uint8_t):
						/* no need for byte order conversion */
						break;
					}
				} else {
					/* length in template corresponds to length in RFC */
					uint16 = ntohs(uint16);
				}

				data_index += length;

				sql_len += snprintf(sql_command+sql_len,
				        sql_command_max_len-sql_len, "%u", uint16);
				break;

			case (UINT32):
				uint32 = *((uint32_t *) (data_set->records+data_index));

				if ((extra_bytes = sizeof(uint32_t) - length) != 0) {
					/* exporter used fewer bytes to encode information element */
					uint32 = (uint32 << (extra_bytes * 8)) >> (extra_bytes * 8);

					/* now we need to convert given value to host byte order */
					switch (length) {
					case sizeof(uint8_t):
						/* no need for byte order conversion */
						break;
					case sizeof(uint16_t):
							uint32 = (uint32_t) ntohs((uint16_t) uint32);
						break;
					}
				} else {
					/* length in template corresponds to length in RFC */
					uint32 = ntohl(uint32);
				}

				data_index += length;
				sql_len += snprintf(sql_command+sql_len, sql_command_max_len-sql_len,
				                    "%u", uint32);
				break;

			case (UINT64):
				uint64 = *((uint64_t *) (data_set->records+data_index));

				if ((extra_bytes = sizeof(uint64_t) - length) != 0) {
					/* exporter used fewer bytes to encode information element */
					uint64 = (uint64 << (extra_bytes * 8)) >> (extra_bytes * 8);

					/* now we need to convert given value to host byte order */
					switch (length) {
					case sizeof(uint8_t):
						/* no need for byte order conversion */
						break;
					case sizeof(uint16_t):
						uint64 = (uint64_t) ntohs((uint16_t) uint64);
						break;
					case sizeof(uint32_t):
						uint64 = (uint64_t) ntohl((uint32_t) uint64);
						break;
					}
				} else {
					/* length in template corresponds to length in RFC */
					uint64 = be64toh(uint64);
				}

				data_index += length;

				sql_len += snprintf(sql_command+sql_len, sql_command_max_len-sql_len,
				                    "%"PRIu64, uint64);
				break;

			case (INT8):
				int8 = (int8_t) *((uint8_t *) (data_set->records+data_index));
				data_index += length;
				sql_len += snprintf(sql_command+sql_len, sql_command_max_len-sql_len,
				                    "%d", int8);
				break;

			case (INT16):
				int16 = (int16_t) *((uint16_t *) (data_set->records+data_index));

				if ((extra_bytes = sizeof(int16_t) - length) != 0) {
					/* exporter used fewer bytes to encode information element */
					int16 = (int16 << (extra_bytes * 8)) >> (extra_bytes * 8);

					/* now we need to convert given value to host byte order */
					switch (length) {
					case sizeof(int8_t):
						/* no need for byte order conversion */
						break;
					}
				} else {
					/* length in template corresponds to length in RFC */
					int16 = ntohs((uint16_t) int16);
				}

				data_index += length;
				sql_len += snprintf(sql_command+sql_len, sql_command_max_len-sql_len,
				                    "%d", int16);
				break;

			case (INT32):
				int32 = (int32_t) *((uint32_t *) (data_set->records+data_index));

				if ((extra_bytes = sizeof(int32_t) - length) != 0) {
					/* exporter used fewer bytes to encode information element */
					uint32 = (uint32 << (extra_bytes * 8)) >> (extra_bytes * 8);

					/* now we need to convert given value to host byte order */
					switch (length) {
					case sizeof(int8_t):
						/* no need for byte order conversion */
						break;
					case sizeof(int16_t):
						int32 = (int32_t) ntohs((uint16_t) int32);
						break;
					}
				} else {
					/* length in template corresponds to length in RFC */
					int32 = ntohl((uint32_t) int32);
				}

				data_index += length;
				sql_len += snprintf(sql_command+sql_len, sql_command_max_len-sql_len,
				                    "%d", int32);
				break;

			case (INT64):
				int64 = (int64_t) *((uint64_t *) (data_set->records+data_index));

				if ((extra_bytes = sizeof(int64_t) - length) != 0) {
					/* exporter used fewer bytes to encode information element */
					int64 = (int64 << (extra_bytes * 8)) >> (extra_bytes * 8);

					/* now we need to convert given value to host byte order */
					switch (length) {
					case sizeof(int8_t):
						/* no need for byte order conversion */
						break;
					case sizeof(int16_t):
						int64 = (int64_t) ntohs((uint16_t) int64);
						break;
					case sizeof(int32_t):
						int64 = (int64_t) ntohl((uint32_t) int64);
						break;
					}
				} else {
					/* length in template corresponds to length in RFC */
					int64 = be64toh((uint64_t) int64);
				}

				data_index += length;
				sql_len += snprintf(sql_command+sql_len, sql_command_max_len-sql_len,
				                    "%"PRId64, int64);
				break;

			case (STRING):
				string = (char *) malloc(length + 1);
				if (!string) {
					VERBOSE(CL_VERBOSE_OFF, "Out of memory (%s:%d)", __FILE__, __LINE__);
					goto err_sql_command;
				}
				memset(string, 0, length + 1);

				data_index += length;

				memcpy(string, data_set->records+data_index, length);

				sql_len += snprintf(sql_command+sql_len, sql_command_max_len-sql_len,
				                    "%s", string);
				free(string);
				string = NULL;
				break;

			case (BOOLEAN):
				uint8 = *((uint8_t *) data_set->records+data_index);

				/* in IPFIX, boolean is encoded in single octet
				 * 1 means TRUE, 2 means FALSE */
				switch (uint8) {
				case (1):
					string = "true";
					break;
				case (2):
					string = "false";
					break;
				}

				data_index += length;

				sql_len += snprintf(sql_command+sql_len, sql_command_max_len-sql_len,
				                    "%s", string);
				break;

			case (IPV4ADDR):
				uint32 = *((uint32_t *) (data_set->records+data_index));

				inet_ntop(AF_INET, data_set->records+data_index, ip_addr, INET6_ADDRSTRLEN);

				data_index += length;
				sql_len += snprintf(sql_command+sql_len, sql_command_max_len-sql_len,
				                    "'%s'", ip_addr);
				break;

			case (IPV6ADDR):
				inet_ntop(AF_INET6, data_set->records+data_index, ip_addr, INET6_ADDRSTRLEN);
				data_index += length;
				sql_len += snprintf(sql_command+sql_len, sql_command_max_len-sql_len,
				                    "'%s'", ip_addr);
				break;

			case (MACADDR):
				mac_addr[5] = *((uint8_t *) (data_set->records+data_index));
				mac_addr[4] = *((uint8_t *) (data_set->records+data_index+1));
				mac_addr[3] = *((uint8_t *) (data_set->records+data_index+2));
				mac_addr[2] = *((uint8_t *) (data_set->records+data_index+3));
				mac_addr[1] = *((uint8_t *) (data_set->records+data_index+4));
				mac_addr[0] = *((uint8_t *) (data_set->records+data_index+5));

				data_index += 6;
				sql_len += snprintf(sql_command+sql_len, sql_command_max_len-sql_len,
				              "'%x:%x:%x:%x:%x:%x'", mac_addr[0], mac_addr[1],
				              mac_addr[2], mac_addr[3], mac_addr[4],
				              mac_addr[5]);

				break;

			case (OCTETARRAY):
				bytea_str = (char *) malloc(length * 2);  /* will contain hexadecimal presentation of data */
				if (!bytea_str) {
					VERBOSE(CL_VERBOSE_OFF, "Out of memory (%s:%d)", __FILE__, __LINE__);
					goto err_sql_command;
				}
				memset(bytea_str, 0, length * 2);

				for (u2 = 0; u2 < length; u2++) {
					/* FIXME soooo sloooow */
					uint8 = *((uint8_t *) data_set->records+data_index);
					sql_len = snprintf(sql_command+sql_len,
					        sql_command_max_len-sql_len, "%x", uint8);
				}
				data_index += length;

				break;

			case (DATETIMESECONDS):
				uint32 = ntohl(*((uint32_t *) data_set->records+data_index));

				data_index += length;
				sql_len += snprintf(sql_command+sql_len, sql_command_max_len-sql_len,
						"to_timestamp(%u)", uint32);
				break;

			case (DATETIMEMILLISECONDS):
			case (DATETIMEMICROSECONDS):
			case (DATETIMENANOSECONDS):
				/* according to RFC 5101, it is encoded as 64-bit integer */
				uint64 = be64toh(*((uint64_t *) (data_set->records+data_index)));
				data_index += length;

				/* PostgreSQL expects microseconds */
				switch (ipfix_type_to_internal(ipfix_type)) {
				case (DATETIMEMILLISECONDS):
					float64 = uint64 / 1000;
					break;
				case (DATETIMEMICROSECONDS):
					float64 = uint64 / 1000000;
					break;
				case (DATETIMENANOSECONDS):
					float64 = uint64 / 1000000000;
					break;
				}

				sql_len += snprintf(sql_command+sql_len, sql_command_max_len-sql_len,
						"to_timestamp(%f)", float64);
				break;

			case (FLOAT32):
				float32 = *((float *) (data_set->records+data_index));
				data_index += length;

				sql_len += snprintf(sql_command+sql_len, sql_command_max_len-sql_len,
				                    "%f", float32);

				break;

			case (FLOAT64):
				float64 = *((double *) (data_set->records+data_index));
				data_index += length;

				sql_len += snprintf(sql_command+sql_len, sql_command_max_len-sql_len,
						"%f", float64);

				break;

			default:
				fprintf(stderr, "PostgreSQL storage plugin: unknown data type\n");
				break;
			}
		}
	}



	sql_len += snprintf(sql_command+sql_len, sql_command_max_len-sql_len,
	                    "%s", ")");
	// fprintf(stderr, "\nDEBUG: SQL command to execute:\n %s\n", sql_command);

	res = PQexec(conf->conn, sql_command);
	if (PQresultStatus(res) != PGRES_COMMAND_OK) {
		VERBOSE(CL_VERBOSE_OFF, "PostgreSQL: %s", PQerrorMessage(conf->conn));
		PQclear(res);
		goto err_sql_command;
	}
	PQclear(res);

	template_index = 0;
	sql_len = 0;
	}

	free(sql_command);

	return 0;


err_sql_command:
	free(sql_command);

	return -1;
}


/**
 * \brief Process new templates
 *
 * New template means new table in DB.
 *
 * \param[in] conf TODO
 * \param[in] ipfix_msg
 * \return 0 on success
 */
static int process_new_templates(struct postgres_config *conf, const struct ipfix_message *ipfix_msg)
{
	uint16_t template_index = 0;
	struct ipfix_template *template;
	uint16_t u;
	int ret;
	uint8_t flag = 0;

	if (conf == NULL || ipfix_msg == NULL) {
		return -1;
	}

	while ((template = ipfix_msg->data_set[template_index].template) != NULL) {

		flag = 1;     /* create table if list is empty */
		/* check whether table for the template exists */
		for (u = 0; u < conf->table_counter; u++) {
			if (conf->table_names[u] == template->template_id) {
				/* table for this template already exists */
				flag = 0;
				break;
			}
		}

		if (flag) {
			/* create new table */
			ret = create_table(conf, template);
			if (ret < 0) {
				VERBOSE(CL_VERBOSE_OFF, "Table wasn't created");
			}

			conf->table_names[conf->table_counter] = template->template_id;
			conf->table_counter += 1;

			flag = 0;
		}

		template_index++;
	}

	return 0;
}


/**
 * \brief Process Data Sets
 *
 * \param[in] conf
 * \param[in] ipfix_msg
 * \return 0 on success
 */
static int process_data_records(struct postgres_config *conf, const struct ipfix_message *ipfix_msg)
{
	uint16_t set_index = 0;
	//struct ipfix_template *template;
	struct ipfix_data_set *data_set;
	//uint16_t u;
	//int ret;
	//uint8_t hit = 0;
	char table_name[64];

	if (conf == NULL || ipfix_msg == NULL) {
		return -1;
	}

	while ((data_set = ipfix_msg->data_set[set_index].data_set) != NULL) {
		snprintf(table_name, 64, TABLE_NAME_PREFIX "%u", ipfix_msg->data_set[set_index].template->template_id);
		insert_into(conf, table_name, &(ipfix_msg->data_set[set_index]));

		set_index++;
	}

	return 0;
}


/**
 * \brief Storage plugin initialization.
 *
 * Initialize IPFIX storage plugin. This function allocates, fills and
 * returns config structure.
 *
 * \param[in] params parameters for this storage plugin
 * \param[out] config the plugin specific configuration structure
 * \return 0 on success, negative value otherwise
 */
int storage_init(char *params, void **config)
{
	struct postgres_config *conf;
	PGconn *conn;
	char *connection_string = NULL;   /* PostgreSQL connection string */
	xmlDocPtr doc;
	xmlNodePtr cur;
	char *host = NULL;
	char *hostaddr = NULL;
	char *port = NULL;
	char *dbname = NULL;
	uint8_t dbname_allocated = 0; /* indicates whether dbname was allocated
	                               * via malloc() */
	char *user = NULL;
	char *pass = NULL;
	size_t connection_string_len;
	char *tmp_connection_string;

	conf = (struct postgres_config *) malloc(sizeof(*conf));
	if (!conf) {
		VERBOSE(CL_VERBOSE_OFF, "Out of memory (%s:%d)", __FILE__, __LINE__);
		return -1;
	}
	memset(conf, 0, sizeof(*conf));


	doc = xmlReadMemory(params, strlen(params), "nobase.xml", NULL, 0);
	if (doc == NULL) {
		VERBOSE(CL_VERBOSE_OFF, "Plugin configuration not parsed successfully");
		goto err_init;
	}

	cur = xmlDocGetRootElement(doc);
	if (cur == NULL) {
		VERBOSE(CL_VERBOSE_OFF, "Empty configuration");
		goto err_xml;
	}

	if (xmlStrcmp(cur->name, (const xmlChar *) "fileWriter")) {
		VERBOSE(CL_VERBOSE_OFF, "root node != fileWriter");
		goto err_xml;
	}

	cur = cur->xmlChildrenNode;
	while (cur != NULL) {
		if ((!xmlStrcmp(cur->name, (const xmlChar *) "host"))) {
			if (host != NULL) {
				xmlFree(host);
				host = NULL;
			}
			host = (char *) xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
		}

		if ((!xmlStrcmp(cur->name, (const xmlChar *) "hostaddr"))) {
			if (hostaddr != NULL) {
				xmlFree(hostaddr);
				hostaddr = NULL;
			}
			hostaddr = (char *) xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
		}

		if ((!xmlStrcmp(cur->name, (const xmlChar *) "port"))) {
			if (port != NULL) {
				xmlFree(port);
				port = NULL;
			}
			port = (char *) xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
		}

		if ((!xmlStrcmp(cur->name, (const xmlChar *) "dbname"))) {
			if (dbname != NULL) {
				xmlFree(dbname);
				dbname = NULL;
			}
			dbname = (char *) xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			dbname_allocated = 1;
		}

		if ((!xmlStrcmp(cur->name, (const xmlChar *) "user"))) {
			if (user != NULL) {
				xmlFree(user);
				user = NULL;
			}
			user = (char *) xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
		}

		if ((!xmlStrcmp(cur->name, (const xmlChar *) "pass"))) {
			if (pass != NULL) {
				xmlFree(pass);
				pass = NULL;
			}
			pass = (char *) xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
		}

		cur = cur->next;
	}

	/* use default values if not specified in configuration */
	if (!dbname) {
		dbname = DEFAULT_CONFIG_DBNAME;
	}

	/* create connection string */
	/* get maximal length of the connection string */
	connection_string_len = 0;

	if (host) {
		/* +1 at the end is for comma (options separator) */
		connection_string_len += strlen("host=") + strlen(host) + 1;
	}

	if (hostaddr) {
		connection_string_len += strlen("hostaddr=") + strlen(hostaddr) + 1;
	}

	if (port) {
		connection_string_len += strlen("port=") + strlen(port) + 1;
	}

	if (dbname) {
		connection_string_len += strlen("dbname=") + strlen(dbname) + 1;
	}

	if (user) {
		connection_string_len += strlen("user=") + strlen(user) + 1;
	}

	if (pass) {
		connection_string_len += strlen("pass=") + strlen(pass) + 1;
	}

	connection_string_len += 1;   /* terminating NULL */

	connection_string = (char *) malloc(connection_string_len);
	if (connection_string == NULL) {
		VERBOSE(CL_VERBOSE_OFF, "Out of memory (%s:%d)", __FILE__, __LINE__);
		goto err_xml;
	}
	memset(connection_string, 0, connection_string_len);

	/* because strings in sprintf() can't overlap, we need temporary buffer (FIXME - we don't need temp buffer) */
	tmp_connection_string = (char *) malloc(connection_string_len);
	if (tmp_connection_string == NULL) {
		VERBOSE(CL_VERBOSE_OFF, "Out of memory (%s:%d)", __FILE__, __LINE__);
		goto err_connection_string;
	}
	memset(tmp_connection_string, 0, connection_string_len);

	/* host specified */
	if (host) {
		sprintf(tmp_connection_string, "%s %s=%s", connection_string,
				                                   "host", host);
		sprintf(connection_string, "%s", tmp_connection_string);
		free(host);
	}

	/* hostaddr specified */
	if (hostaddr) {
		sprintf(tmp_connection_string, "%s %s=%s", connection_string,
				                                   "hostaddr", hostaddr);
		sprintf(connection_string, "%s", tmp_connection_string);
		free(hostaddr);
	}

	/* port specified */
	if (port) {
		sprintf(tmp_connection_string, "%s %s=%s", connection_string,
				                                   "port", port);
		sprintf(connection_string, "%s", tmp_connection_string);
		free(port);
	}

	/* dbname specified */
	if (dbname) {
		sprintf(tmp_connection_string, "%s %s=%s", connection_string,
				                                   "dbname", dbname);
		sprintf(connection_string, "%s", tmp_connection_string);

		/* do not try to free statically allocated memory */
		if (dbname_allocated) {
			/* this was specified via configuration file, free it */
			free(dbname);
		}
	}

	/* user specified */
	if (user) {
		sprintf(tmp_connection_string, "%s %s=%s", connection_string,
				                                   "user", user);
		sprintf(connection_string, "%s", tmp_connection_string);
		free(user);
	}

	/* pass specified */
	if (pass) {
		sprintf(tmp_connection_string, "%s %s=%s", connection_string,
				                                   "pass", pass);
		sprintf(connection_string, "%s", tmp_connection_string);
		free(pass);
	}

	free(tmp_connection_string);

	// fprintf(stderr, "DEBUG: connection string: \"%s\"\n", connection_string);

	/* try to connect to the database */
	conn = PQconnectdb(connection_string);
	if (PQstatus(conn) != CONNECTION_OK) {
		VERBOSE(CL_VERBOSE_OFF, "Unable to create connection to the database: %s", PQerrorMessage(conn));
		goto err_connection_string;
	}

	/* we don't need this XML configuration anymore */
	xmlFreeDoc(doc);

	conf->table_size = 128; /* TODO - macro */
	conf->table_names = (uint16_t *) malloc(conf->table_size * sizeof(uint16_t));

	conf->conn = conn;

	*config = conf;

	return 0;


err_connection_string:
	free(connection_string);

err_xml:
	xmlFreeDoc(doc);

err_init:
	free(conf);

	return -1;
}


/**
 * \brief Store received IPFIX message into a file.
 *
 * Store one IPFIX message into a output file.
 *
 * \param[in] config the plugin specific configuration structure
 * \param[in] ipfix_msg IPFIX message
 * \param[in] templates All currently known templates, not just templates
 * in the message
 * \return 0 on success, negative value otherwise
 */
int store_packet(void *config, const struct ipfix_message *ipfix_msg,
                           const struct ipfix_template_mgr *template_mgr)
{
	struct postgres_config *conf;

	if (config == NULL || ipfix_msg == NULL) {
		return -1;
	}

	conf = (struct postgres_config *) config;

	process_new_templates(conf, ipfix_msg);
	process_data_records(conf, ipfix_msg);

	return 0;
}


/**
 * \brief Store everything we have immediately and close output file.
 *
 * Just flush all buffers.
 *
 * \param[in] config the plugin specific configuration structure
 * \return 0 on success, negative value otherwise
 */
int store_now(const void *config)
{
	/* nothing to do */
	return 0;
}


/**
 * \brief Remove storage plugin.
 *
 * This function is called when we don't want to use this storage plugin
 * anymore. All it does is that it cleans up after the storage plugin.
 *
 * \param[in] config the plugin specific configuration structure
 * \return 0 on success, negative value otherwise
 */
int storage_close(void **config)
{
	struct postgres_config *conf;

	conf = (struct postgres_config *) *config;

	PQfinish(conf->conn);
	VERBOSE(CL_VERBOSE_OFF, "Connection to the database has been closed.");

	free(conf->table_names);
	free(conf);

	return 0;
}


/**@}*/

/* Debug section. This can be safely ignored or deleted */
#ifdef POSTGRES_PLUGIN_DEBUG

char *xml_configuration =
		"<postgres>"
		"<user>m4jkl</user>"
		"<dbname>test</dbname>"
		"</postgres>";


int main(int argc, char **argv)
{
	struct postgres_config *config;
	int ret;
	struct ipfix_message message;
	struct ipfix_template template;

	ret = storage_init(xml_configuration, (void **) &config);
	if (ret != 0) {
		fprintf(stderr, "DEBUG: storage_init() failed with return code %d.\n", ret);
	}

	message.data_set[0].template = &template;
	template.template_id = 333;
	template.field_count = 1;

	process_new_templates(config, &message);
	process_data_records(config, &message);


	ret = storage_close((void **) &config);
	if (ret != 0) {
		fprintf(stderr, "DEBUG: storage_close() failed with return code %d.\n", ret);
	}

	return 0;
}

#endif
