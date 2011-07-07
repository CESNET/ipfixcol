/**
 * \file ipfix-viewer.c
 * \author Michal Srb <michal.srb@cesnet.cz>
 * \brief Simple tool that displays IPFIX data stored in IPFIX file format.
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
 * \defgroup ipfixFileFormatViewer ipfix-viewer
 * 
 * Simple IPFIX file viewer.
 *
 * @{
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <netinet/in.h>
#include <endian.h>
#include <getopt.h>
#include <time.h>

#include "ipfix.h"
#include "templates.h"

#define MAXIMUM_IPFIX_MESSAGE_SIZE 65536

#define TEMPLATE_SET_TYPE         2
#define OPTIONS_TEMPLATE_SET_TYPE 3
#define DATA_SET_TYPE             256

#define TEMPLATE_SET_MINIMUM_SIZE     8
#define OPT_TEMPLATE_SET_MINIMUM_SIZE 10
#define DATA_SET_MINIMUM_SIZE         8
#define SETS_MINIMUM_SIZE             8


/* default values for input parameters */
#define OPTION_COUNT_DEFAULT     10
#define OPTION_SKIP_DEFAULT      0
#define OPTION_COUNT_SET_DEFAULT 0

#define read8(ptr) (*((uint8_t *) (ptr)))
#define read16(ptr) (*((uint16_t *) (ptr)))
#define read32(ptr) (*((uint32_t *) (ptr)))
#define read64(ptr) (*((uint64_t *) (ptr)))


struct input_options {
	unsigned int skip;      /* skip first n IPFIX messages */
	unsigned int count;     /* print only n IPFIX messages from file*/
	uint8_t count_set;      /* indicates whether user specified count 
	                         * parameter or not */
};


// FIXME - these should be in templates.h
int tm_init(void **config);
int tm_exit(void *config);


/* help */
static void usage(char *appname)
{
	fprintf(stderr, "Usage: %s [-s number] [-c number] ipfix_file\n\n", appname);
	fprintf(stderr, "  -s, --skip number      skip first 'number' messages "
	                "from IPFIX file\n");
	fprintf(stderr, "  -c, --count number     print only 'number' messages "
	                "from IPFIX file\n");
	fprintf(stderr, "  -h, --help             print this help and exit\n");
}

/**
 * \brief Print IPFIX message header
 *
 * \param[in] hdr message header structure
 * \return nothing
 */
static void print_header(struct ipfix_header *hdr)
{
	time_t time = (time_t) ntohl(hdr->export_time);
	char *str_time = ctime(&time);
	str_time[strlen(str_time)-1] = '\0';

	printf("--------------------------------------------------------------------------------\n");
	printf("IPFIX Message Header:\n");
	printf("\tVersion: %u\n", ntohs(hdr->version));
	printf("\tLength: %u\n", ntohs(hdr->length));
	printf("\tExport Time: %u (%s)\n", ntohl(hdr->export_time), str_time);
	printf("\tSequence Number: %u\n", ntohl(hdr->sequence_number));
	printf("\tObservation Domain ID: %u\n", ntohl(hdr->observation_domain_id));
}

/**
 * \brief Print set header
 *
 * \param[in] set_header set header structure
 * \return nothing
 */
static void print_set_header(struct ipfix_set_header *set_header)
{
	printf("Set Header:\n");
	printf("\tSet ID: %u", ntohs(set_header->flowset_id));

	switch (ntohs(set_header->flowset_id)) {
	case (2):
		printf(" (Template Set)\n");
		break;
	case (3):
		printf(" (Options Template Set)\n");
		break;
	default:
		if (ntohs(set_header->flowset_id) >= DATA_SET_TYPE) {
			printf(" (Data Set)\n");
		} else {
			printf(" (Unknown ID)\n");
		}
	}

	printf("\tLength: %u\n", ntohs(set_header->length));
}


/**
 * \brief Print template record
 *
 * \param[in] rec template record structure
 * \return length of the template in bytes (including header). 
 */
static uint16_t print_template_record(struct ipfix_template_record *rec)
{
	uint16_t count = 0;
	uint16_t offset = 0;
	uint16_t index;

	printf("Template Record Header:\n");
	printf("\tTemplate ID: %u\n", ntohs(rec->template_id));
	printf("\tField Count: %u\n", ntohs(rec->count));
	offset += 4;    /* template record header */

	printf("Fields:\n");
	
	index = count;

	/* print fields */
	while (count != ntohs(rec->count)) {
		printf("\tIE ID: %u\t", ntohs(rec->fields[index].ie.id));
		printf("\tField Length: %u\n", ntohs(rec->fields[index].ie.length));
		offset += 4;

		/* check for enterprise number bit */
		if (ntohs(rec->fields[index].ie.id) >> 15) {
			/* Enterprise number follows */
			++index;
			printf("\tEnterprise Number: %u\n", ntohl(rec->fields[index].enterprise_number));
			offset += 4;
		}

		++index;
		++count;
	}

	return offset;
}


