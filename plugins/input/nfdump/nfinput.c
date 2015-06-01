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

#include "nffile.h"
#include "ext_parse.h"
#include "ext_fill.h"

/* API version constant */
IPFIXCOL_API_VERSION;

static const char *msg_module = "nfdump input";

#define NO_INPUT_FILE (-2)
#define EXT_PARSE_CNT 26
#define EXT_FILL_CNT  26

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
	xmlChar *xml_file;       /**< input file URI from XML configuration 
	                          * file. (e.g.: "file://tmp/ipfix.dump") */
	char *file;              /**< path where to look for IPFIX files. same as
	                          * xml_file, but without 'file:' */
	char **input_files;      /**< list of all input files */
	int findex;              /**< index to the current file in the list of files */
	struct input_info_file_list	*in_info_list;
	struct input_info_file *in_info; /**< info structure about current input file */
	struct extensions ext;           /**< extensions map */
	struct ipfix_template_mgr_record template_mgr; /**< template manager */
	struct file_header_s header;     /**< header of readed file */
	struct stat_record_s stats;      /**< stats record */
	int basic_added;                 /**< flag indicating if basic templates was added */
	int block;                       /**< block number in current file */
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
static void (*ext_parse[EXT_PARSE_CNT]) (uint32_t *data, int *offset, uint8_t flags, struct ipfix_data_set *data_set) = {
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
		ext25_parse
};

/**
 * \brief Functions for filling templates by extensions
 */
static void (*ext_fill_tm[EXT_FILL_CNT]) (uint8_t flags, struct ipfix_template * template) = {
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
		ext25_fill_tm
};

#define HEADER_ELEMENTS 8
static int header_elements[][2] = {
		//id,size
		{89,4},  //fwd_status
		{152,8}, //flowEndSysUpTime MILLISECONDS !
		{153,8}, //flowStartSysUpTime MILLISECONDS !
		{6,2},  //tcpControlBits flags
		{4,1},  //protocolIdentifier
		{5,1},  //ipClassOfService
		{7,2},  //sourceTransportPort
		{11,2} //destinationTransportPort
};

#define ALLOC_FIELDS_SIZE 60

/**
 * \brief Fill in data record with basic data common for block
 * 
 * \param data_set New data set
 * \param record nfdump record
 */
void fill_basic_data(struct ipfix_data_set *data_set, struct common_record_v0_s *record){

	data_set->records[data_set->header.length] = record->fwd_status;
	data_set->header.length += 4;
	*((uint64_t *) &(data_set->records[data_set->header.length])) = htobe64((uint64_t)record->first*1000+record->msec_first); //sec 2 msec
	data_set->header.length += 8;
	*((uint64_t *) &(data_set->records[data_set->header.length])) = htobe64((uint64_t)record->last*1000+record->msec_last); //sec 2 msec
	data_set->header.length += 8;
	data_set->records[data_set->header.length+1] = record->tcp_flags;
	data_set->header.length += 2;
	data_set->records[data_set->header.length] =record->prot;
	data_set->header.length += 1;
	data_set->records[data_set->header.length] =record->tos;
	data_set->header.length += 1;
	*((uint16_t *) &(data_set->records[data_set->header.length])) = htons(record->srcport);
	data_set->header.length += 2;
	*((uint16_t *) &(data_set->records[data_set->header.length])) = htons(record->dstport);
	data_set->header.length += 2;

}

/**
 * \brief Fill in IPFIX template with basic data common for each block
 * 
 * \param flags some flags
 * \param template new IPFIX template
 */
