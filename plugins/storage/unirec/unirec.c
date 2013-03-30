/**
 * \file unirec.c
 * \author Petr Velan <petr.velan@cesnet.cz>
 * \brief Plugin for converting IPFIX data to UniRec format
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



#include <ipfixcol.h>
#include <stdio.h>
#include <string.h>
#include <endian.h>
#include <time.h>
#include <errno.h>
#include <libxml/parser.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

#include "unirec.h"

/* some auxiliary functions for extracting data of exact length */
#define read8(ptr) (*((uint8_t *) (ptr)))
#define read16(ptr) (*((uint16_t *) (ptr)))
#define read32(ptr) (*((uint32_t *) (ptr)))
#define read64(ptr) (*((uint64_t *) (ptr)))


/** Identifier to MSG_* macros */
static char *msg_module = "unirec";

/**
 * \brief Works as memcpy but converts commond data sizes to to host byteorder
 *
 * @param dst Destination buffer
 * @param src Source buffer
 * @param length Data length
 */
static void data_copy(char *dst, char *src, uint16_t length)
{
	switch (length) {
			case (1):
				*dst = read8(src);
				break;
			case (2):
				*(uint16_t *) dst = ntohs(read16(src));
				break;
			case (4):
				*(uint32_t *) dst = ntohl(read32(src));
				break;
			case (8):
				*(uint64_t *) dst = be64toh(read64(src));
				break;
			case (16):
				*(uint64_t *) dst = be64toh(read64(src+8));
				*(uint64_t *) (dst+8) = be64toh(read64(src));
			break;
			default:
				memcpy(dst, src, length);
				break;
			}
}

/**
 * \brief Create connection to collector
 *
 * The created socket is stored in conf->socket
 * Socket is disconnected on error
 *
 * @param conf Plugin configuration
 * @return 0 on success, 1 on socket error or 2 when target is not listening
 */
static int tcp_connect(unirec_config *conf)
{
	struct addrinfo hints, *addrinfo, *tmp;
	int ret = 0;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = conf->ip;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_ADDRCONFIG | conf->flags;

	/* Get server address */
	ret = getaddrinfo(conf->host, conf->port, &hints, &addrinfo);
	if (ret != 0) {
		MSG_ERROR(msg_module, "Cannot get server info: %s", gai_strerror(ret));
		return 1;
	}

	/* Try addrinfo strucutres one by one */
	for (tmp = addrinfo; tmp != NULL; tmp = tmp->ai_next) {

		if (tmp->ai_family != AF_INET && tmp->ai_family != AF_INET6) {
			continue;
		}

		/* Print information about target address */
		char buff[INET6_ADDRSTRLEN];
		inet_ntop(tmp->ai_family,
				(tmp->ai_family == AF_INET) ?
						(void *) &((struct sockaddr_in *) tmp->ai_addr)->sin_addr :
						(void *) &((struct sockaddr_in6 *) tmp->ai_addr)->sin6_addr,
				(char *) &buff, sizeof(buff));

		MSG_DEBUG(msg_module, "Connecting to IP (port): %s (%s)", buff, conf->port);
		MSG_DEBUG(msg_module, "Socket configuration: AI Family: %i, AI Socktype: %i, AI Protocol: %i",
			tmp->ai_family, tmp->ai_socktype, tmp->ai_protocol);

		/* create socket */
		conf->socket = socket(addrinfo->ai_family,
				addrinfo->ai_socktype, addrinfo->ai_protocol);
		if (conf->socket == -1) {
			MSG_ERROR(msg_module, "Cannot create new socket: %s", strerror(errno));
			continue;
		}

		/* Connect to server*/
		if (connect(conf->socket, addrinfo->ai_addr, addrinfo->ai_addrlen) == -1) {
			MSG_ERROR(msg_module, "Cannot connect to collector: %s", strerror(errno));
			close(conf->socket);
			continue;
		}

		/* Connected*/
		MSG_NOTICE(msg_module, "Successfully connected to collector");
		break;
	}

	/* Free allocated resources */
	freeaddrinfo(addrinfo);

	/* Return error when all addrinfo structures were tried*/
	if (tmp == NULL) {
		return 1;
	}

	return 0;
}

