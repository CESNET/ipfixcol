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
#include <dirent.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stddef.h>
#include <errno.h>
#include <libgen.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <commlbr.h>

#include "ipfixcol.h"


#define NUMBER_OF_INPUT_FILES 100
#define NO_INPUT_FILE         (-2)


/* IPFIX input plugin specific "config" structure */
struct ipfix_config {
	int fd;                  /* file descriptor */
	xmlChar *xml_file;       /* input file URI from XML configuration 
	                          * file. (e.g.: "file://tmp/ipfix.dump") */
	char *file;              /* path where to look for IPFIX files. same as
	                          * xml_file, but without 'file:' */
	char *dir;               /* directory where to look for ipfix files.
	                          * basically it is dirname(file) */
	char *filename;          /* name of the input file. it may contain asterisk
	                          * (e.g.: "ipfix-2011-03-*.dump) */
	char *file_copy;         /* auxiliary variable, copy of the "file" for purpose 
	                          * of basename() */
	char **input_files;      /* list of all input files */
	int findex;              /* index to the current file in the list of files */
	struct input_info_file *in_info; /* info structure about current input file */
};


/* check whether string matches regexp or not 
 * return 1 if string matches, 0 otherwise */
static int regexp_asterisk(char *regexp, char *string)
{
	static int asterisk = '*';
	char *asterisk_pos;
	char *aux_regexp;
	char *saveptr;
	char *token;
	int ok;             /* 1 if string matches regexp, 0 otherwise */

	if ((regexp == NULL) || (string == NULL)) {
		return 0;
	}

	if ((asterisk_pos = strchr(regexp, asterisk)) == NULL) {
		/* this string doesn't contain asterisk... */
		if (strcmp(regexp, string)) {
			return 1;
		} else {
			return 0;
		}
	}

	/* make copy of original string */
	aux_regexp = (char *) malloc(strlen(regexp) + 1);
	if (aux_regexp == NULL) {
		VERBOSE(CL_VERBOSE_OFF, "Not enough memory");
		return -1;
	}

	strcpy(aux_regexp, regexp);
	
	int pos = 1; /* we assume that asterisk is in the middle of the string */
	if (aux_regexp[0] == asterisk) {
		/* asterisk on the beginning */
		pos = 0;
	}
	if (aux_regexp[strlen(aux_regexp)-1] == asterisk) {
		/* asterisk on the end of the string */
		pos = 2;
	}
	if (!strcmp(aux_regexp, "*")) {
		/* there is nothing but asterisk */
		pos = -1;
	}

	token = strtok_r(aux_regexp, "*", &saveptr);

	ok = 0;
	switch (pos) {
	case (-1):
		/* there is nothing but asterisk, so it matches. best scenario :) */
		ok = 1;
		break;
	case (0):
		/* asterisk is on the beginning of the string */
		if (!strncmp(token, string+(strlen(string)-strlen(token)), strlen(token))) {
			ok = 1;
		}
		break;

	case (1):
		/* asterisk is in the middle of the string */
		if (!strncmp(token, string, strlen(token))) {
			token = strtok_r(NULL, "*", &saveptr);
			if (!strncmp(token, string+(strlen(string)-strlen(token)), strlen(token))) {
				ok = 1;
			}
		}
		break;

	case (2):
		/* asterisk is on the end of the string */
		if (!strncmp(token, string, strlen(token))) {
			ok = 1;
		}
		break;
	}

	return ok;
}


static int prepare_input_file(struct ipfix_config *conf)
{
	int fd;
	int ret = 0;

	if (conf->input_files[conf->findex] == NULL) {
		/* no more input files, we are done */
		conf->fd = NO_INPUT_FILE;
		return -1;
	}

	fd = open(conf->input_files[conf->findex], O_RDONLY);
	if (fd == -1) {
		/* input file doesn't exist or we don't have read permission */
		VERBOSE(CL_VERBOSE_OFF, "Unable to open input file \"%s\"", conf->file);
		ret = -1;
	}

	conf->findex += 1;
	conf->fd = fd;
	
	return ret;
}