/**
 * \brief Print options template record
 *
 * \param[in] rec options template record structure
 * \return length of the template in bytes (including header)
 */
static uint16_t print_options_template_record(struct ipfix_options_template_record *rec)
{
	uint16_t count = 0;       /* number of fields processed */
	uint16_t offset = 0;      /* offset in 'rec' structure (bytes) */
	uint16_t index;           /* index in rec->fields[] */

	printf("Options Template Record Header\n");
	printf("\tTemplate ID: %u\n", ntohs(rec->template_id));
	printf("\tField Count: %u\n", ntohs(rec->count));
	printf("\tScope Field Count: %u\n", ntohs(rec->scope_field_count));
	offset += 6;              /* size of the Opt. Template Record Header */

	printf("Fields:\n");
	
	index = count;

	while (count != (ntohs(rec->count) + ntohs(rec->scope_field_count))) {
		printf("\tIE ID: %u\t", ntohs(rec->fields[index].ie.id));
		printf("\tField Length: %u\n", ntohs(rec->fields[index].ie.length));
		offset += 4;

		/* check for enterprise number bit */
		if (ntohs(rec->fields[index].ie.id) & 0xf000) {
			/* Enterprise number follows */
			++index;
			printf("\tEnterprise Number: %u\n", ntohl(rec->fields[index].enterprise_number));
			offset += 4;
		}

		++index;
		++count;
	}

	return offset;
}


/**
 * \brief Print data record
 *
 * \param[in] templ_config template manager configuration structure
 * \param[in] records IPFIX data record
 * \param[in] template_id ID of the corresponding template for data records in
 * this set
 * \param[in] counter index of the data record in current data set
 * \return length of the data record in bytes (including header)
 */
static uint16_t print_data_record(void *tm, struct ipfix_data_set *records, 
                                  uint16_t template_id, unsigned int counter)
{
	struct ipfix_template *rec;
	int i;

	printf("Data Record (#%u):\t(network byte order)\n", counter);

	rec = tm_get_template(tm, template_id);
	if (!rec) {
		/* we don't have template for this data set */
		fprintf(stderr, "ERROR: no template for this data set\n");
		return 0;
	}

	uint16_t count = 0;
	uint16_t offset = 0;
	uint16_t index;
	uint16_t length;
	uint16_t id;

	index = count;

	/* print fields */
	while (count != ntohs(rec->field_count)) {
		id = ntohs(rec->fields[index].ie.id);
		printf("\tIE ID: %u\t", id);

		switch (length = ntohs(rec->fields[index].ie.length)) {
		case (1):
			printf("Value: %#x\n", read8(records->records+offset));
			offset += 1;
			break;
		case (2):
			printf("Value: %#x\n", read16(records->records+offset));
			offset += 2;
			break;
		case (4):
			printf("Value: %#x\n", read32(records->records+offset));
			offset += 4;
			break;
		case (8):
			printf("Value: %#lx\n", read64(records->records+offset));
			offset += 8;
			break;
		default:
			printf("0x");

			for (i = 0; i < length; i++) {
    			printf("%02x", records->records[i]);
			}
			printf("\n");

			offset += length;
			break;
		}

		if (id >> 15) {
			/* check for Enterprise Number */
			printf("Enterprise Number: %#x\n", read32(records->records+offset));
			offset += 4;
			++index;
		}

		++index;
		++count;
	}

	return offset;
}


/**
 * \brief Process (print, collect templates) all sets in the IPFIX message.
 *
 * \param[in] message pointer where the message starts
 * \param[in] tm template manager config
 * \param[in] print indicator whether we want to print things out message, or just
 * collect templates
 * \return pointer to the end of the IPFIX message
 */
