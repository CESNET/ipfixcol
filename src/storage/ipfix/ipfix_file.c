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
 * 
 * This is implementation of the storage plugin API for IPFIX file format.
 * Currently supported input parameters are:
 * path     - where to store output files.
 * filesize - maximum size of an output file (in bytes).
 *
 * Sample input string:
 * "-path=/tmp/file -filesize=10000"
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
#include "ipfixcol.h"


#define OUTPUT_PATH_MAX_LENGTH(dir) pathconf((dir), _PC_PATH_MAX)


/* IPFIX storage plugin specific "config" structure */
struct ipfix_config {
	int fd;                     /* file descriptor of an output file */
	char *filename;             /* name of an output file */
	char *dir;                  /* absolute or relative path where to 
	                             * store output file */
	char prefix[32];            /* prefix for an output file */
	char *output_path;          /* actual path to an output file 
	                             * (dir + prefix + filename) */
	uint64_t filesize;          /* maximum size of an output file */
	uint32_t fcounter;          /* number of created files */
	uint64_t bcounter;          /* bytes written into a current output 
	                             * file */
};

/* 
 * List of valid parameters. These params can be used in input string.
 * Sample input string: "-path=/tmp/file -filesize=1000000"
 * All parameters are optional.
*/
static char *valid_params[] = {
	"path",       /** path where to store files */
	"filesize",   /** maximum size of an output file in bytes */
};
#define	NUMBER_OF_VALID_PARAMS 2


/* auxiliary function used during parsing of input parameters */
static int __select_param(char *string)
{
	int i;

	for (i = 0; i < NUMBER_OF_VALID_PARAMS; i++) {
		if (!strcmp(string, valid_params[i])) {
			return i;
		}
	}

	return -1;
}

/* create prefix for output file */
static char * __create_prefix(struct ipfix_config *config)
{
	snprintf(config->prefix, sizeof(config->prefix)-1, "%05u", 
	         config->fcounter);
	
	return config->prefix;
}

