/**
 * \file nfinput.c
 * \author Michal Kozubik <kozubik@cesnet.cz>
 * \brief nfdump input plugin for IPFIX collector.
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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <stdint.h>
#include <dlfcn.h>
#include <time.h>
#include <signal.h>
#include <ipfixcol.h>
#include <dirent.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

// LZO library for decompression of data blocks
#include <lzo/lzo1x.h>

#include "nffile.h"
#include "ext_parse.h"
#include "ext_fill.h"

/* API version constant */
IPFIXCOL_API_VERSION;

static const char *msg_module = "nfdump input";

#define NO_INPUT_FILE (-2)

#define BASIC_TEMPLATE_ID (-1)
#define UNKNOWN_TEMPLATE (-1)

/**
 * \brief Extension map structure
 */
struct extensions {
	unsigned int filled;
	unsigned int size;
	struct extension *map;
};

/**
 * \brief List of input info structures
 */
struct input_info_file_list {
	struct input_info_file in_info;
	struct input_info_file_list	*next;
};

/**
 * \brief Plugin configuration structure
 */
struct nfinput_config {
	int fd;                  /**< file descriptor */
	xmlChar *xml_file;       /**< input file URI from XML configuration file. (e.g.: "file://tmp/ipfix.dump") */
	char *file;              /**< path where to look for IPFIX files. same as xml_file, but without 'file:' */
	char **input_files;      /**< list of all input files */
	int findex;              /**< index to the current file in the list of files */
	struct input_info_file_list	*in_info_list;
	struct input_info_file *in_info; /**< info structure about current input file */
	struct extensions ext;           /**< extensions map */
	struct ipfix_template_mgr_record template_mgr; /**< template manager */
	struct file_header_s header;     /**< header of readed file */
	struct stat_record_s stats;      /**< stats record */
	int basic_added;                 /**< flag indicating if basic templates was added */
	uint32_t block;                  /**< block number in current file */

	char *block_buffer;              /**< Beginning of data block buffer     */
	struct data_block_header_s block_header;  /**< Current data block header */
	struct record_header_s *block_cur_rec;    /**< Pointer on current record in the block buffer */
	uint32_t block_record;           /**< Record number in current block */
	uint32_t data_records_sent;      /**< Number of already sent DATA records */
};

struct extension {
	uint16_t *value; //map array
	int values_count;
	int id;
	int tmp6_index;  //index of ipv6 template for this extension map
	int tmp4_index;  //index of ipv4 template for this extension map
};

/**
 * \brief Functions for parsing extensions
 */
static void (*ext_parse[]) (uint32_t *data, int *offset, uint16_t flags, struct ipfix_data_set *data_set) = {
		ext0_parse,
		ext1_parse,
		ext2_parse,
		ext3_parse,
		ext4_parse,
		ext5_parse,
		ext6_parse,
		ext7_parse,
		ext8_parse,
		ext9_parse,
		ext10_parse,
		ext11_parse,
		ext12_parse,
		ext13_parse,
		ext14_parse,
		ext15_parse,
		ext16_parse,
		ext17_parse,
		ext18_parse,
		ext19_parse,
		ext20_parse,
		ext21_parse,
		ext22_parse,
		ext23_parse,
		ext24_parse,
		ext25_parse,
		ext26_parse,
		ext27_parse
};

// Size of ext_parse array
#define EXT_PARSE_CNT (sizeof(ext_parse) / sizeof(*ext_parse))

/**
 * \brief Functions for filling templates by extensions
 */
static void (*ext_fill_tm[]) (uint16_t flags, struct ipfix_template * template) = {
		ext0_fill_tm,
		ext1_fill_tm,
		ext2_fill_tm,
		ext3_fill_tm,
		ext4_fill_tm,
		ext5_fill_tm,
		ext6_fill_tm,
		ext7_fill_tm,
		ext8_fill_tm,
		ext9_fill_tm,
		ext10_fill_tm,
		ext11_fill_tm,
		ext12_fill_tm,
		ext13_fill_tm,
		ext14_fill_tm,
		ext15_fill_tm,
		ext16_fill_tm,
		ext17_fill_tm,
		ext18_fill_tm,
		ext19_fill_tm,
		ext20_fill_tm,
		ext21_fill_tm,
		ext22_fill_tm,
		ext23_fill_tm,
		ext24_fill_tm,
		ext25_fill_tm,
		ext26_fill_tm,
		ext27_fill_tm
};

// Size of ext fill array
#define EXT_FILL_CNT (sizeof(ext_fill_tm) / sizeof(*ext_fill_tm))

#define HEADER_ELEMENTS 8
static int header_elements[][2] = {
		//id,size
		{89,4},  // forwardingStatus
		{152,8}, // flowStartMilliseconds
		{153,8}, // flowEndMilliseconds
		{6,2},   // tcpControlBits
		{4,1},   // protocolIdentifier
		{5,1},   // ipClassOfService
		{7,2},   // sourceTransportPort
		{11,2}   // destinationTransportPort
};