static int close_input_file(struct ipfix_config *conf)
{
	int ret;

	ret = close(conf->fd);
	if (ret == -1) {
		VERBOSE(CL_VERBOSE_OFF, "Error when closing output file");
		return -1;
	}

	conf->fd = -1;

	return 0;
}


/* close current input file (if any), and open new one */
static int next_file(struct ipfix_config *conf)
{
	if (conf->fd <= 0) {
		close_input_file(conf);
	}
	prepare_input_file(conf);

	if (conf->fd != NO_INPUT_FILE) {
		
	}

	return conf->fd;
}


/*
 * Input plugin API implementation
*/

int input_init(char *params, void **config)
{
	struct ipfix_config *conf;
	char **input_files;
	xmlDocPtr doc;
	xmlNodePtr cur;
	struct dirent *entry;
	struct dirent *result;
	int ret;
	int fcounter = 0;
	int inputf_index = 0;
	int len;
	DIR *dir;


	/* allocate memory for config structure */
	conf = (struct ipfix_config *) malloc(sizeof(*conf));
	if (!conf) {
		VERBOSE(CL_VERBOSE_OFF, "Not enough memory");
		return -1;
	}
	memset(conf, '\0', sizeof(*conf));


	/* try to parse configuration file */
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
		VERBOSE(CL_VERBOSE_OFF, "\"file\" element is missing. No input, "
		                        "nothing to do");
		goto err_xml;
	}

	/* we only support local files */
	if (strncmp((char *) conf->xml_file, "file:", 5)) {
		VERBOSE(CL_VERBOSE_OFF, "element \"file\": invalid URI - "
		                        "only allowed scheme is \"file:\"");
		goto err_xml;
	}

	/* skip "file:" at the beginning of the URI */
	conf->file = (char *) conf->xml_file + 5;


	/* input info */
	conf->in_info = (struct input_info_file *) malloc(sizeof(*(conf->in_info)));
	if (!conf->in_info) {
		/* out of memory */
		VERBOSE(CL_VERBOSE_OFF, "Not enough memory");
		goto err_file;
	}

	conf->in_info->type = SOURCE_TYPE_IPFIX_FILE;
	conf->in_info->name = conf->file;

	/* we don't need this xml tree any more */
	xmlFreeDoc(doc);

	/* get directory without filename */
	conf->dir = (char *) malloc(strlen(conf->file) + 1);
	if (conf->dir == NULL) {
		VERBOSE(CL_VERBOSE_OFF, "Not enough memory");
		goto err_file;
	}
	strcpy(conf->dir, conf->file);
	conf->dir = dirname(conf->dir);

	/* get filename without directory */
	conf->file_copy = (char *) malloc(strlen(conf->file) + 1);
	if (conf->file_copy == NULL) {
		VERBOSE(CL_VERBOSE_OFF, "Not enough memory");
		goto err_file;
	}
	strcpy(conf->file_copy, conf->file);
	conf->filename = basename(conf->file_copy);

	dir = opendir(conf->dir);
	if (dir == NULL) {
		VERBOSE(CL_VERBOSE_OFF, "Unable to open input file(s)\n");
		goto err_file;
	}

	len = offsetof(struct dirent, d_name) + 
	          pathconf(conf->dir, _PC_NAME_MAX) + 1;

	entry = (struct dirent *) malloc(len);
	if (entry == NULL) {
		VERBOSE(CL_VERBOSE_OFF, "not enough memory");
		goto err_file;
	}

	int array_length = NUMBER_OF_INPUT_FILES;
	input_files = (char **) malloc(array_length * sizeof(char *));
	if (input_files == NULL) {
		VERBOSE(CL_VERBOSE_OFF, "Not enough memory");
		goto err_file;
	}
	memset(input_files, 0, NUMBER_OF_INPUT_FILES);

	do {
		ret = readdir_r(dir, entry, &result);
		if (ret != 0) {
			VERBOSE(CL_VERBOSE_OFF, "Error while reading directory %s\n", conf->dir);
			goto err_file;
		}
		
		if (result == NULL) {
			/* no more files in directory */
			break;
		}

		if ((!strcmp(".", entry->d_name)) || (!strcmp("..", entry->d_name))) {
			continue;
		}

		/* check whether this filename matches given regexp */
		ret = regexp_asterisk(conf->filename, entry->d_name);
		if (ret == 1) {
			/* this file matches */
			if (fcounter >= array_length) {
				input_files = realloc(input_files, array_length * 2);
				if (input_files == NULL) {
					VERBOSE(CL_VERBOSE_OFF, "Not enough memory");
					goto err_file;
				}
				array_length *= 2;
			}

			input_files[inputf_index] = (char *) malloc(strlen(entry->d_name) + strlen(conf->dir) + 2); /* 2 because of "/" and NULL*/
			if (input_files[inputf_index] == NULL) {
				VERBOSE(CL_VERBOSE_OFF, "Not enough memory");
				goto err_file;
			}
			sprintf(input_files[inputf_index], "%s/%s", conf->dir, entry->d_name);
			inputf_index += 1;
		}
	} while (result);

	conf->input_files = input_files;


	ret = prepare_input_file(conf);
	if (ret == -1) {
		/* no input files */
		goto err_file;
	}

	*config = conf;

	return 0;


