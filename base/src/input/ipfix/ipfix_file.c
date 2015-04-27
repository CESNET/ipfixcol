/**
 * \file input/ipfix/ipfix_file.c
 * \author Michal Srb <michal.srb@cesnet.cz>
 * \brief Input plugin for IPFIX file format.
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

/**
 * \defgroup ipfixInputFileFormat Input plugin for IPFIX file format
 * \ingroup inputPlugins
 *
 * This is implementation of the input plugin API for IPFIX file format.
 *
 * @{
 */

#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <errno.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

#include "ipfixcol.h"

/* API version constant */
IPFIXCOL_API_VERSION;

#define NO_INPUT_FILE         (-2)

/** Identifier to MSG_* macros */
static char *msg_module = "ipfix input";

struct input_info_file_list {
	struct input_info_file in_info;
	struct input_info_file_list	*next;
};

/**
 * \struct ipfix_config
 * \brief  IPFIX input plugin specific "config" structure 
 */
struct ipfix_config {
	int fd;                  /**< file descriptor */
	xmlChar *xml_file;       /**< input file URI from XML configuration file. (e.g.: "file://tmp/ipfix.dump") */
	char *file;              /**< path where to look for IPFIX files. Same as xml_file, but without 'file:' */
	char **input_files;      /**< list of all input files */
	int findex;              /**< index to the current file in the list of files */
	struct input_info_file_list	*in_info_list;
	struct input_info_file *in_info; /**< info structure about current input file */
};

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
static int prepare_input_file(struct ipfix_config *conf)
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
		return -1;
	}

	/* New file == new input info */
	struct input_info_file_list *info = calloc(1, sizeof(struct input_info_file_list));
	if (!info) {
		MSG_ERROR(msg_module, "Unable to allocate memory (%s:%d)", __FILE__, __LINE__);
		close(fd);
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
	
	return ret;
}

/**
 * \brief Close input file
 *
 * \param[in] conf  input plugin config structure
 * \return  0 on success, negative value otherwise
 */
static int close_input_file(struct ipfix_config *conf)
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
static int next_file(struct ipfix_config *conf)
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
		} else if (ret == 0) {
			/* ok, new input file ready */
			break;
		} else {
			// Do nothing
		}
	}

	return ret;
}

/*
 * * * * Input plugin API implementation
*/

/**
 * \brief Plugin initialization
 *
 * \param[in] params  XML based configuration for this input plugin
 * \param[out] config  input plugin config structure
 * \return 0 on success, negative value otherwise
 */