#define ALLOC_FIELDS_SIZE 60

// Function prototype
int next_file(struct nfinput_config *conf);

/**
 * \brief Fill in data record with basic data common for block
 * 
 * \param data_set New data set
 * \param record nfdump record
 */
void fill_basic_data(struct ipfix_data_set *data_set, struct record_header_s *record_head){
	/**All required items in structures (common_record_s and common_record_v0_s)
	 * have same offset */
	struct common_record_s *record = (struct common_record_s*) record_head;

	// forwardingStatus
	data_set->records[data_set->header.length] = record->fwd_status;
	data_set->header.length += 4;
	// flowStartMilliseconds
	*((uint64_t *) &(data_set->records[data_set->header.length])) = htobe64((uint64_t)record->first*1000+record->msec_first); //sec 2 msec
	data_set->header.length += 8;
	// flowEndMilliseconds
	*((uint64_t *) &(data_set->records[data_set->header.length])) = htobe64((uint64_t)record->last*1000+record->msec_last); //sec 2 msec
	data_set->header.length += 8;
	// tcpControlBits
	data_set->records[data_set->header.length+1] = record->tcp_flags;
	data_set->header.length += 2;
	// protocolIdentifier
	data_set->records[data_set->header.length] =record->prot;
	data_set->header.length += 1;
	// ipClassOfService
	data_set->records[data_set->header.length] =record->tos;
	data_set->header.length += 1;
	// sourceTransportPort
	*((uint16_t *) &(data_set->records[data_set->header.length])) = htons(record->srcport);
	data_set->header.length += 2;
	// destinationTransportPort
	*((uint16_t *) &(data_set->records[data_set->header.length])) = htons(record->dstport);
	data_set->header.length += 2;
}

/**
 * \brief Fill in IPFIX template with basic data common for each block
 * 
 * \param flags some flags
 * \param template new IPFIX template
 */
void fill_basic_template(uint16_t flags, struct ipfix_template **template){
	static int template_id_counter = 256;

	(*template) = (struct ipfix_template *) calloc(1, sizeof(struct ipfix_template) + \
			ALLOC_FIELDS_SIZE * sizeof(template_ie));
	
	if(*template == NULL){
		MSG_ERROR(msg_module, "Malloc failed to get space for ipfix template");
		return;
	}

	(*template)->template_type = TM_TEMPLATE;
	(*template)->last_transmission = time(NULL);
	(*template)->last_message = 0;
	(*template)->template_id = template_id_counter;
	template_id_counter++;

	(*template)->field_count = 0;
	(*template)->scope_field_count = 0;
	(*template)->template_length = 0;
	(*template)->data_length = 0;

	// add header elements into template
	int i;
	for(i = 0; i < HEADER_ELEMENTS; i++){
		(*template)->fields[(*template)->field_count].ie.id = header_elements[i][0];
		(*template)->fields[(*template)->field_count].ie.length = header_elements[i][1];
		(*template)->field_count++;
		(*template)->data_length += header_elements[i][1];
		(*template)->template_length += 4;
	}

	//add mandatory extensions elements 
	//Extension 1
	ext_fill_tm[1] (flags, *template);
	//Extension 2
	ext_fill_tm[2] (flags, *template);
	//Extension 3
	ext_fill_tm[3] (flags, *template);
}

/**
 * \brief Initialize IPFIX message structure
 * 
 * \param ipfix_msg new ipfix message
 */
void init_ipfix_msg(struct ipfix_message *ipfix_msg){
	ipfix_msg->pkt_header = (struct ipfix_header *) malloc(sizeof(struct ipfix_header));
	ipfix_msg->pkt_header->version = htons(0x000a);
	ipfix_msg->pkt_header->length = htons(IPFIX_HEADER_LENGTH); //header size 
	ipfix_msg->pkt_header->export_time = htonl(time(NULL));
	ipfix_msg->pkt_header->sequence_number = 0;
	ipfix_msg->pkt_header->observation_domain_id = 0; 
}

/**
 * \brief Add new data set into IPFIX message
 * 
 * \param ipfix_msg Current IPFIX message
 * \param data_set Data set to be added
 * \param template Data set's template
 */
void add_data_set(struct ipfix_message *ipfix_msg, struct ipfix_data_set *data_set, struct ipfix_template *template){
	int i;
	for (i = 0; i < MSG_MAX_DATA_COUPLES; i++) {
		if (ipfix_msg->data_couple[i].data_set == NULL) {
			data_set->header.length = htons(data_set->header.length);
			ipfix_msg->data_couple[i].data_set = data_set;
			ipfix_msg->data_couple[i].data_template = template;
			break;
		}
	}
	ipfix_msg->pkt_header->length = htons(ntohs(ipfix_msg->pkt_header->length) + ntohs(data_set->header.length));
}

