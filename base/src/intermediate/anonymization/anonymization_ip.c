/**
 * \file anonymization_ip.c
 * \author Michal Srb <michal.srb@cesnet.cz>
 * \brief Anonymization Intermediate Process
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
 * \defgroup anonInter Anonymization Intermediate Process
 * \ingroup intermediatePlugins
 *
 * This plugin anomymize IP addresses (both IPv4 and IPv6) in IPFIX
 * data records
 *
 * @{
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <arpa/inet.h>

#include <ipfixcol.h>

#include "Crypto-PAn/panonymizer.h"
#include <ipfixcol.h>

/* API version constant */
IPFIXCOL_API_VERSION;

static char *msg_module = "Anon IP";


#define ANONYMIZATION_TYPE_TRUNCATION    1
#define ANONYMIZATION_TYPE_CRYPTOPAN     2


/* interesting IPFIX entities */
/* IPv4 */                 /* element ID, IP version, element name */
#define sourceIPv4Address             {8, 4, "sourceIPv4Address"}
#define destinationIPv4Address        {12, 4, "destinationIPv4Address"}
#define ipNextHopIPv4Address          {15, 4, "ipNextHopIPv4Address"}
#define bgpNextHopIPv4Address         {18, 4, "bgpNextHopIPv4Address"}
#define sourceIPv4Prefix              {44, 4, "sourceIPv4Prefix"}
#define destinationIPv4Prefix         {45, 4, "destinationIPv4Prefix"}
#define mplsTopLabelIPv4Address       {47, 4, "mplsTopLabelIPv4Address"}
#define exporterIPv4Address           {130, 4, "exporterIPv4Address"}
#define collectorIPv4Address          {221, 4, "collectorIPv4Address"}
#define postNATSourceIPv4Address      {225, 4, "postNATSourceIPv4Address"}
#define postNATDestinationIPv4Address {226, 4, "postNATDestinationIPv4Address"}
#define staIPv4Address                {366, 4, "staIPv4Address"}
/* IPv6 */
#define sourceIPv6Address             {27, 6, "sourceIPv6Address"}
#define destinationIPv6Address        {28, 6, "destinationIPv6Address"}
#define ipNextHopIPv6Address          {62, 6, "ipNextHopIPv6Address"}
#define bgpNextHopIPv6Address         {63, 6, "bgpNextHopIPv6Address"}
#define exporterIPv6Address           {131, 6, "exporterIPv6Address"}
#define mplsTopLabelIPv6Address       {140, 6, "mplsTopLabelIPv6Address"}
#define destinationIPv6Prefix         {169, 6, "destinationIPv6Prefix"}
#define sourceIPv6Prefix              {170, 6, "sourceIPv6Prefix"}
#define collectorIPv6Address          {212, 6, "collectorIPv6Address"}
#define postNATSourceIPv6Address      {281, 6, "postNATSourceIPv6Address"}
#define postNATDestinationIPv6Address {282, 6, "postNATDestinationIPv6Address"}


struct ipfix_entity {
	uint16_t element_id;
	uint8_t ip_version;
	char *entity_name;
};

/** this entities will be anonymized by this plugin */
static struct ipfix_entity entities_to_anonymize[] = {
		sourceIPv4Address, destinationIPv4Address, sourceIPv6Address, destinationIPv6Address
};
#define entities_array_length     4


/** plugin's configuration structure */
struct anonymization_ip_config {
	char *params;         /* XML configuration */
	void *ip_config;      /* config structure for Intermediate Process */
	uint8_t type;         /* anonymization type */
	uint32_t ip_id;       /* Intermediate plugin source ID into template manager */
	struct ipfix_template_mgr *tm;
};



/**
 * \brief Truncate IPv4 address
 *
 * \param[in] data pointer where address starts
 * \return void
 */
static void truncate_IPv4Address(uint8_t *data)
{
	*(data+2) = 0x00;
	*(data+3) = 0x00;
}


/**
 * \brief Truncate IPv6 address
 *
 * \param[in] data pointer where address starts
 * \return void
 */
static void truncate_IPv6Address(uint8_t *data)
{
	memset(data+7, 0, 8);
}


/**
 *  \brief Initialize Intermediate Plugin
 *
 * \param[in] params configuration xml for the plugin
 * \param[in] ip_config configuration structure of corresponding intermediate process
 * \param[in] ip_id source ID into template manager for creating templates
 * \param[in] template_mgr collector's Template Manager
 * \param[out] config configuration structure
 * \return 0 on success, negative value otherwise
 */