static uint8_t *process_sets(uint8_t *message, void *tm, uint8_t print)
{
	uint16_t set_type;
	uint16_t data_record_counter;
	uint8_t *curr_set;

	struct ipfix_set_header *set_header;
	struct ipfix_header *header;
	struct ipfix_data_set *data_set;
	struct ipfix_template_record *template_record;
	struct ipfix_options_template_record *opt_template_record;
	struct ipfix_template *template;

	uint8_t *ptr;          /* current position in the message */
	struct ipfix_template *templ;
	uint16_t padding;
	uint16_t set_length;

	uint16_t ret;

	ptr = message + IPFIX_HEADER_LENGTH;

	/* message header */
	header = (struct ipfix_header *) message;

	uint16_t msg_length = ntohs(header->length);
	uint16_t sets_length = msg_length - IPFIX_HEADER_LENGTH;


	/* iterate over sets */
	while ((sets_length - (ptr - message)) > SETS_MINIMUM_SIZE) {

		/* current set header */
		set_header = (struct ipfix_set_header *) ptr;
		if (print) {
			print_set_header(set_header);
		}

		ptr += sizeof(struct ipfix_set_header);

		/* type of the set */
		set_type = ntohs(set_header->flowset_id);

		switch (set_type) {
		case (TEMPLATE_SET_TYPE):
			/* it's Temaplate Set */

			/* start of the records (template header) */
			curr_set = ptr;
	
			/* length of the set (with set header) */
			set_length = ntohs(set_header->length);

			/* print all template records from this set */
			while ((set_length - (ptr - (uint8_t *) set_header)) >= TEMPLATE_SET_MINIMUM_SIZE) {
				template_record = (struct ipfix_template_record *) ptr;
				if (print) {
					print_template_record(template_record);
				}
				if (template_record->count == 0) {
					/* it's withdrawal message */
					if (template_record->template_id == TEMPLATE_SET_TYPE) {
						/* withdraw all Templates */
						tm_remove_all_templates(tm, 0);
					} else {
						/* withdraw specific template */
						tm_remove_template(tm, template_record->template_id);
					}

					ptr += 4; /* 4=size of template record header */
				} else {
					/* add new template */
					templ = tm_add_template(tm, template_record, TEMPLATE_SET_TYPE);
					ptr += templ->template_length+4;
				}
			}

			/* all templates processed. is there any padding? */
			padding = (uint8_t *)set_header+ntohs(set_header->length) - ptr;
			if (padding) {
				if (print) {
					printf("Padding: %u\n", padding);
				}
				ptr += padding;
			}
	
			/* whole template set processed */
			break;

		case (OPTIONS_TEMPLATE_SET_TYPE):
			/* it's Options Template Set */

			/* start of the records (template header) */
			curr_set = ptr;
	
			/* length of the set (with set header) */
			set_length = ntohs(set_header->length);
			
			/* print all options template records from this set */
			while ((set_length - (ptr - (uint8_t *) set_header)) >= TEMPLATE_SET_MINIMUM_SIZE) {
				opt_template_record = (struct ipfix_options_template_record *) ptr;

				if (print) {
					print_options_template_record(opt_template_record);
				}
				if (opt_template_record->count == 0) {
					/* it's withdrawal message */
					if (opt_template_record->template_id == OPTIONS_TEMPLATE_SET_TYPE) {
						/* withdraw all Templates */
						tm_remove_all_templates(tm, 1);
					} else {
						/* withdraw specific template */
						tm_remove_template(tm, opt_template_record->template_id);
					}

					ptr += 6; /* 4=size of opt. template record header */
				} else {
					/* add new template */
					templ = tm_add_template(tm, template_record, OPTIONS_TEMPLATE_SET_TYPE);
					ptr += templ->template_length+6;
				}
			}

			/* all templates processed. is there any padding? */
			padding = (uint8_t *)set_header+ntohs(set_header->length) - ptr;
			if (padding) {
				if (print) {
					printf("Padding: %u\n", padding);
				}
				ptr += padding;
			}
	
			/* whole opt. template set processed */
			break;

		
		default:
			if (set_type >= DATA_SET_TYPE) {
				/* it's Data Set */
				data_record_counter = 0;
		
				/* start of the records (no header, raw data) */
				curr_set = ptr;

				/* length of the set (with set header) */
				set_length = ntohs(set_header->length);

				/* print all data records from this set */
				while (((set_length) - (ptr - (uint8_t *) set_header)) >= DATA_SET_MINIMUM_SIZE) {

					/* next data record */
					data_record_counter += 1;

					data_set = (struct ipfix_data_set *) ptr;

					if (print) {
						ret = print_data_record(tm, data_set, 
						      set_header->flowset_id, data_record_counter);
						if (ret == 0) {
							/* we don't have template for this set, skip */
							fprintf(stderr, "ERROR: No template for this data set.\n");
							ptr = ((uint8_t *) set_header) + set_header->length;
							break;
						}
						ptr += ret;
					} else {
						template = tm_get_template(tm, set_header->flowset_id);
						if (!template) {
							fprintf(stderr, "ERROR: No template for this data set.\n");
							ptr = ((uint8_t *) set_header) + set_header->length;
							/* skip all records in this set */
							break;
						}
					}
				}

				/* is there any padding? */
				padding = (uint8_t *)set_header+ntohs(set_header->length) - ptr;
				if (padding) {
					if (print) {
						printf("Padding: %u\n", padding);
					}
					ptr += padding;
				}

			} else {
				/* unknown set id */
				fprintf(stderr, "ERROR: unknown set id - %u\n", set_type);
				ptr += (uint8_t *) set_header + ntohs(set_header->length) - ptr;
			}

			break;
		}
	}

	return ptr;
}