/**
 * \brief Add new template into IPFIX message
 * 
 * \param ipfix_msg Current IPFIX message
 * \param template New template
 */
void add_template(struct ipfix_message *ipfix_msg, struct ipfix_template * template){
	int i;
	for (i = 0; i < MSG_MAX_TEMPL_SETS; i++) {
		if (ipfix_msg->templ_set[i] == NULL) {
			ipfix_msg->templ_set[i] = (struct ipfix_template_set *) \
					calloc(1, sizeof(struct ipfix_template_set) + 8+template->template_length);

			ipfix_msg->templ_set[i]->header.flowset_id = htons(2);
			ipfix_msg->templ_set[i]->header.length = htons(8 + template->template_length);
			ipfix_msg->templ_set[i]->first_record.template_id = htons(template->template_id);
			ipfix_msg->templ_set[i]->first_record.count = htons(template->field_count);
			
			/* Copy fields, switch byte order */
			int j;
			for (j = 0; j < template->field_count; j++) {
				ipfix_msg->templ_set[i]->first_record.fields[j].ie.id = htons(template->fields[j].ie.id);
				ipfix_msg->templ_set[i]->first_record.fields[j].ie.length = htons(template->fields[j].ie.length);
			}
			break;
		}
	}
//	MSG_ERROR(msg_module, "set len %d templ %d", htons(ipfix_msg->templ_set[i]->header.length), template->template_length);
	ipfix_msg->pkt_header->length = htons(ntohs(ipfix_msg->pkt_header->length) + ntohs(ipfix_msg->templ_set[i]->header.length));
}

/**
 * \brief Clean template manager - remove all templates
 * 
 * \param manager Template manager
 */
void clean_tmp_manager(struct ipfix_template_mgr_record *manager){
	int i;
	for(i = 0; i <= manager->counter; i++ ) {
		if (manager->templates[i] != NULL){
			free(manager->templates[i]);
			manager->templates[i] = NULL;
		}
	}
	manager->counter = 0;
	
	free(manager->templates);
}

/**
 * \brief Parse nfdump data record
 * 
 * \param record nfdump record
 * \param ext extension map
 * \param template_mgr Template manager
 * \param msg Current IPFIX message
 * \return 0 on success
 */
int process_ext_record(struct record_header_s *record, struct extensions *ext,
	struct ipfix_template_mgr_record *template_mgr, struct ipfix_message *msg)
{
	int data_offset = 0;
	int id,eid;
	unsigned j;

	uint16_t rec_flags;
	uint16_t rec_ext_map;
	uint32_t *rec_data;

	switch(record->type) {
	case CommonRecordV0Type: {
		struct common_record_v0_s *tmp_rec = (struct common_record_v0_s*) record;
		rec_flags = tmp_rec->flags;
		rec_ext_map = tmp_rec->ext_map;
		rec_data = tmp_rec->data;
		}
		break;
	case CommonRecordType: {
		struct common_record_s *tmp_rec = (struct common_record_s*) record;
		rec_flags = tmp_rec->flags;
		rec_ext_map = tmp_rec->ext_map;
		rec_data = tmp_rec->data;
		}
		break;
	default:
		MSG_ERROR(msg_module, "Failed to process unknown data record (ID: %u)", record->type);
		return -1;
	}

	id = UNKNOWN_TEMPLATE;
	// Find index of correct extension map
	for (unsigned i = 0; i < ext->filled + 1; i++) {
		if (ext->map[i].id == rec_ext_map) {
			id = i;
		}
	}

	if (id == UNKNOWN_TEMPLATE) {
		MSG_WARNING(msg_module, "Record with unknown (or unsupported) extension map skipped.");
		// Or we can use default template only with mandatory extensions (template_mgr->templates[0])
		return 0;
	}

	struct ipfix_template *tmp = NULL;
	struct ipfix_data_set *set = NULL;
	
	if (TestFlag(rec_flags, FLAG_IPV6_ADDR)) {
		tmp = template_mgr->templates[ext->map[id].tmp6_index];
	} else {
		tmp = template_mgr->templates[ext->map[id].tmp4_index];
	}

	set = (struct ipfix_data_set *) calloc(1, sizeof(struct ipfix_data_set) + tmp->data_length);
	if (set == NULL) {
		MSG_ERROR(msg_module, "Malloc failed getting memory for data set");
		return -1;
	}
	set->header.flowset_id = htons(tmp->template_id);

	fill_basic_data(set, record);
	ext_parse[1](rec_data, &data_offset, rec_flags, set);
	ext_parse[2](rec_data, &data_offset, rec_flags, set);
	ext_parse[3](rec_data, &data_offset, rec_flags, set);

