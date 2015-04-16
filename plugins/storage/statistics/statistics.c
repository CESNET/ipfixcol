/**
 * \file statistics.c
 * \author Petr Velan <petr.velan@cesnet.cz>
 * \brief Plugin for calculating statistics from IPFIX data.
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

/*
 * To add stored elements:
 * 1) Modify the database creation process and add new datasource in storage_init function
 * 2) Expand the stats_data structure
 * 3) Modify get_data_from_set function and expand the switch to work with new element
 * 4) Modify the template variable in store_packet function and add the new element's value
 */

#include <ipfixcol.h>
#include <stdio.h>
#include <string.h>
#include <endian.h>
#include <time.h>
#include <errno.h>
#include <sys/stat.h>
#include <rrd.h>
#include <libxml/parser.h>

/** Default interval for statistics*/
#define DEFAULT_INTERVAL 300

/* some auxiliary functions for extracting data of exact length */
#define read8(ptr) (*((uint8_t *) (ptr)))
#define read16(ptr) (*((uint16_t *) (ptr)))
#define read32(ptr) (*((uint32_t *) (ptr)))
#define read64(ptr) (*((uint64_t *) (ptr)))

/**
 * \struct stats_data
 * \brief Structure to hold statistics data
 */
struct stats_data {
	uint64_t	bytes;
	uint64_t	packets;
	uint64_t	flows;
};

/**
 * \struct stats_config
 *
 * \brief Statistics storage plugin specific "config" structure
 */
struct stats_config {
	uint16_t			interval;
	char 				*filename;
	struct stats_data	data;
	time_t				last;
};


/** Identifier to MSG_* macros */
static char *msg_module = "statistics";

static uint64_t read_data(uint8_t *ptr, uint16_t length)
{
	uint64_t value = 0;

	switch (length) {
			case (1):
				value = read8(ptr);
				break;
			case (2):
				value = ntohs(read16(ptr));
				break;
			case (4):
				value = ntohl(read32(ptr));
				break;
			case (8):
				value = be64toh(read64(ptr));
				break;
			default:
				/* we do not support any other value */
				MSG_WARNING(msg_module, "Variable length %i not supported\n");
				break;
			}

	return value;
}

/**
 * \brief Get data from data record
 *
 * \param[in] data_record IPFIX data record
 * \param[in] template corresponding template
 * \param[out] data statistics data to get
 * \return length of the data record
 */
static uint16_t get_data_from_set(uint8_t *data_record, struct ipfix_template *template, struct stats_data *data)
{
	if (!template) {
		/* we don't have template for this data set */
		MSG_WARNING(msg_module, "No template for the data set\n");
		return 0;
	}

	uint16_t offset = 0;
	uint16_t index, count;
	uint16_t length;
	uint16_t id;

	/* go over all fields */
	for (count = index = 0; count < template->field_count; count++, index++) {
		id = template->fields[index].ie.id;
		length = template->fields[index].ie.length;

		/* if (id >> 15) {Enterprise Number} */

		switch (id) {
		case 1:
			data->bytes += read_data(data_record+offset, length);
			break;
		case 2:
			data->packets += read_data(data_record+offset, length);
			break;
		default:
			break;
		}

		/* skip the length of the value */
		if (length != VAR_IE_LENGTH) {
			offset += length;
		} else {
			/* variable length */
			length = read8(data_record+offset);
			offset += 1;

			if (length == 255) {
				length = ntohs(read16(data_record+offset));
				offset += 2;
			}
			offset += length;
		}
	}

	/* increment number of flows */
	data->flows += 1;

	return offset;
}

/**
 * \brief Process all data sets in IPFIX message
 *
 * \param[in] ipfix_msg IPFIX message
 * \param[out] data statistics data to get
 * \return 0 on success, -1 otherwise
 */
