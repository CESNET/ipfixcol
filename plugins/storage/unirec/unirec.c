/**
 * \file unirec.c
 * \author Petr Velan <petr.velan@cesnet.cz>
 * \author Erik Sabik <xsabik02@stud.fit.vutbr.cz>
 * \brief Plugin for converting IPFIX data to UniRec format
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
#include <libtrap/trap.h>

#include "unirec.h"

// Global variables
uint8_t INIT_COUNT = 0;  // Number of running instances of plugin

/* NEEDED FOR DEBUG CODE */
#include <arpa/inet.h>
#include <sys/time.h>


uint32_t DEBUG_PRINT = 0;
uint32_t DEBUG_PRINT2 = 0;

time_t curr_time;
time_t prev_time;
time_t step_time = 3;
uint32_t DEBUG_TIME_RECORD = 0;

/* some auxiliary functions for extracting data of exact length */
#define read8(ptr) (*((uint8_t *) (ptr)))
#define read16(ptr) (*((uint16_t *) (ptr)))
#define read32(ptr) (*((uint32_t *) (ptr)))
#define read64(ptr) (*((uint64_t *) (ptr)))

/** Identifier to MSG_* macros */
static char *msg_module = "unirec";

// Struct with information about module
trap_module_info_t module_info = {
	"ipfixcol UniRec plugin", // Module name
	// Module description
	"This is both Nemea module and ipfixcol plugin. It converts IPFIX records"
	"to UniRec format for Nemea.\n"
	"Interfaces:\n"
	"   Inputs: 0\n"
	"   Outputs: 1\n",
	0, // Number of input interfaces
	1, // Number of output interfaces
};

/**
 * \brief Works as memcpy but converts common data sizes to host byteorder
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
				*(uint64_t *) dst = be64toh(read64(src));
				dst += 16;
				*(uint64_t *) dst = be64toh(read64(src+16));
				break;
			default:
				memcpy(dst, src, length);
				break;
			}
}

/**
 * \brief Add `ODID` to every port in Trap interface specification 
 *
 * @param conf Pointer to storage plugin structure
 * @param ODID ODID from current exporter
 */
static void update_ifc_spec(unirec_config *conf, uint32_t ODID)
{
	char *state;
	char *port, *limit;	
	char buff[32];
	char ibuff[32];
	uint16_t port_num;
	int ifc_count = conf->ifc_count;


	for (int i = 0; i < ifc_count; i++) {
		memset(ibuff, 0, 32);
		memset(buff, 0, 32);
		state = NULL;

		strncpy(ibuff, conf->ifc_spec.params[i], 30);

		port  = strtok_r(ibuff, ",", &state);
		limit = strtok_r(NULL, ",", &state);
	
		port_num = atoi(port) + ODID;	
		sprintf(buff, "%u,", port_num);
		strncat(&(buff[strlen(buff)]), limit, 10);

		if (strlen(conf->ifc_spec.params[i]) < strlen(buff)) {
			conf->ifc_spec.params[i] = realloc(conf->ifc_spec.params[i], strlen(buff) + 1);
		}
		memcpy(conf->ifc_spec.params[i], buff, strlen(buff));
	}
}

/**
 * \brief Initialize Trap with updated interface specification
 *
 * @param conf Pointer to storage plugin structure
 * @return 1 on success, 0 otherwise
 */
static int init_trap_ifc(unirec_config *conf)
{
	MSG_DEBUG(msg_module, "Initializing TRAP (%u)...\n", conf->odid);
	if ((conf->trap_ctx_ptr = trap_ctx_init(&module_info, conf->ifc_spec)) == NULL) {
		MSG_ERROR(msg_module, "Error in TRAP initialization: (%i) %s\n",
		trap_last_error, trap_last_error_msg);
		printf("Error in TRAP initialization: (%i) %s\n",
		trap_last_error, trap_last_error_msg);
		return 0;
	}
	MSG_DEBUG(msg_module, "OK\n");


	for (int i = 0; i < conf->ifc_count; i++) {
		MSG_NOTICE(msg_module, "Setting interafece %u buffer %s\n", i, conf->ifc_buff_switch[i] ? "ON" : "OFF");
		trap_ctx_ifcctl(conf->trap_ctx_ptr, TRAPIFC_OUTPUT, i, TRAPCTL_BUFFERSWITCH, conf->ifc_buff_switch[i]);
		MSG_NOTICE(msg_module, "Setting interafece %u autoflush to %llu us\n", i, conf->ifc_buff_timeout[i]);
		trap_ctx_ifcctl(conf->trap_ctx_ptr, TRAPIFC_OUTPUT, i, TRAPCTL_AUTOFLUSH_TIMEOUT, conf->ifc_buff_timeout[i]);
		MSG_NOTICE(msg_module, "Setting interafece %u timeout to %llu us\n", i, conf->ifc[i].timeout);
		trap_ctx_ifcctl(conf->trap_ctx_ptr, TRAPIFC_OUTPUT, i, TRAPCTL_SETTIMEOUT, (int) conf->ifc[i].timeout);
	}
	return 1;
}