/**
 * \brief Checks that connection is OK or tries to reconnect
 *
 * @param conf Plugin configuration
 * @return 0 when connection is OK or reestablished, 1 when not
 */
static int reconnect(unirec_config *conf)
{
	/* Check for broken connection */
	if (conf->lastReconnect != 0) {
		/* Check whether we need to attempt reconnection */
		if (conf->lastReconnect + conf->reconnectTimeout <= time(NULL)) {
			/* Try to reconnect */
			if (tcp_connect(conf) == 0) {
				conf->lastReconnect = 0;
			} else {
				/* Set new reconnect time and drop packet */
				conf->lastReconnect = time(NULL);
				return 1;
			}
		} else {
			/* Timeout not reached, drop packet */
			return 1;
		}
	}

	return 0;
}

/**
 * \brief Sends packet using UDP or TCP as defined in plugin configuration
 *
 * When the collector disconnects, tries to reconnect and resend the data
 *
 * \param data Data to send
 * \param length Length of the data to send
 * \param conf Plugin configuration
 * \return 0 on success, -1 on socket error, 1 when data needs to be resent (after reconnect)
 */
static int send_packet(char *data, uint16_t length, unirec_config *conf)
{
	int ret; /* Return value of sendto */
	int sent = 0; /* Sent data size */

	/* Check that connection is OK or drop packet */
	if (reconnect(conf)) {
		return -1;
	}

	/* sendto() does not guarantee that everything will be send in one piece */
	while (sent < length) {
		/* Send conf->buffer to collector (TCP and SCTP ignores last two arguments) */
		ret = send(conf->socket, (void *) data + sent, length - sent, MSG_NOSIGNAL | MSG_DONTWAIT);

		/* Check that the data were sent correctly */
		if (ret == -1) {
			switch (errno) {
			case 0: break; /* OK */
			case EWOULDBLOCK:
				/* When the sending of the message would block and nothing was sent yet, discard it*/
				if (sent == 0) {
					MSG_WARNING(msg_module, "send function would block, skipping packet");
					return -1;
				}
				break;
			case ECONNRESET:
			case EINTR:
			case ENOTCONN:
			case ENOTSOCK:
			case EPIPE:
			case EHOSTUNREACH:
			case ENETDOWN:
			case ENETUNREACH:
			case ENOBUFS:
			case ENOMEM:

				/* The connection is broken */
				MSG_WARNING(msg_module, "Destination closed connection");

				/* free resources */
				close(conf->socket);

				/* Set last connection try time so that we would reconnect immediatelly */
				conf->lastReconnect = 1;

				/* Say that we should try to connect and send data again */
				return 1;
			default:
				/* Unknown error */
				MSG_ERROR(msg_module, "Cannot send data to destination: %s", strerror(errno));
				return -1;
			}
		}

		/* No error from sendto(), add sent data count to total */
		sent += ret;
	}

	MSG_COMMON(5, "Packet sent to %s on port %s.", conf->host, conf->port);

	return 0;
}

/**
 * \brief Return an UniRec field that matches given IPFIX element
 *
 * @param element Element to match
 * @param fields List of UniRec fields to search
 * @return return matching UniRec filed or NULL
 */