	for(int item = 0; item < ext->map[id].values_count; item++) {
		int ext_id = ext->map[id].value[item];
		ext_parse[ext_id](rec_data, &data_offset, rec_flags, set);
	}

	set->header.length += sizeof(struct ipfix_set_header);
	add_data_set(msg, set, tmp);
	return 0;
}

/**
 * \brief Parse nfdump extension map record
 * 
 * \param extension_map nfdump record
 * \param ext extensions map
 * \param template_mgr Template manager
 * \param msg Current IPFIX message
 * \return 0 on success
 */
int process_ext_map(struct record_header_s *record, struct extensions *ext,
		struct ipfix_template_mgr_record *template_mgr, struct ipfix_message *msg)
{	
	struct extension_map_s *extension_map = (struct extension_map_s*) record;

	// Check if all extensions are supported
	int eid = 0;
	while (extension_map->ex_id[eid] != 0) {
		if (extension_map->ex_id[eid] >= EXT_FILL_CNT) {
			// Unsupported extension found
			MSG_WARNING(msg_module, "Input file contains extension map (ID: %u) "
				"with unsupported extension(s). Records that belongs to this "
				"map will be skipped.", extension_map->map_id);
			return 0;
		}
		++eid;
	}

	// Add new template
	ext->filled++;
	if (ext->filled == ext->size) {
		//double the size of extension map array
		ext->map = (struct extension *) realloc(ext->map, (ext->size * 2) * sizeof(struct extension));
		if (ext->map == NULL) {
			MSG_ERROR(msg_module, "Can't reallocation extension map array");
			return -1;
		}
		ext->size *= 2;
	}

	ext->map[ext->filled].value = (uint16_t *) malloc(extension_map->extension_size);
	ext->map[ext->filled].values_count = 0;
	ext->map[ext->filled].id = extension_map->map_id;

	if (template_mgr->counter + 2 >= template_mgr->max_length) {
		//double the size of extension map array
		if ((template_mgr->templates = (struct ipfix_template **) realloc(template_mgr->templates, sizeof(struct ipfix_template *) * (template_mgr->max_length * 2))) == NULL) {
			MSG_ERROR(msg_module, "Can't reallocation extension map array");
			return -1;
		}
		int i;
		for (i = template_mgr->counter + 1; i < template_mgr->max_length * 2; ++i) {
			template_mgr->templates[i] = NULL;
		}
		template_mgr->max_length *= 2;
	}

	struct ipfix_template *template1;
	struct ipfix_template *template2;

	template_mgr->counter++;
	//template for this record with ipv4
	fill_basic_template(0, &(template_mgr->templates[template_mgr->counter]));
	template1 = template_mgr->templates[template_mgr->counter];
	ext->map[ext->filled].tmp4_index = template_mgr->counter;

	template_mgr->counter++;
	//template for this record with ipv6
	fill_basic_template(1, &(template_mgr->templates[template_mgr->counter]));
	template2 = template_mgr->templates[template_mgr->counter];
	ext->map[ext->filled].id = extension_map->map_id;
	ext->map[ext->filled].tmp6_index = template_mgr->counter;

	eid = 0;
	while (extension_map->ex_id[eid] != 0) {
		ext->map[ext->filled].value[eid] = extension_map->ex_id[eid];
		ext->map[ext->filled].values_count++;
		ext_fill_tm[extension_map->ex_id[eid]] (0, template1);
		ext_fill_tm[extension_map->ex_id[eid]] (1, template2);
		++eid;
	}

	add_template(msg, template1);
	add_template(msg, template2);
	return 0;
}

/**
 * \brief Free extensions map
 * 
 * \param ext map
 */
void free_ext(struct extensions *ext)
{
	unsigned int i;
	
	for (i = 0; i <= ext->filled; i++) {
		free(ext->map[i].value);
	}
	
	free(ext->map);
}

/**
 * \brief Clean up
 *
 * \param[in] config  configuration structure
 * \return 0 on success, negative value otherwise
 */
int input_close(void **config)
{
	struct nfinput_config *conf = (struct nfinput_config *) *config;
	struct input_info_file_list *aux_list = conf->in_info_list;
	
	int i;
	if (conf->input_files) {
		for (i = 0; conf->input_files[i]; ++i) {
			free(conf->input_files[i]);
		}
		free(conf->input_files);
	}
	
	while (aux_list) {
		conf->in_info_list = conf->in_info_list->next;
		free(aux_list);
		aux_list = conf->in_info_list;
	}
	
	xmlFree(conf->xml_file);
	free(conf->in_info);
	free(conf->block_buffer);
	
	free_ext(&(conf->ext));
	clean_tmp_manager(&(conf->template_mgr));
	free(conf);
}


/**
 * \brief Conver IPFIX message into packet
 * \param[in] msg IPFIX message structure
 * \param[out] packet_length Length of packet
 * \return pointer to packet
 */