err_file:
	xmlFree(conf->xml_file);

err_xml:
	xmlFreeDoc(doc);

err_init:
	/* plugin initialization failed */
	free(conf);
	*config = NULL;

	return -1;
}


int get_packet(void *config, struct input_info **info, char **packet)
{
	int ret;
	int counter = 0;
	struct ipfix_header *header;
	uint16_t packet_len;
	struct ipfix_config *conf;

	conf = (struct ipfix_config *) config;

	*info = (struct input_info *) conf->in_info;

	header = (struct ipfix_header *) malloc(sizeof(*header));
	if (!header) {
		VERBOSE(CL_VERBOSE_OFF, "Not enough memory");
		return -1;
	}

	/* read IPFIX header only */
read_header:
	ret = read(conf->fd, header, sizeof(*header));
	if (ret == -1) {
		if (errno == EINTR) {
			ret = INPUT_INTR;
			goto err_header;
		}
	    VERBOSE(CL_VERBOSE_OFF, "Failed to receive IPFIX packet header: %s", strerror(errno));
	    ret = INPUT_ERROR;
		goto err_header;
	}
	if (ret == 0) {
		/* EOF, next file? */
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
		VERBOSE(CL_VERBOSE_OFF, "Input file is not an IPFIX file");
		goto err_header;
	}

	/* get packet length */
	packet_len = ntohs(header->length);
	if (packet_len < sizeof(*header)) {
		/* invalid length of the IPFIX message */
		VERBOSE(CL_VERBOSE_OFF, "Input file has invalid length (too short)");
		goto err_header;
	}

	/* allocate memory for whole IPFIX message */
	*packet = (char *) malloc(packet_len);
	if (*packet == NULL) {
		VERBOSE(CL_VERBOSE_OFF, "Not enough memory");
		goto err_header;
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
	    VERBOSE(CL_VERBOSE_OFF, "Error while reading from input file: %s", strerror(errno));
	    ret = INPUT_ERROR;
		goto err_info;
	}
	if (ret == 0) {
		/* EOF, next file? */
		ret = next_file(conf);
		if (ret == NO_INPUT_FILE) {
			/* all files processed */
			ret = INPUT_CLOSED;
			goto err_info;
		}

		/* TODO fix possible memory leaks */
		goto read_header;
	}
	counter += ret;

	free(header);

	return packet_len;

err_info:
	free(*packet);
	*packet = NULL;

err_header:
	free(header);
	return ret;
}


int input_close(void **config)
{
	struct ipfix_config *conf = *config;
	int ret = 0;
	int i;

	free(conf->dir);
	free(conf->file_copy);

	/* free list of input files */
	if (conf->input_files) {
		for (i = 0; conf->input_files[i]; i++) {
			free(conf->input_files[i]);
		}
		free(conf->input_files);
	}

	xmlFree(conf->xml_file);
	free(conf->in_info);
	free(conf);

	return ret;
}

/**@}*/