static unirecField *match_field(template_ie *element, unirecField *fields)
{
	unirecField *tmp = NULL;
	uint16_t id;
	uint32_t en;
	int i;

	id = element->ie.id;

	/* Go over all records to fill */
	for (tmp = fields; tmp != NULL ; tmp = tmp->next) {

		/* Handle enterprise element */
		if (id >> 15) { /* EN is set */
			/* Enterprise number is in the next element */
			en = (element+1)->enterprise_number;

			for (i = 0; i < tmp->ipfixCount; i++) {
				/* We need to mask first bit of id before the comparison */
				if ((id & 0x7F) == tmp->ipfix[i].id && en == tmp->ipfix[i].en) {
					return tmp;
				}
			}
		} else { /* EN == 0 */
			for (i = 0; i < tmp->ipfixCount; i++) {
				if (id == tmp->ipfix[i].id) {
					return tmp;
				}
			}
		}
	}

	return NULL;
}

/**
 * \brief Get data from data record
 *
 * Uses conf->unirecFields to store values from this record
 *
 * \param[in] data_record IPFIX data record
 * \param[in] template corresponding template
 * \param[out] data statistics data to get
 * \return length of the data record
 */
static uint16_t process_record(char *data_record, struct ipfix_template *template, unirec_config *conf)
{
	if (!template) {
		/* We don't have template for this data set */
		MSG_WARNING(msg_module, "No template for the data set\n");
		return 0;
	}

	if (!conf) {
		/* Configuration not given */
		MSG_WARNING(msg_module, "No configuration given to process_record\n");
		return 0;
	}

	uint16_t offset = 0;
	uint16_t index, count;
	uint16_t length, size_length;
	unirecField *matchField;

	/* Go over all fields */
	for (count = index = 0; count < template->field_count; count++, index++) {

		matchField = match_field(&template->fields[index], conf->fields);

		length = template->fields[index].ie.length;
		size_length = 0;

		/* Handle variable length */
		if (length == 65535) {
			/* Variable length */
			length = read8(data_record+offset);
			size_length = 1;

			if (length == 255) {
				length = ntohs(read16(data_record+offset+size_length));
				size_length = 3;
			}
		}

		/* Copy the element value on match */
		if (matchField) {
			MSG_COMMON(6, "We found a match! [%s]", matchField->name);

			/* Check the allocated size */
			if (!matchField->value || matchField->valueSize < length) {
				matchField->value = realloc(matchField->value, length);
				matchField->valueSize = length;
			}

			/* Copy the value, change byteorder  */
			data_copy(matchField->value, data_record + offset + size_length, length);
			matchField->valueFilled = 1;
		}

		/* Skip enterprise element number if necessary*/
		if (template->fields[index].ie.id >> 15) {
			index++;
		}

		/* Skip the length of the value */
		offset += length + size_length;
	}

	return offset;
}

/**
 * \brief Fill value of static element to given offset of the UniRec buffer
 *
 * For dynamic elements it writes only value size
 *
 * @param offset Offset of the value
 * @param field UniRec field with value to fill
 * @param conf plugin configuration
 * @return Returns size of the written data or zero on failure
 */
static uint16_t unirec_fill_static(uint16_t offset, unirecField *field, unirec_config *conf)
{
	uint16_t size;

	/* Ensure that the buffer is big enough (+2 is for dynamic element size) */
	if (conf->bufferSize < offset + field->size + 2) {
		conf->buffer = realloc(conf->buffer, offset + field->size + 2);
		conf->bufferSize = offset + field->size + 2;
	}

	/* Required value not present */
	if (field->required && !field->valueFilled) {
		return 0;
	}

	/* Fixed size elements */
	if (field->size != -1) {
		if (field->valueFilled) {
			/* Find maximum size and set rest to zero if necessary */
			if (field->size > field->valueSize) {
				size = field->valueSize;
				memset(conf->buffer + offset, 0, field->size);
			} else {
				size = field->size;
			}

			/* Copy the actual value size and maximum allowed size */
			memcpy(conf->buffer + offset, field->value, size);
		} else {
			/* Set value to zero */
			memset(conf->buffer + offset, 0, field->size);
		}

		return field->size;
	}

	/* Dynamic size elements - fill only size */
	if (field->valueFilled) {
		*(uint16_t *) (conf->buffer + offset) = field->valueSize;
	} else {
		*(uint16_t *) (conf->buffer + offset) = 0;
	}

	return 2; /* size is 16bit value */
}