void *message_to_packet(const struct ipfix_message *msg, int *packet_length)
{
	struct ipfix_set_header *aux_header;
	int i, c, len, offset = 0;

	*packet_length = ntohs(msg->pkt_header->length);
	void *packet = calloc(1, *packet_length);

	if (!packet) {
		return NULL;
	}

	/* Copy header */
	memcpy(packet, msg->pkt_header, IPFIX_HEADER_LENGTH);
	offset += IPFIX_HEADER_LENGTH;

	/* Copy template sets */
	for (i = 0; i < MSG_MAX_TEMPL_SETS && msg->templ_set[i]; ++i) {
		aux_header = &(msg->templ_set[i]->header);
		len = ntohs(aux_header->length);
		for (c = 0; c < len; c += 4) {
			memcpy(packet + offset + c, aux_header, 4);
			aux_header++;
		}
		offset += len;
	}

	/* Copy template sets */
	for (i = 0; i < MSG_MAX_OTEMPL_SETS && msg->opt_templ_set[i]; ++i) {
		aux_header = &(msg->opt_templ_set[i]->header);
		len = ntohs(aux_header->length);
		for (c = 0; c < len; c += 4) {
			memcpy(packet + offset + c, aux_header, 4);
			aux_header++;
		}
		offset += len;
	}

	/* Copy data sets */
	for (i = 0; i < MSG_MAX_DATA_COUPLES && msg->data_couple[i].data_set; ++i) {
		len = ntohs(msg->data_couple[i].data_set->header.length);
		memcpy(packet + offset,   &(msg->data_couple[i].data_set->header), 4);
		memcpy(packet + offset + 4, msg->data_couple[i].data_set->records, len - 4);
		offset += len;
	}

	return packet;
}

/**
 * \brief Get new record from nfdump file(s)
 * This function loads data blocks from nfdump files to internal buffer and
 * prepares pointer to new record.
 *
 * \param[in,out] conf Plugin configuration
 * \param[out] record Pointer to new record in internal buffer
 * \return On success returns size of new record (in bytes). Otherwise returns
 *         INPUT_ERROR or INPUT_CLOSED.
 */
int get_next_record(struct nfinput_config *conf, record_header_t **record)
{
	ssize_t read_size;
	int ret;
	record_header_t *rec_ptr, *next_rec_ptr;
	char *block_end;

	// Is there next record in same data block
	if (conf->block_cur_rec != NULL) {
		next_rec_ptr = (record_header_t *)(((char *)conf->block_cur_rec) + conf->block_cur_rec->size);
		block_end = conf->block_buffer + conf->block_header.size;

		// Is this end of data block?
		if (conf->block_record < conf->block_header.NumRecords && (char*) next_rec_ptr < block_end) {
			(*record) = next_rec_ptr;
			conf->block_cur_rec = next_rec_ptr;
			conf->block_record++;
			return next_rec_ptr->size;
		}

		conf->block_record = 0;
		conf->block_cur_rec = NULL;
	}

read_new_block:
	// Read new block
	if (conf->block < conf->header.NumBlocks) {
		// Read header of data block
		read_size = read(conf->fd, &conf->block_header, sizeof(data_block_header_t));
		if (read_size < 0) {
			MSG_ERROR(msg_module, "Failed to read data block header: %s", strerror(errno));
			return INPUT_ERROR;
		} else if (read_size == 0) {
			// End of file -> next file
			MSG_WARNING(msg_module, "Unexpected end of file.");
			goto read_new_file;
		} else if (read_size != sizeof(data_block_header_t)) {
			// Part of data block header is missing
			MSG_ERROR(msg_module, "Data block is probably corrupted.");
			return INPUT_ERROR;
		}
		conf->block++;

		// Check version of data block
		if (conf->block_header.id != DATA_BLOCK_TYPE_2) {
			// Unsupported data block type
			MSG_ERROR(msg_module, "Unsupported data block detected.");
			return INPUT_ERROR;
		}

		// Check size of buffer
		if (conf->block_header.size > BUFFSIZE) {
			// Maximum size of datablock should be same as BUFFSIZE!
			MSG_ERROR(msg_module, "Datablock is too large.");
			return INPUT_ERROR;
		}

		// Read content of data block
		read_size = read(conf->fd, conf->block_buffer, conf->block_header.size);
		if (read_size < 0) {
			MSG_ERROR(msg_module, "Failed to read data block content: %s", strerror(errno));
			return INPUT_ERROR;
		} else if (read_size == 0 && conf->block_header.size != 0) {
			// End of file -> next file
			MSG_WARNING(msg_module, "Unexpected end of file.");
			goto read_new_file;
		} else if (read_size != conf->block_header.size) {
			// Part of data block content is missing
			MSG_ERROR(msg_module, "Data block is probably corrupted.");
			return INPUT_ERROR;
		}

		// Is there any record?
		if (conf->block_header.NumRecords == 0) {
			MSG_WARNING(msg_module, "Empty data block found.");
			goto read_new_block;
		}

		// Decompress data block
		if (conf->header.flags & FLAG_COMPRESSED) {
			// Create new buffer
			char *new_buffer = (char*) malloc(BUFFSIZE);
			if (!new_buffer) {
				MSG_ERROR(msg_module, "Unable to allocate memory (%s:%d)", __FILE__, __LINE__);
				return INPUT_ERROR;
			}

			uint64_t new_size = BUFFSIZE;
			if (lzo1x_decompress(conf->block_buffer, conf->block_header.size,
					new_buffer, &new_size, NULL) != LZO_E_OK) {
				MSG_ERROR(msg_module, "Failed to decompress data block.");
				free(new_buffer);
				return INPUT_ERROR;
			}

			conf->block_header.size = new_size;
			free(conf->block_buffer);
			conf->block_buffer = new_buffer;
		}

		// Prepare new record
		rec_ptr = (record_header_t *) conf->block_buffer;
		(*record) = rec_ptr;
		conf->block_cur_rec = rec_ptr;
		conf->block_record = 1;  // First record is returned
		return rec_ptr->size;
	}

read_new_file:
	// Is there any new file?
	ret = next_file(conf);
	if (!ret) {
		conf->block = 0;
		goto read_new_block;
	}

	return (ret == NO_INPUT_FILE) ? INPUT_CLOSED : INPUT_ERROR;
}