int intermediate_init(char *params, void *ip_config, uint32_t ip_id, struct ipfix_template_mgr *template_mgr, void **config)
{
	struct anonymization_ip_config *conf;
	int retval;
	uint8_t key;


	if (!params) {
		MSG_ERROR(msg_module, "Missing plugin configuration");
		return -1;
	}

	/* initialize Crypto-PAn library */
	key = rand() % 256;
	PAnonymizer_Init(&key);
    	MSG_DEBUG(msg_module, "Crypto-PAn library initialized");

	conf = (struct anonymization_ip_config *) malloc(sizeof(*conf));
	if (!conf) {
		MSG_ERROR(msg_module, "Unable to allocate memory (%s:%d)", __FILE__, __LINE__);
		return -1;
	}
	memset(conf, 0, sizeof(*conf));

    /* parse params */
    xmlDoc *doc = NULL;
    xmlNode *root_element = NULL;
    xmlNode *cur_node = NULL;


    /* parse xml string */
    doc = xmlParseDoc(BAD_CAST params);
    if (doc == NULL) {
    	MSG_ERROR(msg_module, "Cannot parse config xml");
        retval = 1;
        goto out;
    }
    /* get the root element node */
    root_element = xmlDocGetRootElement(doc);
    if (root_element == NULL) {
    	MSG_ERROR(msg_module, "Cannot get document root element");
        retval = 1;
        goto out;
    }


    /* go over all elements */
    for (cur_node = root_element->children; cur_node; cur_node = cur_node->next) {

        if (cur_node->type == XML_ELEMENT_NODE && cur_node->children != NULL) {
            /* copy value to memory - don't forget the terminating zero */
            int tmp_val_len = strlen((char *) cur_node->children->content) + 1;
            char *tmp_val = malloc(sizeof(char) * tmp_val_len);
            /* this is not a preferred cast, but we really want to use plain chars here */
            if (tmp_val == NULL) {
            	MSG_ERROR(msg_module, "Cannot allocate memory: %s", strerror(errno));
                retval = 1;
                goto out;
            }
            strncpy_safe(tmp_val, (char *)cur_node->children->content, tmp_val_len);

            if (xmlStrEqual(cur_node->name, BAD_CAST "type")) { /* anonymization type */
                if (!strcmp(tmp_val, "truncation")) {
                	conf->type = ANONYMIZATION_TYPE_TRUNCATION;
                } else if (!strcmp(tmp_val, "cryptopan")) {
                	conf->type = ANONYMIZATION_TYPE_CRYPTOPAN;
                }
            }
            free(tmp_val);

        }
    }

	conf->params = params;
	conf->ip_config = ip_config;
	conf->ip_id = ip_id;
	conf->tm = template_mgr;

	xmlFreeDoc(doc);

	*config = conf;

	MSG_NOTICE(msg_module, "Successfully initialized");

	/* plugin successfully initialized */
	return 0;


out:
	free(conf);

	return retval;
}


/**
 * \brief Anonymization Intermediate Process
 *
 * \param[in] config configuration structure
 * \param[in] message IPFIX message
 * \return 0 on success, negative value otherwise
 */