/* prepare (open, create) output file  */
static int __prepare_output_file(struct ipfix_config *config)
{
	int fd;

	/* file counter */
	config->fcounter += 1;

	snprintf(config->output_path, OUTPUT_PATH_MAX_LENGTH(config->dir)-1,
                 "%s/%s-%s", config->dir, __create_prefix(config), 
                 config->filename);

	/* open output file */
	fd = open(config->output_path, O_WRONLY | O_CREAT | O_TRUNC, 
	          S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	if (fd == -1) {
		/* TODO - log */
		config->fcounter -= 1;
		return -1;
	}
	
	config->fd = fd;
	
	return 0;
}

/* close output file */
static int __close_output_file(struct ipfix_config *config)
{
	int ret;

	ret = close(config->fd);
	if (ret == -1) {
		/* TODO - log */
		return -1;
	}
	
	config->fd = -1;
	config->bcounter = 0;
	
	return 0;
}

/* trim whitespaces from the right side of the string */
static void __rstrtrim(char *str)
{
	int len = strlen(str);
	char *end = str+(len-1);
	
	for ( ; end > str; end--) {
		if (!isspace(*end)) {
			break;
		}
	}
	*(end+1) = '\0';
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
	char *saveptr1;
	char *saveptr2;
	char *token;
	char *subtoken;
	int ret;
	
	/* config structure */
	conf = (struct ipfix_config *) malloc(sizeof(*conf));
	if (conf == NULL) {
		/* TODO - log */
		return -1;
	}
	memset(conf, '\0', sizeof(*conf));


	/* default values */
	conf->dir = ".";
	conf->filename = "output";
	conf->filesize = 100*65536; /* 65536 is maximum size of an IPFIX
	                             * message. TODO - macro in some header
	                             * file */

	/* parse parameters from input string */
	if (params) {
		while (1) {
			token = strtok_r(params, "-", &saveptr1);
			if (token == NULL) {
				break;
			}
	
			subtoken = strtok_r(token, "=", &saveptr2);
	
			switch (__select_param(subtoken)) {
			case 0:
				/* path */
				subtoken = strtok_r(NULL, "=", &saveptr2);
	
				__rstrtrim(subtoken);
	
				conf->dir = dirname(subtoken);
				conf->filename = basename(subtoken);
				break;
			case 1:
				/* maximum file size */
				subtoken = strtok_r(NULL, "=", &saveptr2);
				conf->filesize = atoi(subtoken);
				break;
			default:
				/* unknown parameter */
				/* TODO - log */
				return -1;
			}
			params = NULL;
		}
	}

	
	conf->output_path = (char *) malloc(OUTPUT_PATH_MAX_LENGTH(conf->dir));
	
	ret = __prepare_output_file(conf);
	if (ret < 0) {
		/* TODO - log */
		return -1;
	}

	*config = conf;

	return 0;
}

/**
 * \brief Store received IPFIX message into a file.
 *
 * Store one IPFIX message into a output file.
 *
 * \param[in] config the plugin specific configuration structure
 * \param[in] ipfix_msg IPFIX message
 * \param[in] templates All currently known templates, not just templates in the message
 * \return 0 on success, negative value otherwise
 */
int store_packet(void *config, struct ipfix_message_t *ipfix_msg, struct ipfix_template_t *templates)
{
	ssize_t count = 0;
	uint16_t wbytes = 0;
	struct ipfix_config *conf;
	conf = (struct ipfix_config *) config;

	/* check whether there is a free space for the packet in current 
 	 * output file */
	if (ntohs(ipfix_msg->msg_header->length) + conf->bcounter 
	    > conf->filesize) {
		if (conf->filesize < ntohs(ipfix_msg->msg_header->length)) {
			/* this packet is bigger than our filesize limit */
			/* TODO - log */
			return -1;
		}

		/* there is not enough space in this file, prepare new one */
		__close_output_file(conf);
		__prepare_output_file(conf);
	}

	/* write IPFIX message into an output file */
	while (count < ntohs(ipfix_msg->msg_header->length)) {
		count = write(conf->fd, (ipfix_msg->msg_header)+wbytes, 
		              ntohs(ipfix_msg->msg_header->length)-wbytes);
		if (count == -1) {
			if (errno == EINTR) {
				/* interrupted by signal, try again */
				break;
			} else {
				/* serious error occurs */
				/* TODO - log */
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
 * Just close current output file, no matter how big it is, and prepare new one.
 *
 * \param[in] config the plugin specific configuration structure
 * \return 0 on success, negative value otherwise
 */
int store_now(const void *config)
{
	struct ipfix_config *conf;
	conf = (struct ipfix_config *) config;

	__close_output_file(conf);

	__prepare_output_file(conf);

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
int storage_remove(void *config)
{
	struct ipfix_config *conf;
	conf = (struct ipfix_config *) config;

	__close_output_file(conf);

	if (conf->bcounter == 0) {
		/* current output file is empty, get rid of it */
		unlink(conf->output_path);	
	}

	free(conf->output_path);
	free(conf);

	return 0;
}

/**@}*/




#ifdef	__DEBUG_STORAGE_PLUGIN_IPFIX
/* debug and self test section, it can be safely ignored or deleted */

#define __PRINT_CONFIG_STRUCT(conf) do {                       \
	printf("\tfd: %d\n", conf->fd);                        \
	printf("\tfilename: \"%s\"\n", conf->filename);        \
	printf("\tdir: \"%s\"\n", conf->dir);                  \
	printf("\tprefix: \"%s\"\n", conf->prefix);            \
	printf("\toutput_path: \"%s\"\n", conf->output_path);  \
	printf("\tfilesize: %lu\n", conf->filesize);           \
	printf("\tfcounter: %u\n", conf->fcounter);            \
	printf("\tbcounter: %lu\n", conf->bcounter);           \
	} while (0);

#define __TEST_BIG_BUFFER	1000000

static int __load_packet_from_file(char *path, char *buf, size_t size)
{
	int fd_packet;
	int ret;
	struct stat st;

	fd_packet = open(path, O_RDONLY);
	if (fd_packet == -1) {
		perror("open");
		return -1;
	}

	fstat(fd_packet, &st);

	if ((unsigned long) st.st_size > size) {
		fprintf(stderr, "buffer is too small\n");
		return -2;
	}

	ret = read(fd_packet, buf, st.st_size);
	if (ret != st.st_size) {
		printf("WARNING: packet is crippled\n");
	}

	close(fd_packet);
	
	return st.st_size;
}

int main(int argc, char **argv)
{
	struct ipfix_config *conf;
	int ret;
	struct ipfix_message_t msg;
	char *buf;
	char *params = "";

	if (argc < 2) {
		printf("Usage: %s path_to_sample_ipfix_file input_string"
	                "\n", argv[0]);
		return -1;
	}

	if (argc >= 3) {
		params = argv[2];
	}

	printf("\ninput string: \"%s\"\n\n", params);

	/* stoage_init() */
	printf("--- init\n");

	printf("calling storage_init()...");
	ret = storage_init(params, (void **) &conf);
	printf(" done [%d]\n", ret);

	printf("config structure:\n");
	__PRINT_CONFIG_STRUCT(conf);

	/* store_packet() */
	printf("\n--- store\n");

	buf = (char *) malloc (__TEST_BIG_BUFFER);
	if (!buf) {
		fprintf(stderr, "not enough memory\n");
		exit(-1);
	}

	ret = __load_packet_from_file(argv[1], buf, __TEST_BIG_BUFFER);
	if (ret <= 0) {
		return -1;
	}

	msg.msg_header = (struct ipfix_header_t *) buf;

	printf("calling storage_packet()...");
	ret = store_packet(conf, &msg, NULL);
	printf(" done [%d]\n", ret);
	printf("calling storage_packet()...");
	ret = store_packet(conf, &msg, NULL);
	printf(" done [%d]\n", ret);
	printf("calling storage_packet()...");
	ret = store_packet(conf, &msg, NULL);
	printf(" done [%d]\n", ret);

	free(buf);

	/* store_now() */
	printf("\n--- store now\n");
	printf("calling storage_packet()...");
	ret = store_now(conf);
	printf(" done [%d]\n", ret);

	printf("config structure:\n");
	__PRINT_CONFIG_STRUCT(conf);

	/* storage_remove() */
	printf("\n--- storage remove\n");
	printf("calling storage_remove()...");
	ret = storage_remove(conf);
	printf(" done [%d]\n", ret);

	return 0;
}
#endif