/**
 * \brief Read nfdump message from file
 *
 * \param[in] config  input plugin config structure
 * \param[out] info  information about source of the IPFIX data 
 * \param[out] packet  IPFIX message in memory
 * \param[out] source_status Status of source (new, opened, closed)
 * \return length of the message on success. otherwise:
 * INPUT_INTR - on signal interrupt,
 * INPUT_CLOSED - if there are no more input files,
 * negative value on other possible errors
 */ 
int get_packet(void *config, struct input_info **info, char **packet, int *source_status)
{
	const uint32_t max_records_per_packet = 30; // Make only small packets

	uint32_t processed_records = 0;
	uint32_t processed_data_records = 0;
	int ret_val = 0, stop = 0, packet_len = 0;
	struct nfinput_config *conf = (struct nfinput_config *) config;

	// Prepare and init new message
	struct ipfix_message *ipfix_msg = calloc(1, sizeof(struct ipfix_message));
	if (!ipfix_msg) {
		MSG_ERROR(msg_module, "Unable to allocate memory (%s:%d)", __FILE__, __LINE__);
		return INPUT_ERROR;
	}

	// Init ipfix messege structure
	init_ipfix_msg(ipfix_msg);
	ipfix_msg->pkt_header->sequence_number = htonl(conf->data_records_sent);


	if (!conf->basic_added) {
		// Add basic templates
		conf->basic_added = 1;
		add_template(ipfix_msg, conf->template_mgr.templates[0]);
		add_template(ipfix_msg, conf->template_mgr.templates[1]);
		processed_records += 2;
	}

	// Read new records from nfdump file (templates + data)
	while (processed_records < max_records_per_packet && !stop) {
		record_header_t *record;
		ret_val = get_next_record(conf, &record);
		if (ret_val <= 0) {
			// Failed to get new record
			break;
		}

		switch (record->type) {
		case CommonRecordV0Type:
		case CommonRecordType:
			// Process data record
			stop = process_ext_record(record, &(conf->ext), &(conf->template_mgr), ipfix_msg);
			++processed_records;
			++processed_data_records;
			break;
		case ExtensionMapType:
			// Process extension map (template)
			stop = process_ext_map(record, &(conf->ext), &(conf->template_mgr), ipfix_msg);
			++processed_records;
			break;
		default:
			// Unsupported record type -> skip
			MSG_DEBUG(msg_module, "Unsupported record type (%u) skipped.", record->type);
			break;
		}
	}

	conf->data_records_sent += processed_data_records;
	*info = (struct input_info *) &(conf->in_info_list->in_info);

	if (ret_val != INPUT_ERROR) {
		*packet = message_to_packet(ipfix_msg, &packet_len);
		if (!(*packet)) {
			MSG_ERROR(msg_module, "Unable to allocate memory (%s:%d)", __FILE__, __LINE__);
			ret_val = INPUT_ERROR;
		}

		if ((*info)->status == SOURCE_STATUS_NEW) {
			(*info)->status = SOURCE_STATUS_OPENED;
			(*info)->odid = ntohl(((struct ipfix_header*) *packet)->observation_domain_id);
		}
		if (ret_val == INPUT_CLOSED && processed_records == 0) {
			(*info)->status = SOURCE_STATUS_CLOSED;
		}

		*source_status = (*info)->status;

	} else {
		*source_status = SOURCE_STATUS_CLOSED;
	}

	// Cleanup
	for (int i = 0; ipfix_msg->data_couple[i].data_set; ++i) {
		free(ipfix_msg->data_couple[i].data_set);
	}
	for (int i = 0; ipfix_msg->templ_set[i]; ++i) {
		free(ipfix_msg->templ_set[i]);
	}
	free(ipfix_msg->pkt_header);
	free(ipfix_msg);

	return (packet_len > IPFIX_HEADER_LENGTH) ? packet_len : ret_val;
}