/**
 * \brief Fill value of dynamic element to given offset of the UniRec buffer
 *
 * @param offset Offset of the value
 * @param field UniRec field with value to fill
 * @param conf plugin configuration
 * @return Returns size of the written data
 */
static uint16_t unirec_fill_dynamic(uint16_t offset, unirecField *field, unirec_config *conf)
{

	/* Only dynamic size with filled value */
	if (field->size == -1 && field->valueFilled) {
		/* Check buffer size */
		if (conf->bufferSize < offset + field->valueSize) {
			conf->buffer = realloc(conf->buffer, offset + field->valueSize);
			conf->bufferSize = offset + field->size;
		}

		/* Copy the value */
		memcpy(conf->buffer + offset, field->value, field->valueSize);

		/* Return real size */
		return field->valueSize;
	}

	/* Unset the filled property for future use */
	field->valueFilled = 0;

	return 0;
}

/**
 * \brief Creates and sends UniRec records
 *
 * Takes values for current record that are filled in conf->fields
 *
 * @param conf
 */
static void process_unirec_fields(unirec_config *conf)
{
	unirecField *tmp;
	uint16_t offset = conf->bufferOffset, tmpOffset;

	/* Create UniRec record */

	/* Fill static size values and dynamic lengths */
	for (tmp = conf->fields; tmp != NULL; tmp = tmp->next) {
		tmpOffset = unirec_fill_static(offset, tmp, conf);
		/* Record does not contain required value */
		if (!tmpOffset) {
			MSG_COMMON(6, "Record does not contain required value(s).");
			return;
		}

		offset += tmpOffset;
	}

	/* Fill dynamic values */
	for (tmp = conf->fields; tmp != NULL; tmp = tmp->next) {
		offset += unirec_fill_dynamic(offset, tmp, conf);
	}

	/* Set the offset + record length to total UniRec buffer length */
	conf->bufferOffset = offset;
}

/**
 * \brief Process all data sets in IPFIX message
 *
 * \param[in] ipfix_msg IPFIX message
 * \param[out] data statistics data to get
 * \return 0 on success, -1 otherwise
 */
static int process_data_sets(const struct ipfix_message *ipfix_msg, unirec_config *conf)
{
	uint16_t data_index = 0;
	struct ipfix_data_set *data_set;
	char *data_record;
	struct ipfix_template *template;
	uint32_t offset;
	uint16_t min_record_length, ret = 0;


	data_set = ipfix_msg->data_couple[data_index].data_set;

	while(data_set) {
		template = ipfix_msg->data_couple[data_index].data_template;

		/* Skip datasets with missing templates */
		if (template == NULL) {
			/* Process next set */
			data_set = ipfix_msg->data_couple[++data_index].data_set;

			continue;
		}

		min_record_length = template->data_length;
		offset = 4;  /* Size of the header */

		if (min_record_length & 0x8000) {
			/* Record contains fields with variable length */
			min_record_length = min_record_length & 0x7fff; /* size of the fields, variable fields excluded  */
		}

		while ((int) ntohs(data_set->header.length) - (int) offset - (int) min_record_length >= 0) {
			data_record = (((char *) data_set) + offset);
			ret = process_record(data_record, template, conf);
			/* Check that the record was processes successfuly */
			if (ret == 0) {
				return -1;
			}

			offset += ret;

			/* Create UniRec record and send it */
			process_unirec_fields(conf);
		}

		/* Process next set */
		data_set = ipfix_msg->data_couple[++data_index].data_set;
	}

	return 0;
}

/**
 * \brief Destroys and frees an unirecField structure
 *
 * @param field Field to destroy
 */