int intermediate_process_message(void *config, void *message)
{
	struct ipfix_message *msg;
	struct ipfix_data_set *data_set;
	struct ipfix_template *template;
	int index;
	int ret;
	uint8_t *p;
	uint32_t *data;
	uint8_t **data_records;
	uint16_t data_records_index;
	char ip_orig[INET6_ADDRSTRLEN];
	char ip_anon[INET6_ADDRSTRLEN];
	struct anonymization_ip_config *conf;
	uint32_t odid;

	conf = (struct anonymization_ip_config *) config;


	msg = (struct ipfix_message *) message;

	if (msg->source_status == SOURCE_STATUS_CLOSED) {
		/* Source was closed */
		pass_message(conf->ip_config, msg);
		return 0;
	}

	if (msg->pkt_header->version != htons(IPFIX_VERSION)) {
		/* this is control message and this plugin is not interested in it */
		pass_message(conf->ip_config, msg);
		return 0;
	}

	odid = ntohl(msg->pkt_header->observation_domain_id);
	index = 0;
	while ((data_set = msg->data_couple[index].data_set) != NULL) {
		template = msg->data_couple[index].data_template;

		if (!template) {
			/* oops, this is weird... no template for data set, skip it (note this might be a bug in previous intermediate plugin) */
			++index;
			continue;
		}

		int entities_index = 0;
		while (entities_index < entities_array_length) {
			ret = template_contains_field(template, entities_to_anonymize[entities_index].element_id);
			if (ret >= 0) {
				switch (entities_to_anonymize[entities_index].ip_version) {
				case 4: {
					/* iterate over data records and modify IP address fields */
					data_records = get_data_records(data_set, template);

					data_records_index = 0;
					while (data_records[data_records_index]) {
						p = data_records[data_records_index];
						message_get_data((uint8_t **) &data, p + ret, 4); /* IPv4 address is 4 octet integer */

						inet_ntop(AF_INET, data, ip_orig, INET6_ADDRSTRLEN);

						if (conf->type == ANONYMIZATION_TYPE_CRYPTOPAN) {
							/* anonymization type: cryptopan */
							uint32_t new_addr;
							uint32_t old_addr = (uint32_t) *data;

							old_addr = ntohl(old_addr);

							/* anonymize given IPv4 address using CryptoPAn */
							new_addr = anonymize(old_addr);

							new_addr = htonl(new_addr);

							message_set_data(p + ret, (uint8_t *) &new_addr, 4);

							inet_ntop(AF_INET, &new_addr, ip_anon, INET6_ADDRSTRLEN);

							MSG_DEBUG(msg_module, "[%u] %s %s -> %s", odid, entities_to_anonymize[entities_index].entity_name,
									ip_orig, ip_anon);

							free(data);

							++data_records_index;

							continue;
						}

						/* anonymization type: truncation (this is default) */

						truncate_IPv4Address((uint8_t *) data);
						message_set_data(p + ret, (uint8_t *) data, 4);

						inet_ntop(AF_INET, data, ip_anon, INET6_ADDRSTRLEN);

						MSG_DEBUG(msg_module, "[%u] %s %s -> %s", odid, entities_to_anonymize[entities_index].entity_name,
								ip_orig, ip_anon);

						free(data);

						++data_records_index;
					}

					free(data_records);
				}
				break;
				case 6: {
					/* iterate over data records and modify fields with IP addresses */
					data_records = get_data_records(data_set, template);

					data_records_index = 0;
					while (data_records[data_records_index]) {
						p = data_records[data_records_index];
						message_get_data((uint8_t **) &data, p + ret, 16); /* IPv6 address is 16 octet integer */

						inet_ntop(AF_INET6, data, ip_orig, INET6_ADDRSTRLEN);

						if (conf->type == ANONYMIZATION_TYPE_CRYPTOPAN) {
							uint64_t *old_addr = (uint64_t *) data;
							uint64_t new_addr[2];
							anonymize_v6(old_addr, new_addr);

							message_set_data(p + ret, (uint8_t *) new_addr, 16);

							inet_ntop(AF_INET6, new_addr, ip_anon, INET6_ADDRSTRLEN);

							MSG_DEBUG(msg_module, "[%u] %s %s -> %s", odid,
									entities_to_anonymize[entities_index].entity_name, ip_orig, ip_anon);

							free(data);

							++data_records_index;

							continue;
						}

						/* anonymization type: truncation (this is default) */


						truncate_IPv6Address((uint8_t *) data);
						message_set_data(p + ret, (uint8_t *) data, 16);

						inet_ntop(AF_INET6, data, ip_anon, INET6_ADDRSTRLEN);

						MSG_DEBUG(msg_module, "[%u] %s %s -> %s", odid,
								entities_to_anonymize[entities_index].entity_name, ip_orig, ip_anon);

						free(data);

						++data_records_index;
					}

					free(data_records);
				}
				break;
				default:
					MSG_ERROR(msg_module, "[%u] Invalid address family", odid);
					break;
				} /* switch */
			} /* if */

			++entities_index;
		} /* while */

		++index;
	}

	pass_message(conf->ip_config, message);

	return 0;
}


/**
 * \brief Close Intermediate Plugin
 *
 * \param[in] config configuration structure
 * \return 0 on success, negative value otherwise
 */
int intermediate_close(void *config)
{
	struct anonymization_ip_config *conf;

	conf = (struct anonymization_ip_config *) config;

	free(conf);

	return 0;
}

/**@}*/
