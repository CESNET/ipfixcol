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
 * \defgroup ipfixFileFormat Input plugin for IPFIX file format
 * 
 * This is implementation of the input plugin API for IPFIX file format.
 * Currently supported input parameters:
 * path   - path to an IPFIX file. This parameter is mandatory.
 *
 * Sample input string:
 * "/tmp/ipfix.file"
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

#include "ipfixcol.h"

/* IPFIX input plugin specific "config" structure */
struct ipfix_config {
	int fd;                  /* file descriptor */
};


/*
 * Input plugin API implementation
*/ 

int input_init(char *params, void **config)
{
	int fd;
	struct ipfix_config *conf;


	if (!params) {
		/* TODO - log, bad params */
		return -1;
	}

	conf = (struct ipfix_config *) malloc (sizeof(*conf));

	memset(conf, '\0', sizeof(*conf));

	/* open IPFIX file */
	fd = open(params, O_RDONLY);
	if (fd == -1) {
		/* input file doesn't exist or we don't have read permission */
		/* TODO - log, no input, nothing to do */
		goto err_open;
	}

	conf->fd = fd;

	*config = conf;

	return 0;

err_open:
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
		/* TODO - log, out of memory */
		return -1;
	}

	/* read IPFIX header only */
	ret = read(conf->fd, header, sizeof(*header));
	if ((ret == -1) || (ret == 0)) {
		/* error during reading */
		/* TODO - log */
		goto err_header;
	}

	/* check magic number */
	if (ntohs(header->version) != IPFIX_VERSION) {
		/* not an IPFIX file */
		/* TODO - log */
		goto err_header;
	}

	/* get packet length */
	packet_len = ntohs(header->length);
	if (packet_len < sizeof(*header)) {
		/* invalid length of the IPFIX message */
		/* TODO - log */
		goto err_header;
	}
	
	/* allocate memory for whole IPFIX message */
	*packet = (char *) malloc(packet_len);
	if (*packet == NULL) {
		/* TODO - log, out of memory */
		goto err_header;
	}

	memcpy(*packet, header, sizeof(*header));
	counter += sizeof(*header);
	
	ret = read(conf->fd, (*packet)+counter, packet_len-counter);
	if (ret == -1) {
		/* error during reading */
		/* TODO - log */
		goto err_header;
	}
	counter += ret;

	/* input info */
	in_info = (struct input_info *) malloc(sizeof(*in_info));
	if (!info) {
		/* out of memory */
		/* TODO - log */
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
		/* TODO - log warning */
	}

	free(conf);

	return ret;
}

/**@}*/


/* DEBUG - this can be safely ignored or deleted */
#ifdef __DEBUG_INPUT_PLUGIN_IPFIX

#include <stdio.h>
int main(int argc, char **argv)
{
	struct ipfix_config *config;
	int ret;

	ret = input_init(argv[1], (void **) &config);
	printf("input_init() [%d]\n", ret);
	if (ret != 0) {
		return ret;
	}

	printf("%d\n", config->fd);

	int output;
	output = open("output.ipfix", O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (output == -1) {
		return -1;
	}

	char *packet;
	struct input_info *nfo;

	ret = 1;
	while (ret > 0) {
		ret = get_packet((void *) config, &nfo, &packet);
		printf("get_packet() [%d], ", ret);
		if (ret > 0) {
			printf("source %d\n", nfo->type);
			write(output, packet, ret);
			free(packet);
			free(nfo);
		}
	}

	close(output);


	ret = input_close((void **) &config);
	printf("\ninput_close() [%d]\n", ret);

	return 0;
}
#endif
/* DEBUG */