static void destroy_field(unirecField *field)
{
	/* Free field name */
	free(field->name);

	/* Free field value */
	free(field->value);

	/* Free field */
	free(field);
}

/**
 * \brief Destroy UniRec fields list
 *
 * @param fields Field list to destroy
 */
static void destroy_fields(unirecField *fields)
{
	unirecField *tmp;

	while (fields != NULL) {
		/* Save last position */
		tmp = fields;
		/* Move pointer */
		fields = fields->next;

		/* Free field */
		destroy_field(tmp);
	}
}

/**
 * \brief Creates IPFIX id from string
 *
 * @param ipfixToken String in eXXidYY format, where XX is enterprise number and YY is element ID
 * @return Returns id that is used to compare field against IPFIX template
 */
static ipfixElement ipfix_from_string(char *ipfixToken)
{
	ipfixElement element;
	char *endptr;

	element.en = strtol(ipfixToken + 1, &endptr, 10);
	element.id = strtol(endptr + 2, (char **) NULL, 10);

	return element;
}

/**
 * \brief Loads all available elements from configuration file
 * @return List of UniRec elements on success, NULL otherwise
 */
static unirecField *load_elements()
{
	FILE *uef = NULL;
	char *line;
	size_t lineSize = 100;
	ssize_t res;
	char *token, *state; /* Variables for strtok  */
	unirecField *fields = NULL, *currentField = NULL;

	/* Open the file */
	uef = fopen(UNIREC_ELEMENTS_FILE, "r");
	if (uef == NULL) {
		MSG_ERROR(msg_module, "Cannot load UniRec configuration file (\"%s\")", UNIREC_ELEMENTS_FILE);
		return NULL;
	}

	/* Init buffer */
	line = malloc(lineSize);

	/* Process all lines */
	while (1) {
		/* Read one line */
		res = getline(&line, &lineSize, uef);
		if (res <= 0) {
			break;
		}

		/* Exclude comments */
		if (line[0] == '#') continue;

		/* Create new element structure, make space for ipfixElCount ipfix elements and NULL */
		currentField = malloc(sizeof(unirecField));
		currentField->name = NULL;
		currentField->value = NULL;

		/* Read individual tokens */
		int position = 0;
		for (token = strtok_r(line, " \t", &state); token != NULL; token = strtok_r(NULL, " \t", &state), position++) {
			switch (position) {
			case 0:
				currentField->name = strdup(token);
				break;
			case 1:
				currentField->size = atoi(token);
				break;
			case 2: {
				/* Split the string */
				char *ipfixToken, *ipfixState; /* Variables for strtok  */
				int ipfixPosition = 0;
				currentField->ipfixCount = 0;
				for (ipfixToken = strtok_r(token, ",", &ipfixState);
						ipfixToken != NULL; ipfixToken = strtok_r(NULL, ",", &ipfixState), ipfixPosition++) {
					/* Enlarge the element if necessary */
					if (ipfixPosition > 0) {
						currentField = realloc(currentField, sizeof(unirecField) + (ipfixPosition) * sizeof(uint64_t));
					}
					/* Store the ipfix element id */
					currentField->ipfix[ipfixPosition] = ipfix_from_string(ipfixToken);
					currentField->ipfixCount++;
				}
				break;
			}
			}
		}

		/* Check that all necessary data was provided */
		if (position < 2) {
			if (currentField->name) {
				free(currentField->name);
			}
			free(currentField);
			continue;
		}

		currentField->next = fields;
		fields = currentField;
	}

	fclose(uef);
	free(line);

	return fields;
}


/**
 * \brief Update field's length and ipfix elements from configuration fields list
 *
 * @param currentField Field to update
 * @param confFields Field list loaded from configuration
 * @return Returns 0 on success
 */