void fill_basic_template(uint8_t flags, struct ipfix_template **template){
	static int template_id_counter = 256;

	(*template) = (struct ipfix_template *) calloc(1, sizeof(struct ipfix_template) + \
			ALLOC_FIELDS_SIZE * sizeof(template_ie));
	
	if(*template == NULL){
		MSG_ERROR(msg_module, "Malloc faild to get space for ipfix template");
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
	for(i=0;i<HEADER_ELEMENTS;i++){	
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
	for (i = 0; i < MSG_MAX_TEMPLATES; i++) {
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
int process_ext_record(struct common_record_v0_s * record, struct extensions *ext, 
	struct ipfix_template_mgr_record *template_mgr, struct ipfix_message *msg)
{
	int data_offset = 0;
	int id,eid;
	unsigned j;

	id = -1;
	//check id -> most extensions should be on its index
	if(ext->filled > record->ext_map && ext->map[record->ext_map].id == record->ext_map) {
		id = record->ext_map;
	} else { //index does NOT match map id.. we have to find it
		for (j = 0; j < ext->filled + 1; j++) {
			if (ext->map[j].id == record->ext_map) {
				id = j;
			}
		}
	}

	struct ipfix_template *tmp = NULL;
	struct ipfix_data_set *set = NULL;
	
	if (TestFlag(record->flags, FLAG_IPV6_ADDR)) {
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
	ext_parse[1](record->data, &data_offset, record->flags, set);
	ext_parse[2](record->data, &data_offset, record->flags, set);
	ext_parse[3](record->data, &data_offset, record->flags, set);

	for(eid = 0; eid < ext->map[id].values_count; eid++) {
		ext_parse[ext->map[id].value[eid]](record->data, &data_offset, record->flags, set);
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
int process_ext_map(struct extension_map_s * extension_map, struct extensions *ext,
		struct ipfix_template_mgr_record *template_mgr, struct ipfix_message *msg)
{
	struct ipfix_template *template1;
	struct ipfix_template *template2;

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

	int eid = 0;
	for (eid = 0; eid < extension_map->size/2; eid++) { // extension id is 2 byte
		if (extension_map->ex_id[eid] == 0) {
			break;
		}
		ext->map[ext->filled].value[eid] = extension_map->ex_id[eid];
		ext->map[ext->filled].values_count++;
		ext_fill_tm[extension_map->ex_id[eid]] (0, template1);
		ext_fill_tm[extension_map->ex_id[eid]] (1, template2);
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
	int i;
	
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
	struct nfinput_config *conf = (struct nfinput_config *) config;
	unsigned int size = 0;
	char *buffer = NULL, *buffer_start = NULL;;
	uint buffer_size = 0;
	
	struct data_block_header_s block_header;
	struct common_record_v0_s *record;
	int stop = 0, ret, len;

	*info = (struct input_info *) &(conf->in_info_list->in_info);
	
	struct ipfix_message *msg;
	
read_begin:
	if (conf->block >= conf->header.NumBlocks) {
		ret = next_file(conf);
		if (ret == NO_INPUT_FILE) {
			/* all files processed */
			ret = INPUT_CLOSED;
			goto err_header;
		}
		
		goto read_begin;
	}
	
	ret = read(conf->fd, &block_header, sizeof(struct data_block_header_s));
	if (ret == -1) {
		if (errno == EINTR) {
			ret = INPUT_INTR;
			goto err_header;
		}
		MSG_ERROR(msg_module, "Failed to read header: %s", strerror(errno));
	    ret = INPUT_ERROR;
		goto err_header;
	}
	if (ret == 0) {
		/* EOF, next file? */
		*source_status = SOURCE_STATUS_CLOSED;
		ret = next_file(conf);
		if (ret == NO_INPUT_FILE) {
			/* all files processed */
			ret = INPUT_CLOSED;
			goto err_header;
		}
		/* next file is ready */
		goto read_begin;
	}

	if (buffer_start) {
		free(buffer_start);
		buffer_start = NULL;
	}
	
	buffer_start = (char *) malloc(block_header.size);
	if (buffer_start == NULL) {
		MSG_ERROR(msg_module, "Can't allocate memory for record data");
		goto err_header;
	}
	
	buffer_size = block_header.size;
	buffer = buffer_start;
	
	ret = read(conf->fd, buffer, block_header.size);
	if (ret == -1) {
		if (errno == EINTR) {
			ret = INPUT_INTR;
			goto err_header;
		}
		MSG_ERROR(msg_module, "Failed to read block: %s", strerror(errno));
	    ret = INPUT_ERROR;
		goto err_header;
	}
	if (ret == 0) {
		/* EOF, next file? */
		*source_status = SOURCE_STATUS_CLOSED;
		ret = next_file(conf);
		if (ret == NO_INPUT_FILE) {
			/* all files processed */
			ret = INPUT_CLOSED;
			goto err_header;
		}
		/* next file is ready */
		goto read_begin;
	}

	size = 0;
	//read block
	
	msg = calloc(1, sizeof(struct ipfix_message));
	if (!msg) {
		MSG_ERROR(msg_module, "Not enough memory");
		return INPUT_ERROR;
	}
	
	init_ipfix_msg(msg);
	
	if (!conf->basic_added) {
		conf->basic_added = 1;
		add_template(msg, conf->template_mgr.templates[0]);
		add_template(msg, conf->template_mgr.templates[1]);
	}
	
	while (size < block_header.size && !stop) {
		record = (struct common_record_v0_s *) buffer;
		switch (record->type) {
		case CommonRecordV0Type:
			stop = process_ext_record(record, &(conf->ext), &(conf->template_mgr), msg);
			break;
		case ExtensionMapType:
			stop = process_ext_map((struct extension_map_s *) buffer, &(conf->ext), &(conf->template_mgr), msg);
			break;
		default:
			break;
		}

		size += record->size;
		
		if (size >= block_header.size) {
			break;
		}
		
		buffer += record->size;
	}
	
	*packet = message_to_packet(msg, &len);
	
	conf->block++;
	
	*info = (struct input_info *) &(conf->in_info_list->in_info);
	
	/* Set source status */
	*source_status = (*info)->status;
	if ((*info)->status == SOURCE_STATUS_NEW) {
		(*info)->status = SOURCE_STATUS_OPENED;
		(*info)->odid = ntohl(((struct ipfix_header *) *packet)->observation_domain_id);
	}

	int i;
	for (i = 0; msg->data_couple[i].data_set; ++i) {
		free(msg->data_couple[i].data_set);
	}
	for (i = 0; msg->templ_set[i]; ++i) {
		free(msg->templ_set[i]);
	}
	
	free(msg->pkt_header);
	free(msg);
	free(buffer_start);
	
	return len;
	
err_header:
	if (msg) {
		free(msg);
	}
	if (buffer_start) {
		free(buffer_start);
	}
	return ret;
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
	conf->ext.map[conf->ext.filled].id = 0;
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

	if (conf->fd <= 0) {
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