static int read_header_and_stats(struct nfinput_config *conf)
{
	int read_size;
	
	//read header of nffile
	read_size = read(conf->fd, &(conf->header), sizeof(struct file_header_s));
	if (read_size != sizeof(struct file_header_s)) {
		MSG_ERROR(msg_module, "Can't read file header: %s", conf->input_files[conf->findex - 1]);
		return 1;
	}
	if (conf->header.magic != 0xA50C){
		MSG_DEBUG(msg_module, "Skipping file: %s", conf->input_files[conf->findex - 1]);
		return 1;
	}

	read_size = read(conf->fd, &(conf->stats), sizeof(struct stat_record_s));
	if (read_size != sizeof(struct stat_record_s)) {
		MSG_ERROR(msg_module, "Can't read file statistics: %s", conf->input_files[conf->findex - 1]);
		return 1;
	}

	//template for this record with ipv4
	fill_basic_template(0, &(conf->template_mgr.templates[conf->template_mgr.counter]));
	conf->ext.map[conf->ext.filled].tmp4_index = conf->template_mgr.counter;

	conf->template_mgr.counter++;
	//template for this record with ipv6
	fill_basic_template(1, &(conf->template_mgr.templates[conf->template_mgr.counter]));
	conf->ext.map[conf->ext.filled].id = BASIC_TEMPLATE_ID;
	conf->ext.map[conf->ext.filled].tmp6_index = conf->template_mgr.counter;

	return 0;
}

/**
 * \brief Open input file
 *
 * Open next input file from list of available input files.
 *
 * \param[in] conf  input plugin config structure
 * \return  0 on success, negative value otherwise. In case
 * that there is no more input files to process conf->fd is 
 * set to NO_INPUT_FILE
 */
static int prepare_input_file(struct nfinput_config *conf)
{
	int fd;
	int ret = 0;

	if (conf->input_files[conf->findex] == NULL) {
		/* no more input files, we are done */
		conf->fd = NO_INPUT_FILE;
		return -1;
	}

	MSG_NOTICE(msg_module, "Opening input file: %s", conf->input_files[conf->findex]);
	
	fd = open(conf->input_files[conf->findex], O_RDONLY);
	if (fd == -1) {
		/* input file doesn't exist or we don't have read permission */
		MSG_ERROR(msg_module, "Unable to open input file: %s", conf->input_files[conf->findex]);
		ret = -1;
	}

	/* New file == new input info */
	struct input_info_file_list *info = calloc(1, sizeof(struct input_info_file_list));
	if (!info) {
		MSG_ERROR(msg_module, "Unable to allocate memory (%s:%d)", __FILE__, __LINE__);
		return -1;
	}
	
	info->in_info.name   = conf->input_files[conf->findex];
	info->in_info.type   = SOURCE_TYPE_IPFIX_FILE;
	info->in_info.status = SOURCE_STATUS_NEW;
	
	/* Insert new input info into list */
	info->next = conf->in_info_list;
	conf->in_info_list = info;
	
	conf->findex += 1;
	conf->fd = fd;
	
	ret = read_header_and_stats(conf);
	
	conf->block = 0;
	
	return ret;
}

/**
 * \brief Close input file
 *
 * \param[in] conf  input plugin config structure
 * \return  0 on success, negative value otherwise
 */
static int close_input_file(struct nfinput_config *conf)
{
	int ret;

	if (conf->fd < 0) {
		/* File already closed */
		return 0;
	}

	ret = close(conf->fd);
	if (ret == -1) {
		MSG_ERROR(msg_module, "Error when closing output file");
		return -1;
	}

	MSG_NOTICE(msg_module, "Input file closed");
	conf->fd = -1;
	return 0;
}


/**
 * \brief Prepare new input file
 *
 * Close current input file (if any), and open new one
 *
 * \param[in] conf  input plugin config structure
 * \return  0 on success.
 * NO_INPUT_FILE in case there is no more input files.
 * negative value otherwise.
 */
int next_file(struct nfinput_config *conf)
{
	int ret;

	if (conf->fd >= 0) {
		close_input_file(conf);
	}

	ret = 1;
	while (ret) {
		ret = prepare_input_file(conf);
		if (conf->fd == NO_INPUT_FILE) {
			/* no more input files */
			return NO_INPUT_FILE;
		} else if (!ret) {
			/* ok, new input file ready */
			return ret;
		}
	}

	return NO_INPUT_FILE;
}