static int update_field(unirecField **currentField, unirecField *confFields)
{
	unirecField *tmp;
	int updated = 0;

	/* Look for specified element */
	for (tmp = confFields; tmp != NULL; tmp = tmp->next) {
		if (strcmp((*currentField)->name, tmp->name) == 0) {
			(*currentField)->size = tmp->size;
			(*currentField)->ipfixCount = 0;

			/* Copy IPFIX element identifiers */
			int i;
			for (i = 0; i < tmp->ipfixCount; i++) {
				/* Realloc when there is more than one element */
				if (i > 0) {
					(*currentField) = realloc((*currentField), sizeof(unirecField) + i * sizeof(uint64_t));
				}

				(*currentField)->ipfix[i] = tmp->ipfix[i];
				(*currentField)->ipfixCount++;
			}

			/* Set updated flag */
			updated = 1;
		}
	}

	/* Return 0 on success */
	return 1 - updated;
}


/**
 * \brief Parse UniRec format from XML
 *
 * @param conf Plugin internal configuration
 * @return 0 on success
 *
 *
 * https://homeproj.cesnet.cz/projects/traffic-analysis/wiki/UniRec
 * https://homeproj.cesnet.cz/projects/traffic-analysis/wiki/Seznam_polo%C5%BEek_UniRec
 *
 *
 */