/**
 * \brief Return an UniRec field that matches given IPFIX element
 *
 * @param element Element to match
 * @param ht Hash table containing UniRec fields to search
 * @param ipfix_id Ipfix element id to fill.
 * @param en_id Enterprise id to fill.
 * @return return matching UniRec filed or NULL
 */
static unirecField *match_field(template_ie *element, fht_table_t *ht, uint16_t *ipfix_id, uint32_t *en_id)
{
	uint16_t id;
	uint32_t en;
	uint64_t ht_key;
	unirecField **tmp;

	id = element->ie.id;

	if (id >> 15) {
		en = (element+1)->enterprise_number;
		id = id & 0x7FFF;
	} else {
		en = 0;
	}

	*ipfix_id = id;
	*en_id = en;

	ht_key = (((uint64_t)en) << 32) | ((uint32_t)id);
	tmp = fht_get_data(ht, &ht_key);

	return tmp == NULL ? NULL : *tmp;
}

/**
 * \brief Get data from data record
 *
 * Uses conf->unirecFields to store values from this record
 *
 * \param[in] data_record IPFIX data record
 * \param[in] template corresponding template
 * \param[out] conf structure containing necessary information for converting ipfix to unirec
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

	uint16_t ipfix_id;
        uint32_t en_id;
	uint64_t sec, msec, frac;

	/* Go over all fields */
	for (count = index = 0; count < template->field_count; count++, index++) {
		matchField = match_field(&template->fields[index], conf->ht_fields, &ipfix_id, &en_id);

		length = template->fields[index].ie.length;
		size_length = 0;

		/* Handle variable length */
		if (length == VAR_IE_LENGTH) {
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
			// Determine if it is static or dynamic element
			// If static, copy to all Unirec ifcs whose are using this element
			// Else if dynamic, copy ptr to this element to `matchField->value` for futher use
			if (matchField->size != -1) {
				// Static element
				for (int i = 0; i < conf->ifc_count; i++) {
					// For every interface
					if (matchField->included_ar[i]) {
						// If element is present in current interface
						// Copy special elements in special way
						switch (matchField->type) {
							case UNIREC_FIELD_IP:
								if ((en_id == 0 && (ipfix_id == 8 || ipfix_id == 12)) ||
								    (en_id == 39499 && ipfix_id == 40)) {
									// IPv4 or INVEA_SIP_RTP_IPV4
									// Put IPv4 into 128 bits in a special way (see ipaddr.h in Nemea-UniRec for details)
									*(uint64_t*)(conf->ifc[i].buffer + matchField->offset_ar[i]) = 0;
									*(uint32_t*)(conf->ifc[i].buffer + matchField->offset_ar[i] + 8) = *(uint32_t*)(data_record + offset + size_length);
									*(uint32_t*)(conf->ifc[i].buffer + matchField->offset_ar[i] + 12) = 0xffffffff;
								} else {		
									// IPv6 or INVEA_SIP_RTP_IPV6
									memcpy(conf->ifc[i].buffer + matchField->offset_ar[i], data_record + offset + size_length, length);
								}
								break;
							case UNIREC_FIELD_PACKET:
								// PACKET SIZE IS DIFFERENT FOR DIFFERENT EXPORTER!!!
								if (template->fields[index].ie.length == 4) {
									*(uint32_t*)(conf->ifc[i].buffer + matchField->offset_ar[i]) =  ntohl(*(uint32_t*)(data_record + offset + size_length));
								}
								else if (template->fields[index].ie.length == 8) {
									*(uint32_t*)(conf->ifc[i].buffer + matchField->offset_ar[i]) =  ntohl(*(uint32_t*)(data_record + offset + size_length + 4));
                                                        	} else {
									*(uint32_t*)(conf->ifc[i].buffer + matchField->offset_ar[i]) =  0xFFFFFFFF;
								}
								break;
							case UNIREC_FIELD_TS:
								// Handle Time variables
								msec = be64toh(*(uint64_t*)(data_record + offset + size_length));
								sec = msec / 1000;
								frac = ((msec % 1000) * 0x4189374BC6A7EFULL) >> 32;
								*(uint64_t*)(conf->ifc[i].buffer + matchField->offset_ar[i]) =  (sec<<32) | frac;
								break;
							case UNIREC_FIELD_DBF:
								// Handle DIR_BIT_FIELD
								*(uint8_t*)(conf->ifc[i].buffer + matchField->offset_ar[i]) = (*(uint16_t*)(data_record + offset + size_length)) ? 1 : 0;
								break;
							case UNIREC_FIELD_LBF:
								// Handle LINK_BIT_FIELD, is BIG ENDIAN but we are using only LSB
								*(uint64_t*)(conf->ifc[i].buffer + matchField->offset_ar[i]) = 1LLU << ((*(uint8_t*)(data_record + offset + size_length + 3)) - 1);
								break;
							default:
								// Check length of ipfix element and if it is larger than unirec element, then saturate unirec element
								if (matchField->size >= length) {
									data_copy( (conf->ifc[i].buffer) + matchField->offset_ar[i],	// Offset to Unirec buffer for current ifc
										   (data_record + offset + size_length),		// Offset to data in Ipfix message
									   	length);						// Size of element
								} else {
									// set maximum value to unirec element
									memset( (conf->ifc[i].buffer) + matchField->offset_ar[i],
										0xFF,
										matchField->size);
								}
						}

						// if required, add required-filled count
						conf->ifc[i].requiredFilled += matchField->required_ar[i];
					}
				}
			} else {
				// Dynamic element
				matchField->valueSize = length;
				matchField->value     = (void*) (data_record + offset + size_length);
				matchField->valueFilled = 1;
				// Fill required count for Unirec where this element is required
				for (int i = 0; i < conf->ifc_count; i++) {
					// For every interface
					if (matchField->included_ar[i]) {
						// If element is present in current ifc
						conf->ifc[i].requiredFilled += matchField->required_ar[i];
					}
				}
			}
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
 * \brief Copy dynamic fields to output buffer
 *
 * \param[in] conf Pointer to interface config structure
 */
static void process_dynamic(ifc_config *conf)
{
	conf->bufferOffset = conf->bufferStaticSize;

	// Cycle throu all fields
	for (int i = 0; i < conf->dynCount; i++) {
		
		// Store end offset of dynamic value to Unirec buffer
		*(uint16_t*)(conf->buffer + conf->dynAr[i]->offset_ar[conf->number]) = conf->bufferDynSize + conf->dynAr[i]->valueSize; 
		
		// If dynamic field was filled, copy it to Unirec buffer
		if (conf->dynAr[i]->valueFilled) {
			memcpy(	conf->buffer + conf->bufferOffset,
				conf->dynAr[i]->value,
				conf->dynAr[i]->valueSize > MAX_DYNAMIC_FIELD_SIZE ? MAX_DYNAMIC_FIELD_SIZE : conf->dynAr[i]->valueSize);

			conf->bufferOffset  += conf->dynAr[i]->valueSize;
			conf->bufferDynSize += conf->dynAr[i]->valueSize;
			conf->dynAr[i]->valueFilled = 0;
			conf->dynAr[i]->valueSize = 0;
		}
	}
}

/**
 * \brief Process all data sets in IPFIX message
 *
 * \param[in] ipfix_msg IPFIX message
 * \param[in] conf Pointer to interface config structure
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
	int i;
	
	// ********** Store ODID ************* 
	uint32_t ODID = ntohl(ipfix_msg->pkt_header->observation_domain_id);
	// Fill ODID
	conf->odid = (uint16_t) ODID;
	// ***********************************

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

			// Process data record only once
			ret = process_record(data_record, template, conf);

			//Check that the record was processes successfuly
			if (ret == 0) {
				return -1;
			}
			
 	             	 //Fill dynamic fields for every UniRec record and send it
                	for (i = 0; i < conf->ifc_count; i++) {
				// Check if we have all required fields
				if (conf->ifc[i].requiredCount == conf->ifc[i].requiredFilled) {
					// Fill dynamic fields if there are ones
					if (conf->ifc[i].dynamic) {
						process_dynamic(&(conf->ifc[i]));
					}
					// Send record
					trap_ctx_send(	conf->trap_ctx_ptr,
							conf->ifc[i].number,
							conf->ifc[i].buffer,
							conf->ifc[i].bufferStaticSize + conf->ifc[i].bufferDynSize);
							// conf->ifc[i].timeout); // Timeout is set by IFCCTL	
				} else {
					// Set default values for dynamic fields
					for (int j = 0; j < conf->ifc[i].dynCount; j++) {
						conf->ifc[i].dynAr[j]->valueFilled = 0;
						conf->ifc[i].dynAr[j]->valueSize = 0;
					}
				}

				// Clear static fields
				memset(conf->ifc[i].buffer, 0, conf->ifc[i].bufferStaticSize);

				conf->ifc[i].requiredFilled = 0;
				conf->ifc[i].bufferDynSize = 0;
			}

			offset += ret;
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

	free(field->included_ar);
	free(field->required_ar);
	free(field->offset_ar);

	if (field->size > 0) {
		free(field->value);
	}

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
 * \brief Convert ipfix element id to unirec type for faster processing ipfix messages
 * \param ipfix_el Ipfix element structure.
 * \return One value from enum `unirecFieldEnum`.
 */
static int8_t getUnirecFieldTypeFromIpfixId(ipfixElement ipfix_el)
{
	uint16_t id = ipfix_el.id;
	uint32_t en = ipfix_el.en;

 	if ((en == 0 && (id == 8 || id == 12)) ||
            (en == 39499 && id== 40) ||
	    (en == 0 && (id == 27 || id == 28)) ||
            (en == 39499 && id == 41)) {
		// IP or INVEA_SIP_RTP_IP
		return UNIREC_FIELD_IP;
	} else if (en == 0 && id == 2) {
		// Packets
		return UNIREC_FIELD_PACKET;
	} else if (en == 0 && (id == 152 || id == 153)) {
		// Timestamps
		return UNIREC_FIELD_TS;
	} else if (en == 0 && id == 10) {
		// DIR_BIT_FIELD
		return UNIREC_FIELD_DBF;		
	} else if (en == 0 && id == 405) {
		// LINK_BIT_FIELD
		return UNIREC_FIELD_LBF;
	} else {
		// Other
		return UNIREC_FIELD_OTHER;
	}
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
		currentField->offset_ar = NULL;
		currentField->required_ar = NULL;
		currentField->included_ar = NULL;

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
						ipfixToken != NULL;
						ipfixToken = strtok_r(NULL, ",", &ipfixState), ipfixPosition++) {
					/* Enlarge the element if necessary */
					if (ipfixPosition > 0) {
						currentField = realloc(currentField, sizeof(unirecField) + (ipfixPosition) * sizeof(uint64_t));
					}
					/* Store the ipfix element id */
					currentField->ipfix[ipfixPosition] = ipfix_from_string(ipfixToken);
					currentField->ipfixCount++;
					/* Fill in Unirec field type based on ipfix element id */
					currentField->type = getUnirecFieldTypeFromIpfixId(currentField->ipfix[ipfixPosition]);
				}
				break;
			} // case 2 end
			} // switch end
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
 * @param dynamic Flag is set to 1 if current element is dynamic, 0 otherwise
 * @return Returns 0 on success
 */