/**
 * \brief Init extensions structure
 * 
 * \param conf Plugin configuration
 * \return 0 on success
 */
int init_ext(struct nfinput_config *conf)
{
	conf->ext.filled = 0;
	conf->ext.size = 2;

	//inital space for extension map
	conf->ext.map = (struct extension *) calloc(conf->ext.size, sizeof(struct extension));

	if (conf->ext.map == NULL) {
		MSG_ERROR(msg_module, "Unable to allocate memory (%s:%d)", __FILE__, __LINE__);
		return -1;
	}
	return 0;
}

/**
 * \brief Init template manager
 * 
 * \param conf Plugin configuration
 * \return 0 on success
 */
int init_manager(struct nfinput_config *conf)
{
	
	conf->template_mgr.templates = (struct ipfix_template **) calloc(conf->ext.size, sizeof(struct ipfix_template *));
	if(conf->template_mgr.templates == NULL){
		MSG_ERROR(msg_module, "Unable to allocate memory (%s:%d)", __FILE__, __LINE__);
		return -1;
	}
	
	conf->template_mgr.max_length = conf->ext.size;
	conf->template_mgr.counter = 0;
	return 0;
}

/**
 * \brief Plugin initialization
 *
 * \param[in] params  XML based configuration for this input plugin
 * \param[out] config  input plugin config structure
 * \return 0 on success, negative value otherwise
 */
int input_init(char *params, void **config)
{
	struct nfinput_config *conf;
	
	char **input_files;
	xmlDocPtr doc;
	xmlNodePtr cur;
	int ret;
	int i;

	/* allocate memory for config structure */
	conf = (struct nfinput_config *) calloc(1, sizeof(*conf));
	if (!conf) {
		MSG_ERROR(msg_module, "Unable to allocate memory (%s:%d)", __FILE__, __LINE__);
		return -1;
	}

	/* try to parse configuration file */
	doc = xmlReadMemory(params, strlen(params), "nobase.xml", NULL, 0);
	if (doc == NULL) {
		MSG_ERROR(msg_module, "Plugin configuration not parsed successfully");
		goto err_init;
	}
	cur = xmlDocGetRootElement(doc);
	if (cur == NULL) {
		MSG_ERROR(msg_module, "Empty configuration");
		goto err_xml;
	}
	if (xmlStrcmp(cur->name, (const xmlChar *) "nfdumpReader")) {
		MSG_ERROR(msg_module, "root node != nfdumpReader");
		goto err_init;
	}
	cur = cur->xmlChildrenNode;
	while (cur != NULL) {
		/* find out where to look for input file */
		if ((!xmlStrcmp(cur->name, (const xmlChar *) "file"))) {
			conf->xml_file = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
		}
		break;
	}

	/* check whether we have found "file" element in configuration file */
	if (conf->xml_file == NULL) {
		MSG_ERROR(msg_module, "\"file\" element is missing. No input, "
				"nothing to do");
		goto err_xml;
	}

	/* we only support local files */
	if (strncmp((char *) conf->xml_file, "file:", 5)) {
		MSG_ERROR(msg_module, "element \"file\": invalid URI - "
				"only allowed scheme is \"file:\"");
		goto err_xml;
	}

	/* skip "file:" at the beginning of the URI */
	conf->file = (char *) conf->xml_file + 5;

	/* we don't need this xml tree any more */
	xmlFreeDoc(doc);

	input_files = utils_files_from_path(conf->file);
	
	if (!input_files) {
		goto err_init;
	}
	
	conf->input_files = input_files;
	
	/* print all input files */
	if (input_files[0] != NULL) {
		MSG_NOTICE(msg_module, "List of input files:");
		for (i = 0; input_files[i] != NULL; i++) {
			MSG_NOTICE(msg_module, "\t%s", input_files[i]);
		}
	}

	conf->block_buffer = (char *) malloc(BUFFSIZE);
	if (!conf->block_buffer) {
		MSG_ERROR(msg_module, "Unable to allocate memory (%s:%d)", __FILE__, __LINE__);
		goto err_init;
	}
	conf->block_cur_rec = NULL;
	conf->block_record = 0;
	conf->data_records_sent = 0;
	
	ret = init_ext(conf);
	if (ret) {
		goto err_init;
	}
	
	ret = init_manager(conf);
	if (ret) {
		goto err_init;
	}
	
	/* Prepare first file */
	ret = next_file(conf);
	if (ret < 0) {
		/* no input files */
		MSG_ERROR(msg_module, "No input files, nothing to do");
		goto err_init;
	}
	
	*config = conf;
	return 0;

err_xml:
	xmlFreeDoc(doc);

err_init:
	/* plugin initialization failed */
	free(conf);
	*config = NULL;

	return -1;
}