static int parse_format(unirec_config *conf)
{
	char *token, *state; /* Variables for strtok  */
	unirecField *lastField = NULL, *currentField = NULL, *confFields;
	int ret;

	if (conf->format == NULL) {
		return -1;
	}

	/* Load elements from configuration file */
	confFields = load_elements();

	/* Split string to tokens */
	for (token = strtok_r(conf->format, ";", &state); token != NULL; token = strtok_r(NULL, ";", &state)) {

		/* Create new field element */
		currentField = malloc(sizeof(unirecField));
		/* Init values used for iteration */
		currentField->next = NULL;
		currentField->value = NULL;
		currentField->valueSize = 0;
		currentField->valueFilled = 0;
		currentField->ipfixCount = 0;

		/* Fill name and required values */
		if (token[0] == '?') {
			currentField->required = 0;
			currentField->name = strdup(token + 1);
		} else {
			currentField->required = 1;
			currentField->name = strdup(token);
		}

		/* Fill values from configuration file */
		ret = update_field(&currentField, confFields);
		if (ret) {
			MSG_ERROR(msg_module, "Field \"%s\" is not present in UniRec configuration file", currentField->name);
			destroy_field(currentField);
			destroy_fields(confFields);
			return 1;
		}

		/* Add it at the end of the list */
		if (lastField != NULL) {
			lastField->next = currentField;
		} else {
			/* At first run assign the first field to configuration structure */
			conf->fields = currentField;
		}
		/* Move the pointer to the end */
		lastField = currentField;
	}

	/* Elements from UniRec configuration file are not needed anymore */
	destroy_fields(confFields);

	return 0;
}

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
int storage_init (char *params, void **config)
{
	unirec_config *conf;
	xmlDocPtr doc;
	xmlNodePtr cur;

	conf = (unirec_config *) malloc(sizeof(*conf));
	if (!conf) {
		MSG_ERROR(msg_module, "Out of memory (%s:%d)", __FILE__, __LINE__);
		return -1;
	}
	memset(conf, 0, sizeof(*conf));

	doc = xmlReadMemory(params, strlen(params), "nobase.xml", NULL, 0);
	if (doc == NULL) {
		MSG_ERROR(msg_module, "Cannot parse plugin configuration");
		goto err_init;
	}

	cur = xmlDocGetRootElement(doc);
	if (cur == NULL) {
		MSG_ERROR(msg_module, "Empty configuration");
		goto err_xml;
	}

	if (xmlStrcmp(cur->name, (const xmlChar *) "fileWriter")) {
		MSG_ERROR(msg_module, "root node != fileWriter");
		goto err_xml;
	}

	/* Process the configuration elements */
	cur = cur->xmlChildrenNode;
	while (cur != NULL) {
		if ((!xmlStrcmp(cur->name, (const xmlChar *) "timeout"))) {
			char *timeout = (char *) xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			if (timeout != NULL) {
				conf->reconnectTimeout = atoi(timeout);
				free(timeout);
			}
		} else if ((!xmlStrcmp(cur->name, (const xmlChar *) "remoteIPAddress"))) {
			if (!conf->host) {
				conf->host = (char *) xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			}
		} else if ((!xmlStrcmp(cur->name, (const xmlChar *) "remotePort"))) {
			if (!conf->port) {
				conf->port = (char *) xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			}
		} else if ((!xmlStrcmp(cur->name, (const xmlChar *) "IPProtocol"))) {
			char *ipText = (char *) xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			int ip = 0;

			if (ipText != NULL) {
				ip= atoi(ipText);
				free(ipText);
			}

			switch (ip) {
			case 4: conf->ip = AF_INET; break;
			case 6: conf->ip = AF_INET6; break;
			case 64:
				conf->ip = AF_INET6;
				conf->flags = AI_V4MAPPED | AI_ALL;
				break;
			default: conf->ip = AF_UNSPEC; break;
			}
		} else if ((!xmlStrcmp(cur->name, (const xmlChar *) "format"))) {
			if (!conf->format) {
				conf->format = (char *) xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			}
		}

		cur = cur->next;
	}

	/* Check that all necessary information is provided */
	if (!conf->format) {
		MSG_ERROR(msg_module, "UniRec format not given");
		goto err_xml;
	}

	if (!conf->host) {
		MSG_ERROR(msg_module, "Remote host not given");
		goto err_xml;
	}

	if (!conf->port) {
		MSG_ERROR(msg_module, "Remote port not given");
		goto err_xml;
	}


	MSG_NOTICE(msg_module, "Following configuration is used:\n\thostname: %s\n"
			"\tport: %s\n\ttimeout: %i\n\tformat: %s",
			conf->host, conf->port, conf->reconnectTimeout, conf->format);

	/* Use default values if not specified in configuration */
	if (conf->reconnectTimeout == 0) {
		conf->reconnectTimeout = DEFAULT_TIMEOUT;
	}

	/* Parse UniRec format */
	if (parse_format(conf)) {
		goto err_parse;
	}

	/* Allocate basic buffer for UniRec records */
	conf->buffer = malloc(100);
	if (!conf->buffer) {
		goto err_parse;
	}
	conf->bufferSize = 100;

	/* Make the connection attempt when sending first packet */
	conf->lastReconnect = 1;

	*config = conf;

	/* Destroy the XML configuration document */
	xmlFreeDoc(doc);

	return 0;

err_parse:
	free(conf->format);
	free(conf->host);
	free(conf->port);

	/* free fieldList */
	destroy_fields(conf->fields);

err_xml:
	xmlFreeDoc(doc);

err_init:
	free(conf);


	return -1;
}

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
int store_packet (void *config, const struct ipfix_message *ipfix_msg,
        const struct ipfix_template_mgr *template_mgr)
{
	if (config == NULL || ipfix_msg == NULL) {
		return -1;
	}

	/* Unnecessary? */
	if (template_mgr == NULL) {
		return -1;
	}

	struct unirec_config *conf = (struct unirec_config*) config;

	process_data_sets(ipfix_msg, conf);

	/* Send the record */
	send_packet(conf->buffer, conf->bufferOffset, conf);

	/* Remove old data from buffer */
	conf->bufferOffset = 0;

	return 0;
}

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
int store_now (const void *config)
{
	if (config == NULL) {
		return -1;
	}


	return 0;
}

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
int storage_close (void **config)
{
	unirec_config *conf = (unirec_config*) *config;

	free(conf->format);
	free(conf->host);
	free(conf->port);
	free(conf->buffer);

	/* free fieldList */
	destroy_fields(conf->fields);

	free(*config);
	return 0;
}