/**
 * \brief Read whole IPFIX message from file
 *
 * \param[in] fd opened file descriptor of input file
 * \param[in] buf buffer, already allocated space where to store IPFIX message. Must not
 * be NULL
 * \param[in] size size of the buffer
 * \return length of the message
 */
static int32_t get_message(int fd, void *buf, size_t size)
{
	int ret;
	struct ipfix_header *header = buf;
	uint16_t msg_length;

	if ((buf == NULL) || (size < IPFIX_HEADER_LENGTH)) {
		return -1;
	}

	/* read only header first */
	ret = read(fd, header, IPFIX_HEADER_LENGTH);
	if (ret == 0) {
		return ret;
	}
	if (ret == -1) {
		perror("read");
		exit(EXIT_FAILURE);
	}
	if ((ret < IPFIX_HEADER_LENGTH)
	    || (ntohs(header->version) != IPFIX_VERSION)) {
		fprintf(stderr, "Invalid or corrupted IPFIX file\n");
		exit(EXIT_FAILURE);
	}

	msg_length = ntohs(header->length);

	if (size < msg_length) {
		fprintf(stderr, "Buffer is too small for IPFIX message\n");
		return -1;
	}

	/* read rest of the message */
	ret = read(fd, buf + IPFIX_HEADER_LENGTH, msg_length - IPFIX_HEADER_LENGTH);
	if (ret == -1) {
		perror("read");
		exit(EXIT_FAILURE);
	}

	return msg_length;
}

/**
 * \brief Process (print/get templates) IPFIX message. 
 *
 * \param[in] message IPFIX message
 * \param[in] tm template manager structure
 * \param[in] print 1 if we want to print out IPFIX message, 0 = if we just want
 * to process it (get templates).
 * \return 0 on success, negative value otherwise
 */
int process_message(uint8_t *message, struct ipfix_template_mgr *tm, int print)
{
	struct ipfix_header *header;

	header = (struct ipfix_header *) message;
	if (print) {
		print_header(header);
	}

	process_sets(message, tm, print);

	return 0;
}

int main(int argc, char **argv)
{
	if (argc < 2) {
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}


	int opt;
	struct ipfix_template_mgr *tm;/* template manager */
	int fd;                         /* input file */
	uint8_t *message;               /* start of the message */
	uint16_t msg_length;            /* message length */
	char *input_file;               /* input file */
	struct input_options options;   /* input options */
	uint8_t print_msg = 1;          /* 1 = print; 0 = silent */


	/* default options. use these if not specified otherwise */
	options.count_set = OPTION_COUNT_SET_DEFAULT;
	options.skip = OPTION_SKIP_DEFAULT;
	options.count = OPTION_COUNT_DEFAULT;

	static struct option long_options[] = {
	    {"skip", 1, 0, 's'},
	    {"count", 1, 0, 'c'},
	    {"help", 0, 0, 'h'},
	    {0, 0, 0, 0}
	};

	/* parse options */
	while (1) {
		opt = getopt_long(argc, argv, "hs:c:",
                        long_options, NULL);
		if (opt == -1) {
        	break;
		}

        switch (opt) {
        case 's':
			options.skip = atoi(optarg);
			break;
        case 'c':
			options.count = atoi(optarg);
			options.count_set = 1;
			break;
        case 'h':
			usage(argv[0]);
			exit(EXIT_SUCCESS);
			break;
		case '?':
			usage(argv[0]);
			exit(EXIT_FAILURE);
			break;
		default:
			fprintf(stderr, "fsdfs\n");	
		}
	}

	if (optind < argc) {
		input_file = argv[optind];
	} else {
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}


	/* initialize template manager */
	tm_init((void **) &tm);

	/* open input file */
	fd = open(input_file, O_RDONLY);
	if (fd == -1) {
		perror("open");
		exit(EXIT_FAILURE);
	}

	/* allocate memory for whole IPFIX message */
	message = (uint8_t *) malloc(MAXIMUM_IPFIX_MESSAGE_SIZE);
	if (!message) {
		fprintf(stderr, "not enough memory\n");
		exit(EXIT_FAILURE);
	}


	while (1) {
		/* read all IPFIX messages from file and process them */
		memset(message, 0, MAXIMUM_IPFIX_MESSAGE_SIZE);
	
		msg_length = get_message(fd, message, MAXIMUM_IPFIX_MESSAGE_SIZE);
		if (msg_length == 0) {
			printf("EOF\n");
			break;
		}

		if (options.skip) {
			/* skip some messages */
			print_msg = 0;
			--options.skip;
		} else {
			print_msg = 1;
			if (options.count_set) {
				/* print exactly 'count' messages */
				if (options.count) {
					--options.count;
				} else {
					break;
				}
			} 
		}

		process_message(message, tm, print_msg);
	}

	close(fd);

	free(message);
	tm_exit(tm);

	return 0;
}