static int update_field(unirecField **currentField, unirecField *confFields, int *dynamic)
{
	unirecField *tmp;
	int updated = 0;
	*dynamic = 0; // Assume there is no dynamic field, if there is then change this variable to 1

	/* Look for specified element */
	for (tmp = confFields; tmp != NULL; tmp = tmp->next) {
		if (strcmp((*currentField)->name, tmp->name) == 0) {
			(*currentField)->size = tmp->size;
			(*currentField)->ipfixCount = 0;
			(*currentField)->type = tmp->type;

			if (tmp->size == -1) {
				// If it is dynamic field, set dynamic to 1
				*dynamic = 1;
			}

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
 * \brief Parse format of unirec template for every interface specified in startup.xml.
 * \param[in] conf Pointer to interface config structure
 * @return Returns 0 on success
 */
static int parse_format(unirec_config *conf_plugin)
{
	char *token, *state; /* Variables for strtok  */
	unirecField *lastField, *currentField, *confFields;
	int ret;
	ifc_config *conf;
	int dynamic;
	uint16_t field_offset;
	uint32_t fields_count = 0;

	/* Load elements from configuration file */
	confFields = load_elements();

	for (int c = 0; c < conf_plugin->ifc_count; c++) {
		conf = &conf_plugin->ifc[c];
		conf->dynamic = 0;
		conf->dynCount = 0;
		field_offset = 0;

		lastField = NULL;
		currentField = NULL;

		if (conf->format == NULL) {
			return -1;
		}

		// Allocate memory for dynamic fields array
		conf->dynAr = malloc(sizeof(unirecField*) * INIT_DYNAMIC_ARR_SIZE);
		conf->dynArAlloc = INIT_DYNAMIC_ARR_SIZE;


		// Allocate memory for output data buffer
		conf->buffer = malloc(INIT_OUTPUT_BUFFER_SIZE);
		if (conf->buffer == NULL) {
			return -1;
		}
		conf->bufferAllocSize = INIT_OUTPUT_BUFFER_SIZE;
		memset(conf->buffer, 0, conf->bufferAllocSize);

		// Used for static size overflow check
		conf->bufferSize = conf->bufferAllocSize;

		// ****** Split string to tokens ******
		for (token = strtok_r(conf->format, ",", &state); token != NULL; token = strtok_r(NULL, ",", &state)) {
			// Add it to config array if it is not there yet
			lastField = conf_plugin->fields;

			// Check if field is already present in fields list or if it is new field
			while (1) {
				// Try to find field in field list
				if (lastField != NULL && strcmp(lastField->name, token[0] == '?' ? token + 1 : token) == 0) {
					// Element is already present in config array so we do not need to add it
					// but we need to adjust required_ar and included_ar and offset_ar
					if (token[0] == '?') {
						lastField->required_ar[c] = 0;
					} else {
						lastField->required_ar[c] = 1;
						
						// Do not count DIRECTION_FLAGS as required value because it is always present
						if (strcmp(token, "DIRECTION_FLAGS") != 0 ) {
							conf->requiredCount++;			
						}
					}
					lastField->included_ar[c] = 1;
					lastField->offset_ar[c] = field_offset;
					if (lastField->size == -1) {
						// Dynamic field
						field_offset += 2;		// 2B is size of offset of the end of dynamic field in unirec record
						conf->bufferStaticSize += 2;	// same as above

						// Add last field to dynAr and update dynamic field counter
						conf->dynAr[conf->dynCount] = lastField;
						conf->dynCount++;
						if (conf->dynCount >= conf->dynArAlloc) {
							conf->dynArAlloc += INIT_DYNAMIC_ARR_SIZE;
							conf->dynAr = realloc(conf->dynAr, sizeof(unirecField*) * conf->dynArAlloc);
						}

						// Update output buffer for new dynamic value
						conf->bufferAllocSize += MAX_DYNAMIC_FIELD_SIZE;
						conf->buffer = realloc(conf->buffer, conf->bufferAllocSize);
					} else {
						conf->bufferStaticSize += lastField->size;
						field_offset += lastField->size;

						// Update output buffer size if needed
						if (conf->bufferSize <= conf->bufferStaticSize) {
							conf->bufferAllocSize += INIT_OUTPUT_BUFFER_SIZE;
							conf->buffer = realloc(conf->buffer, conf->bufferAllocSize);	
							conf->bufferSize += INIT_OUTPUT_BUFFER_SIZE;
						}
					}

					break;
				}
				// If it is very first field or it was not found in fields list, then create it and add it to fields list
				if (lastField == NULL || lastField->next == NULL) {
					/* Create new field element */
					currentField = malloc(sizeof(unirecField));
					currentField->required_ar = malloc(sizeof(uint8_t)  * conf_plugin->ifc_count); 
					currentField->included_ar = malloc(sizeof(uint8_t)  * conf_plugin->ifc_count); 
					currentField->offset_ar   = malloc(sizeof(uint16_t) * conf_plugin->ifc_count); 
					memset(currentField->required_ar, 0, conf_plugin->ifc_count);
					memset(currentField->included_ar, 0, conf_plugin->ifc_count);
					memset(currentField->offset_ar,   0, 2 * conf_plugin->ifc_count);   // 2Bytes * ifc count
					/* Init values used for iteration */
					currentField->next = NULL;
					currentField->nextIfc = NULL;
					currentField->value = NULL;
					currentField->valueSize = 0;
					currentField->valueFilled = 0;
					currentField->ipfixCount = 0;
					currentField->included_ar[c] = 1;
					currentField->offset_ar[c] = field_offset;

					/* Fill name and required values */
					if (token[0] == '?') {
						currentField->required = 0;
						currentField->name = strdup(token + 1);
						currentField->required_ar[c] = 0;
					} else {
						conf->requiredCount++;
						currentField->required = 1;
						currentField->name = strdup(token);
						currentField->required_ar[c] = 1;
					}

					/* Handle special fields */
					if (strcmp(currentField->name, "DIRECTION_FLAGS") == 0) {
						conf->requiredCount--; // Is required but it is always present
						currentField->size = 1;
						currentField->value = malloc(1);   
						currentField->valueSize = 1;
						currentField->ipfixCount = 1;  // Only for compatibility with other elements
						currentField->ipfix[0].id = 0; // when comparing
						currentField->ipfix[0].en = 0; //
					} else {
						/* Fill values from configuration file */
						ret = update_field(&currentField, confFields, &dynamic);
						if (ret) {
							MSG_ERROR(msg_module, "Field \"%s\" is not present in UniRec configuration file", currentField->name);
							destroy_field(currentField);
							destroy_fields(confFields);
							return 1;
						}
						conf->dynamic |= dynamic; // Set dynamic field flag on interface
					}

					// Update offset of current field
					currentField->offset_ar[c] = field_offset;

					// Determine if field is static or dynamic
					if (currentField->size == -1) {
						// Dynamic field
						// Update offset for next field
						field_offset += 2;		// 2B is size of offset to the end of dynamic field
						conf->bufferStaticSize += 2;	// same as above

						// Add current field to dynAr and update dynamic field counter
						conf->dynAr[conf->dynCount] = currentField;
						conf->dynCount++;
						if (conf->dynCount >= conf->dynArAlloc) {
							conf->dynArAlloc += INIT_DYNAMIC_ARR_SIZE;
							conf->dynAr = realloc(conf->dynAr, sizeof(unirecField*) * conf->dynArAlloc);
						}

						// Update output buffer for new dynamic value
						conf->bufferAllocSize += MAX_DYNAMIC_FIELD_SIZE;
						conf->buffer = realloc(conf->buffer, conf->bufferAllocSize);

					} else {
						// Static field
						field_offset += currentField->size;
						conf->bufferStaticSize += currentField->size;

						// Update output buffer for new dynamic value
						if (conf->bufferSize <= conf->bufferStaticSize) {
							conf->bufferAllocSize += INIT_OUTPUT_BUFFER_SIZE;
							conf->buffer = realloc(conf->buffer, conf->bufferAllocSize);	
							conf->bufferSize += INIT_OUTPUT_BUFFER_SIZE;
						}

					}

					// Add current field to list
					if (lastField == NULL) {
						conf_plugin->fields = currentField;
					} else {
						lastField->next = currentField;
					}
					fields_count++;
					break;
				} // End of create new field condition
				lastField = lastField->next;
			}	// End of unique element cycle
		}	// End of current interface cycle
	}	// End of interfaces cycle

	/* Elements from UniRec configuration file are not needed anymore */
	destroy_fields(confFields);

	// DEBUG PRINT
	/*
	currentField = conf_plugin->fields;
	while(currentField != NULL) {
		printf("%s ", currentField->name);
		for (int i = 0; i < conf_plugin->ifc_count; i++) {
			printf("[%u,%u]", currentField->required_ar[i], currentField->included_ar[i]);
		}
		printf("\n");
		currentField = currentField->next;
	}
	*/

	// ***** Create fash hash table from fields list *****
	uint8_t round = 4;
	int dbg_i;
	fht_table_t *ht;
	while (1) {
		uint8_t insert_flag = 1;
		round++;
		if (round > 10) {
			MSG_ERROR(msg_module, "Could not insert all Unirec fields in hash table!");
			return 1;
		}
		ht = fht_init(1 << round,
		              FIELDS_HT_KEYSIZE,
		              sizeof(unirecField*),
		              FIELDS_HT_STASH_SIZE);
		
		for (dbg_i = 0, currentField = conf_plugin->fields; currentField != NULL ; dbg_i++, currentField = currentField->next) {
			for (int i = 0; i < currentField->ipfixCount; i++) {
				uint64_t ht_key = (((uint64_t)(currentField->ipfix[i].en)) << 32) | ((uint32_t)(currentField->ipfix[i].id));
				if (fht_insert(ht, &ht_key, &currentField, NULL, NULL) == FHT_INSERT_LOST) {
					insert_flag = 0;
					break;
				}
			}
			if (insert_flag == 0) {
				break;
			}
		}

		// Did we successfully inserted all fields in hash table?
		if (insert_flag) {
			// Success, break and continue
			break;
		} else {
			// Destroy current hash table and try again with larger size
			fht_destroy(ht);
		}
	}
	conf_plugin->ht_fields = ht;

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
	// Plugin can be initialized only once
	if (INIT_COUNT == 0) {
		INIT_COUNT++;
	} else {
		MSG_ERROR(msg_module, "Trying to initialize multiple times!");
		return -1;
	}

	printf("Initializing storage plugin\n");
	unirec_config *conf;
	xmlDocPtr doc;
	xmlNodePtr cur;
        xmlNodePtr cur_sub;
        int service_ifc_flag = 0;
	int ifc_count_read = 0;
	int ifc_count_space = 16;
	int ifc_count_step = 16;
	char **ifc_params = malloc(sizeof(char *) * ifc_count_space);
	char *ifc_types = malloc(sizeof(char) * ifc_count_space);
	int *ifc_timeout = malloc(sizeof(int) * ifc_count_space);
	char **ifc_format = malloc(sizeof(char *) * ifc_count_space);
	char *ifc_buff_switch = malloc(sizeof(char) * ifc_count_space);
	uint64_t *ifc_buff_timeout = malloc(sizeof(uint64_t) * ifc_count_space);
	trap_ifc_spec_t ifc_spec;

	// Allocate memory for main config structure
	conf = (unirec_config *) malloc(sizeof(unirec_config));
	if (!conf) {
		MSG_ERROR(msg_module, "Out of memory (%s:%d)", __FILE__, __LINE__);
		return -1;
	}
	memset(conf, 0, sizeof(unirec_config));

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
		if ((!xmlStrcmp(cur->name, (const xmlChar *) "interface"))) {
			ifc_count_read++;
			// Check size of buffers, realloc if needed
			if (ifc_count_read > ifc_count_space) {
				ifc_count_space += ifc_count_step;
				ifc_params = realloc(ifc_params, sizeof(char *) * ifc_count_space);
				ifc_types = realloc(ifc_types, sizeof(char) * ifc_count_space);
				ifc_timeout = realloc(ifc_timeout, sizeof(int) * ifc_count_space);
				ifc_format = realloc(ifc_format, sizeof(char *) * ifc_count_space);
				ifc_buff_switch = realloc(ifc_buff_switch, sizeof(char) * ifc_count_space);
				ifc_buff_timeout = realloc(ifc_buff_timeout, sizeof(uint64_t) * ifc_count_space);
			}
			cur_sub = cur->xmlChildrenNode;
			// Process interface elements
			while (cur_sub != NULL) {
				if ((!xmlStrcmp(cur_sub->name, (const xmlChar *) "type"))) {
					ifc_types[ifc_count_read-1] = *((char *) xmlNodeListGetString(doc, cur_sub->xmlChildrenNode, 1));
					if (ifc_types[ifc_count_read-1] == 's') {
						service_ifc_flag = 1;
					}
				} else
				if ((!xmlStrcmp(cur_sub->name, (const xmlChar *) "params"))) {
					ifc_params[ifc_count_read-1] = (char *) xmlNodeListGetString(doc, cur_sub->xmlChildrenNode, 1);
				} else
				if ((!xmlStrcmp(cur_sub->name, (const xmlChar *) "bufferSwitch"))) {
					ifc_buff_switch[ifc_count_read-1] = atoi((char *) xmlNodeListGetString(doc, cur_sub->xmlChildrenNode, 1));
				} else
				if ((!xmlStrcmp(cur_sub->name, (const xmlChar *) "flushTimeout"))) {
					ifc_buff_timeout[ifc_count_read-1] = strtoull((char *) xmlNodeListGetString(doc, cur_sub->xmlChildrenNode, 1), NULL, 10);
				} else
				if ((!xmlStrcmp(cur_sub->name, (const xmlChar *) "ifcTimeout"))) {
					char *timeout = (char *) xmlNodeListGetString(doc, cur_sub->xmlChildrenNode, 1);
					if (timeout != NULL) {
						ifc_timeout[ifc_count_read-1] = atoi(timeout);
						
						free(timeout);
					} else {
						ifc_timeout[ifc_count_read-1] = DEFAULT_TIMEOUT;
					}
				} else
				if ((!xmlStrcmp(cur_sub->name, (const xmlChar *) "format"))) {
					ifc_format[ifc_count_read-1] = (char *) xmlNodeListGetString(doc, cur_sub->xmlChildrenNode, 1);
				}
				cur_sub = cur_sub->next;
			}
		}

		cur = cur->next;
	}

	int i;
	/* Clear uninitialized data */
	// If there is no more space for NULL terminating byte then realloc
	if (ifc_count_read == ifc_count_space) {
		ifc_types = realloc(ifc_types, sizeof(char) * ifc_count_space + 1);
		ifc_types[ifc_count_space] = '\0';
	} else {
		for (i = ifc_count_read; i < ifc_count_space; i++) {
			ifc_types[i] = '\0';
			ifc_params[i] = NULL;
			ifc_buff_switch[i] = 0;
		}
	}
	ifc_spec.types = ifc_types;
	ifc_spec.params = ifc_params;

	// Set Trap context variables
	conf->trap_init = 0;
	conf->ifc_spec.types = ifc_types;
	conf->ifc_spec.params = ifc_params;
	conf->ifc_buff_switch = ifc_buff_switch;
	conf->ifc_buff_timeout = ifc_buff_timeout;

	// Allocate array of pointers to interface config structures
	conf->ifc_count = ifc_count_read - service_ifc_flag;
	conf->ifc = (ifc_config *) malloc(sizeof(ifc_config) * conf->ifc_count);
	if (!conf->ifc) {
		MSG_ERROR(msg_module, "Out of memory (%s:%d)", __FILE__, __LINE__);
		return -1;
	}
	memset(conf->ifc, 0, sizeof(ifc_config *) * conf->ifc_count);


	// Set initial data to interface config structures
	for (i = 0; i < conf->ifc_count; i++) {
		// Set initial data
		conf->ifc[i].number = i;
		conf->ifc[i].format = ifc_format[i];
		conf->ifc[i].timeout = ifc_timeout[i];	
		conf->ifc[i].special_field_odid = NULL;
		conf->ifc[i].special_field_link_bit_field = NULL;
		conf->ifc[i].requiredCount = 0;
		conf->ifc[i].requiredFilled = 0;
		conf->ifc[i].bufferStaticSize = 0;
		conf->ifc[i].bufferDynSize = 0;
		conf->ifc[i].bufferAllocSize = 0;

		/* Check that all necessary information is provided */
		if (!conf->ifc[i].format) {
			MSG_ERROR(msg_module, "UniRec format not given");
			goto err_xml;
		}

		if (!ifc_spec.params[i]) {
			MSG_ERROR(msg_module, "Parameters of TRAP interface not given");
			goto err_xml;
		}


		/* Use default values if not specified in configuration */
		if (conf->ifc[i].timeout == 0) {
			conf->ifc[i].timeout = DEFAULT_TIMEOUT;
		}
	}

	/* Check TRAP interface types */
	if (!ifc_spec.types) {
		MSG_ERROR(msg_module, "Type of TRAP interface not given");
		goto err_xml;
	}

	if (parse_format(conf)) {
		goto err_parse;
	}


	/* Set number of TRAP output interfaces */
	module_info.num_ifc_out = conf->ifc_count;
	/* Set verbosity of TRAP library */
	if (verbose == ICMSG_ERROR) {
		trap_set_verbose_level(-1);
	} else if (verbose == ICMSG_WARNING) {
		trap_set_verbose_level(0); //0
	} else if (verbose == ICMSG_NOTICE) {
		trap_set_verbose_level(1); // 0
	} else if (verbose == ICMSG_DEBUG) {
		trap_set_verbose_level(2); // 0
	}
	MSG_NOTICE(msg_module, "Verbosity level of TRAP set to %i\n", trap_get_verbose_level());


	/* Initialize Trap */
	if (!init_trap_ifc(conf)) {
		MSG_ERROR(msg_module, "Could not initialize TRAP\n");
	}

	/* Copy configuration */
	*config = conf;

	/* Destroy the XML configuration document */
	xmlFreeDoc(doc);


	/* Free unnecessary data */
	free(ifc_format);
	free(ifc_timeout);

	return 0;

err_parse:
	for (i = 0; i < conf->ifc_count; i++) {
		/* free format */
		free(conf->ifc[i].format);

		/* free fieldList */
		destroy_fields(conf->ifc[i].fields);
	}
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

	struct unirec_config *conf = (struct unirec_config*) config;

	/* Process all data in the ipfix packet */
	if (!process_data_sets(ipfix_msg, conf)) {
		return -1;
	}

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

	printf("Plugin is shuting down for ODID: %u\n", conf->odid);

	trap_ctx_finalize(&conf->trap_ctx_ptr);

	// Free everything
	int i;
	for (i = 0 ; i < conf->ifc_count; i++) {
		free(conf->ifc[i].format);
		free(conf->ifc[i].buffer);
		free(conf->ifc[i].dynAr);
	}
        destroy_fields(conf->fields);

	free(*config);
	return 0;
}
