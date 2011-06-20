/**
 * \file ipfix_format.c
 * \author Michal Srb <michal.srb@cesnet.cz>
 * \brief Input plugin for IPFIX file format.
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
 * \defgroup ipfixInputFileFormat Input plugin for IPFIX file format
 * \ingroup inputPlugins
 *
 * This is implementation of the input plugin API for IPFIX file format.
 *
 * @{
 */

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <commlbr.h>

#include "ipfixcol.h"


/* IPFIX input plugin specific "config" structure */
struct ipfix_config {
	int fd;                  /* file descriptor */
	xmlChar *xml_file;       /* input file URI from XML configuration 
	                          * file */
	char *file;              /* actual path where to store messages */
};


/*
 * Input plugin API implementation
*/

int input_init(char *params, void **config)
{
	int fd;
	struct ipfix_config *conf;

	xmlDocPtr doc;
	xmlNodePtr cur;


	/* allocate memory for config structure */
	conf = (struct ipfix_config *) malloc(sizeof(*conf));
	if (!conf) {
		VERBOSE(CL_VERBOSE_OFF, "not enough memory");
		return -1;
	}
	memset(conf, '\0', sizeof(*conf));


	/* try to parse configuration file */
	doc = xmlReadMemory(params, strlen(params), "nobase.xml", NULL, 0);
	if (doc == NULL) {
		VERBOSE(CL_VERBOSE_OFF, "plugin configuration not parsed successfully");
		goto err_init;
	}
	cur = xmlDocGetRootElement(doc);
	if (cur == NULL) {
		VERBOSE(CL_VERBOSE_OFF, "empty configuration");
		goto err_init;
	}
	if (xmlStrcmp(cur->name, (const xmlChar *) "fileReader")) {
		VERBOSE(CL_VERBOSE_OFF, "root node != fileReader");
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
		VERBOSE(CL_VERBOSE_OFF, "\"file\" element is missing. no input, "
		                        "nothing to do");
		goto err_init;
	}

	/* we only support local files */
	if (strncmp((char *) conf->xml_file, "file:", 5)) {
		VERBOSE(CL_VERBOSE_OFF, "element \"file\": invalid URI - "
		                        "only allowed scheme is \"file:\"");
		goto err_init;
	}

	/* skip "file:" at the beginning of the URI */
	conf->file = (char *) conf->xml_file + 5;


	/* open IPFIX file */
	fd = open(params, O_RDONLY);
	if (fd == -1) {
		/* input file doesn't exist or we don't have read permission */
		VERBOSE(CL_VERBOSE_OFF, "unable to open input file");
		goto err_init;
	}

	conf->fd = fd;

	*config = conf;

	return 0;

err_init:
	/* plugin initialization failed */
	free(conf);
	*config = NULL;

	return -1;
}


int get_packet(void *config, struct input_info** info, char **packet)
{
	int ret;
	int counter = 0;
	struct ipfix_header *header;
	uint16_t packet_len;
	struct ipfix_config *conf;
	struct input_info *in_info;


	conf = (struct ipfix_config *) config;

	header = (struct ipfix_header *) malloc(sizeof(*header));
	if (!header) {
		VERBOSE(CL_VERBOSE_OFF, "not enough memory");
		return -1;
	}

	/* read IPFIX header only */
	ret = read(conf->fd, header, sizeof(*header));
	if ((ret == -1) || (ret == 0)) {
		/* error during reading */
		VERBOSE(CL_VERBOSE_OFF, "error while reading from input file");
		goto err_header;
	}

	/* check magic number */
	if (ntohs(header->version) != IPFIX_VERSION) {
		/* not an IPFIX file */
		VERBOSE(CL_VERBOSE_OFF, "input file is not an IPFIX file");
		goto err_header;
	}

	/* get packet length */
	packet_len = ntohs(header->length);
	if (packet_len < sizeof(*header)) {
		/* invalid length of the IPFIX message */
		VERBOSE(CL_VERBOSE_OFF, "input file has invalid length (too short)");
		goto err_header;
	}

	/* allocate memory for whole IPFIX message */
	*packet = (char *) malloc(packet_len);
	if (*packet == NULL) {
		VERBOSE(CL_VERBOSE_OFF, "not enough memory");
		goto err_header;
	}

	memcpy(*packet, header, sizeof(*header));
	counter += sizeof(*header);

	ret = read(conf->fd, (*packet)+counter, packet_len-counter);
	if (ret == -1) {
		/* error during reading */
		VERBOSE(CL_VERBOSE_OFF, "error while reading from input file");
		goto err_header;
	}
	counter += ret;

	/* input info */
	in_info = (struct input_info *) malloc(sizeof(*in_info));
	if (!info) {
		/* out of memory */
		VERBOSE(CL_VERBOSE_OFF, "not enough memory");
		goto err_info;
	}

	in_info->type = SOURCE_TYPE_IPFIX_FILE;
	*info = in_info;

	free(header);

	return packet_len;

err_info:
	free(*packet);

err_header:
	free(header);
	return -1;
}


int input_close(void **config)
{
	struct ipfix_config *conf = *config;
	int ret;

	ret = close(conf->fd);
	if (ret == -1) {
		VERBOSE(CL_VERBOSE_OFF, "error while closing output file");
	}

	xmlFree(conf->xml_file);
	free(conf);

	return ret;
}

/**@}*/