static int process_data_sets(const struct ipfix_message *ipfix_msg, struct stats_data *data)
{
	uint16_t data_index = 0;
	struct ipfix_data_set *data_set;
	uint8_t *data_record;
	struct ipfix_template *template;
	uint32_t offset;
	uint16_t min_record_length;


	data_set = ipfix_msg->data_couple[data_index].data_set;

	while(data_set) {
		template = ipfix_msg->data_couple[data_index].data_template;

		/* skip datasets with missing templates */
		if (template == NULL) {
			/* process next set */
			data_set = ipfix_msg->data_couple[++data_index].data_set;

			continue;
		}

		min_record_length = template->data_length;
		offset = 4;  /* size of the header */

		if (min_record_length & 0x8000) {
			/* record contains fields with variable length */
			min_record_length = min_record_length & 0x7fff; /* size of the fields, variable fields excluded  */
		}

		while ((int) ntohs(data_set->header.length) - (int) offset - (int) min_record_length >= 0) {
			data_record = (((uint8_t *) data_set) + offset);
			offset += get_data_from_set(data_record, template, data);
		}

		/* process next set */
		data_set = ipfix_msg->data_couple[++data_index].data_set;
	}

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
	struct stats_config *conf;
	xmlDocPtr doc;
	xmlNodePtr cur;

	conf = (struct stats_config *) malloc(sizeof(*conf));
	if (!conf) {
		MSG_ERROR(msg_module, "Out of memory (%s:%d)", __FILE__, __LINE__);
		return -1;
	}
	memset(conf, 0, sizeof(*conf));

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

	/* process the configuration elements */
	cur = cur->xmlChildrenNode;
	while (cur != NULL) {
		if ((!xmlStrcmp(cur->name, (const xmlChar *) "interval"))) {
			char *interval = (char *) xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			if (interval != NULL) {
				conf->interval = atoi(interval);
				free(interval);
			}
		}
		if ((!xmlStrcmp(cur->name, (const xmlChar *) "file"))) {
			if (!conf->filename) {
				conf->filename = (char *) xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			}
		}
		cur = cur->next;
	}

	/* filename must be specified */
	if (!conf->filename) {
		MSG_ERROR(msg_module, "RRD database file not given");
		goto err_xml;
	}

	/* use default values if not specified in configuration */
	if (conf->interval == 0) {
		conf->interval = DEFAULT_INTERVAL;
	}

	/* initialize RRD database file */
	int rrd_argc = 0, i;
	char *rrd_argv[16], timebuff[128], stepbuff[64], ds_bytes[64], ds_packets[64], ds_flows[64];
	struct stat sts;

	/* do not overwrite existing file */
	if (stat(conf->filename, &sts) == -1 && errno == ENOENT) {
		rrd_argv[rrd_argc++] = "create";
		rrd_argv[rrd_argc++] = conf->filename;
		snprintf(timebuff, 128, "--start=%lld", (long long)time(NULL)); /* start timestamp */
		rrd_argv[rrd_argc++] = timebuff;
		snprintf(stepbuff, 64, "--step=%d", conf->interval); /* interval (step) at which values are stored */
		rrd_argv[rrd_argc++] = stepbuff;
		snprintf(ds_bytes, 64, "DS:bytes:GAUGE:%u:0:U", conf->interval*2); /* datasource definition, wait 2x the interval for data */
		snprintf(ds_packets, 64, "DS:packets:GAUGE:%u:0:U", conf->interval*2);
		snprintf(ds_flows, 64, "DS:flows:GAUGE:%u:0:U", conf->interval*2);
		rrd_argv[rrd_argc++] = ds_bytes;
		rrd_argv[rrd_argc++] = ds_packets;
		rrd_argv[rrd_argc++] = ds_flows;
		rrd_argv[rrd_argc++] = "RRA:AVERAGE:0.5:1:2016"; /* store 2016 values 1:1 from input */
		rrd_argv[rrd_argc++] = "RRA:AVERAGE:0.5:24:720"; /* store 720 values, each averaged from 24 input values */
		rrd_argv[rrd_argc++] = "RRA:AVERAGE:0.5:288:180"; /* store 180 values, each averaged from 288 input values */

		if ((i = rrd_create(rrd_argc, rrd_argv))) {
			MSG_ERROR(msg_module, "Create RRD DB Error: %ld %s\n", i, rrd_get_error());
			rrd_clear_error();
		}
	}

	*config = conf;

	/* destroy the XML configuration document */
	xmlFreeDoc(doc);

	return 0;

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

	struct stats_config *conf = (struct stats_config*) config;

	if (conf->last == 0) {
		conf->last = time(NULL);
	} else if (time(NULL) > conf->last + conf->interval) {
		conf->last = time(NULL);

	/*	printf("###############\n  Bytes: %lu\n  Packets: %lu\n  Flows: %lu\n###############\n",
					conf->data.bytes, conf->data.packets, conf->data.flows);	*/

		/* update RRD database file */
		int rrd_argc = 0, i;
		char *rrd_argv[16], buff[128], *template = "bytes:packets:flows";
		rrd_argv[rrd_argc++] = "update";
		rrd_argv[rrd_argc++] = conf->filename;
		rrd_argv[rrd_argc++] = "--template";
		rrd_argv[rrd_argc++] = template;
		snprintf(buff, 128, "%llu:%lu:%lu:%lu", (long long) conf->last,
				conf->data.bytes, conf->data.packets, conf->data.flows);
		rrd_argv[rrd_argc++] = buff;

		if (( i=rrd_update(rrd_argc, rrd_argv))) {
			printf("RRD Insert Error: %d %s\n", i, rrd_get_error());
			rrd_clear_error();
		}

		/* reset the counters */
		memset(&conf->data, 0, sizeof(struct stats_data));
	}

	process_data_sets(ipfix_msg, &conf->data);

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
	struct stats_config *conf = (struct stats_config*) *config;

	free(conf->filename);
	free(*config);
	return 0;
}
