/**
 * \file ipfix_format.c
 * \author Michal Srb <michal.srb@cesnet.cz>
 * \brief Storage plugin for IPFIX file format.
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
 * \defgroup ipfixFileFormat Storage plugin for IPFIX file format
 * \ingroup storagePlugins
 *
 * This is implementation of the storage plugin API for IPFIX file format.
 *
 * @{
 */

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <libgen.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

#include "commlbr.h"
#include "ipfixcol.h"


/* IPFIX storage plugin specific "config" structure */
struct ipfix_config {
	int fd;                     /* file descriptor of an output file */
	uint32_t fcounter;          /* number of created files */
	uint64_t bcounter;          /* bytes written into a current output
	                             * file */
	xmlChar *xml_file;          /* URI from XML configuration file */
	char *file;                 /* actual path where to store messages */
};


/* prepare (open, create) output file  */
static int prepare_output_file(struct ipfix_config *config)
{
	int fd;

	/* file counter */
	config->fcounter += 1;

	fd = open(config->file, O_WRONLY | O_CREAT | O_TRUNC,
	          S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	if (fd == -1) {
		config->fcounter -= 1;
		VERBOSE(CL_VERBOSE_OFF, "unable to open output file");
		return -1;
	}

	config->fd = fd;

	return 0;
}

/* close output file */
static int close_output_file(struct ipfix_config *config)
{
	int ret;

	ret = close(config->fd);
	if (ret == -1) {
		VERBOSE(CL_VERBOSE_OFF, "error when closing output file");
		return -1;
	}

	config->fd = -1;
	config->bcounter = 0;

	return 0;
}




/*
 * Storage Plugin API implementation
*/


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
	struct ipfix_config *conf;

	xmlDocPtr doc;
	xmlNodePtr cur;

 	/* allocate space for config structure */
	conf = (struct ipfix_config *) malloc(sizeof(*conf));
	if (conf == NULL) {
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
	if (xmlStrcmp(cur->name, (const xmlChar *) "fileWriter")) {
		VERBOSE(CL_VERBOSE_OFF, "root node != fileWriter");
		goto err_init;
	}
	cur = cur->xmlChildrenNode;
	while (cur != NULL) {
		/* find out where to store output files */
		if ((!xmlStrcmp(cur->name, (const xmlChar *) "file"))) {
			conf->xml_file = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
		}
		break;
	}

	/* check whether we have found "file" element in configuration file */
	if (conf->xml_file == NULL) {
		VERBOSE(CL_VERBOSE_OFF, "configuration file doesn't specify where "
		                        "to store output files (\"file\" element "
								"is missing)");
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

	prepare_output_file(conf);

	*config = conf;

	return 0;


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
                 const struct ipfix_template_t *templates)
{
	ssize_t count = 0;
	uint16_t wbytes = 0;
	struct ipfix_config *conf;
	conf = (struct ipfix_config *) config;


	/* write IPFIX message into an output file */
	while (count < ntohs(ipfix_msg->pkt_header->length)) {
		count = write(conf->fd, (ipfix_msg->pkt_header)+wbytes,
		              ntohs(ipfix_msg->pkt_header->length)-wbytes);
		if (count == -1) {
			if (errno == EINTR) {
				/* interrupted by signal, try again */
				break;
			} else {
				/* serious error occurs */
				VERBOSE(CL_VERBOSE_OFF, "error while writing into the "
				                        "output file");
				return -1;
			}
		} else {
			wbytes += count;
		}
	}

	conf->bcounter += wbytes;

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
	/* FIXME - is this enough? */
	struct ipfix_config *conf;
	conf = (struct ipfix_config *) config;

	fsync(conf->fd);

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
	struct ipfix_config *conf;
	conf = (struct ipfix_config *) *config;

	close_output_file(conf);

	if (conf->bcounter == 0) {
		/* current output file is empty, get rid of it */
		unlink(conf->file);
	}

	xmlFree(conf->xml_file);
	free(conf);

	*config = NULL;

	return 0;
}

/**@}*/