int input_init(char *params, void **config)
{
	struct ipfix_config *conf;
	char **input_files;
	xmlDocPtr doc;
	xmlNodePtr cur;
	int ret;
	int i;

	/* allocate memory for config structure */
	conf = (struct ipfix_config *) calloc(1, sizeof(*conf));
	if (!conf) {
		MSG_ERROR(msg_module, "Not enough memory");
		return -1;
	}
	memset(conf, '\0', sizeof(*conf));

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
	if (xmlStrcmp(cur->name, (const xmlChar *) "fileReader")) {
		MSG_ERROR(msg_module, "root node != fileReader");
		goto err_init;
	}

	cur = cur->xmlChildrenNode;
	while (cur != NULL) {
		/* find out where to look for input file */
		if ((!xmlStrcmp(cur->name, (const xmlChar *) "file"))) {
			conf->xml_file = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			break;
		}

		cur = cur->next;
	}

	/* check whether we have found "file" element in configuration file */
	if (conf->xml_file == NULL) {
		MSG_ERROR(msg_module, "\"file\" element is missing. No input files; nothing to do");
		goto err_xml;
	}

	/* we only support local files */
	if (strncmp((char *) conf->xml_file, "file:", 5)) {
		MSG_ERROR(msg_module, "element \"file\": invalid URI - only allowed scheme is \"file:\"");
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
	
	ret = next_file(conf);
	if (ret < 0) {
		/* no input files */
		MSG_ERROR(msg_module, "No input files; nothing to do");
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

/**
 * \brief Read IPFIX message from file
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
	int ret;
	int counter = 0;
	struct ipfix_header *header;
	uint16_t packet_len;
	struct ipfix_config *conf;
	char *packet_orig;	

	conf = (struct ipfix_config *) config;
	
	*info = (struct input_info *) &(conf->in_info_list->in_info);
	
	packet_orig = *packet;
	
	header = (struct ipfix_header *) malloc(sizeof(*header));
	if (!header) {
		MSG_ERROR(msg_module, "Not enough memory");
		return INPUT_ERROR;
	}

	/* read IPFIX header only */
read_header:
	ret = read(conf->fd, header, sizeof(*header));
	if (ret == -1) {
		if (errno == EINTR) {
			ret = INPUT_INTR;
			goto err_header;
		}
		MSG_ERROR(msg_module, "Failed to read IPFIX packet header: %s", strerror(errno));
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
		goto read_header;
	}

	/* check magic number */
	if (ntohs(header->version) != IPFIX_VERSION) {
		/* not an IPFIX file */
		MSG_ERROR(msg_module, "Bad magic number. Expected %x, got %x", IPFIX_VERSION, ntohs(header->version));
		/* we don't know how big is this message. It's not IPFIX message or
		 * header is corrupted. skip whole file */
		MSG_ERROR(msg_module, "Input file may be corrupted. Skipping");

		/* try to open next input file */
		*source_status = SOURCE_STATUS_CLOSED;
		ret = next_file(conf);
		if (ret == NO_INPUT_FILE) {
			/* all files processed */
			ret = INPUT_CLOSED;
			goto err_info;
		}

		goto read_header;
	}

	/* get packet length */
	packet_len = ntohs(header->length);
	if (packet_len < sizeof(*header)) {
		/* invalid length of the IPFIX message */
		MSG_ERROR(msg_module, "Input file has invalid length (too short)");
		goto err_header;
	}

	if (*packet == NULL) {
		/* allocate memory for whole IPFIX message */
		*packet = (char *) malloc(packet_len);
		if (*packet == NULL) {
			MSG_ERROR(msg_module, "Not enough memory");
			goto err_header;
		}
	}

	memcpy(*packet, header, sizeof(*header));
	counter += sizeof(*header);

	/* get rest of the packet */
	ret = read(conf->fd, (*packet)+counter, packet_len-counter);
	if (ret == -1) {
		if (errno == EINTR) {
			ret = INPUT_INTR;
			goto err_info;
		}
		MSG_ERROR(msg_module, "Error while reading from input file: %s", strerror(errno));
		ret = INPUT_ERROR;
		goto err_info;
	}
	if (ret == 0 && packet_len-counter > 0) {
		/* EOF, next file? */
		*source_status = SOURCE_STATUS_CLOSED;
		ret = next_file(conf);
		if (ret == NO_INPUT_FILE) {
			/* all files processed */
			ret = INPUT_CLOSED;
			goto err_info;
		}
		if (packet_orig != *packet) {
			/* this plugin allocated this memory */
			free(*packet);
			*packet = NULL;
		}

		goto read_header;
	}
	counter += ret;

	free(header);
	
	*info = (struct input_info *) &(conf->in_info_list->in_info);
	
	/* Set source status */
	*source_status = (*info)->status;
	if ((*info)->status == SOURCE_STATUS_NEW) {
		(*info)->status = SOURCE_STATUS_OPENED;
		(*info)->odid = ntohl(((struct ipfix_header *) *packet)->observation_domain_id);
	}

	return packet_len;
	
err_info:
	if (packet_orig != *packet) {
		/* this plugin allocated this memory */
		free(*packet);
		*packet = NULL;
	}

err_header:
	free(header);
	return ret;
}

/**
 * \brief Clean up
 *
 * \param[in] config  configuration structure
 * \return 0 on success, negative value otherwise
 */
int input_close(void **config)
{
	struct ipfix_config *conf = *config;
	struct input_info_file_list *aux_list = conf->in_info_list;
	int ret = 0;
	int i;

	/* free list of input files */
	if (conf->input_files) {
		for (i = 0; conf->input_files[i]; i++) {
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
	free(conf);

	return ret;
}

/**@}*/
