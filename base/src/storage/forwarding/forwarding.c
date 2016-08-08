/**
 * \file storage/forwarding/forwarding.c
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Forwarind plugin interface (source file)
 *
 * Copyright (C) 2016 CESNET, z.s.p.o.
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
 * \defgroup forwardingStoragePlugin Forwarding output process
 * \ingroup storagePlugins
 *
*/
 
 
/**
 * \defgroup forwaring Main
 * \ingroup forwardingStoragePlugin
 *
 * @{
*/

#include <ipfixcol.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <inttypes.h>

#include "configuration.h"

// API version constant
IPFIXCOL_API_VERSION

// Module identification
static const char* msg_module= "forwarding";

/**
 * \brief Get a number of data records in a Data Set
 * \param[in] msg IPFIX message
 * \param[in] header Pointer to Data set header
 * \return On error returns -1. Otherwise returns number of data records.
 */
static int fwd_rec_cnt(const struct ipfix_message *msg,
	const struct ipfix_set_header *header)
{
	// Find template
	int i;
	struct ipfix_template *tmplt = NULL;

	for (i = 0; i < MSG_MAX_DATA_COUPLES && msg->data_couple[i].data_set; ++i) {
		if (&msg->data_couple[i].data_set->header != header) {
			continue;
		}

		// Couple found
		tmplt = msg->data_couple[i].data_template;
		break;
	}

	if (!tmplt) {
		// Unknown template
		return -1;
	}

	// Get number of records
	return data_set_records_count(msg->data_couple[i].data_set, tmplt);
}

/**
 * \brief Auxiliary structure for processing of an (Options) Template record
 */
struct template_ctx {
	/** Type of the template (TM_TEMPLATE or TM_OPTIONS_TEMPLATE)      */
	int type;
	/**< A configuration of the plugin (a template manager, etc.)        */
	struct plugin_config *cfg;
	/**< Identification of a flow source                                 */
	const struct input_info *src_info;

	/**< Status flag                                                     */
	bool fail;
};

/**
 * \brief Process a one (Options) Template record
 * \remark This is a function for a callback
 * \param[in]     rec     (Options) Template record
 * \param[in]     rec_len A length of the record
 * \param[in,out] data    Configuration of the plugin
 */
static void fwd_process_template_func(uint8_t *rec, int rec_len, void *data)
{
	struct template_ctx *ctx = (struct template_ctx *) data;
	struct ipfix_template_record *tmplt = (struct ipfix_template_record *) rec;

	if (ctx->fail) {
		// Processing of a previous template in the same set failed
		return;
	}

	enum TMPLT_MGR_ACTION action;
	uint16_t new_id;

	action = tmplts_process_template(ctx->cfg->tmplt_mgr, ctx->src_info, tmplt,
		ctx->type, rec_len, &new_id);
	switch (action) {
	case TMPLT_ACT_PASS:
		// Pass a template (with possibly new ID assigned by the manager)
		break;
	case TMPLT_ACT_DROP:
		// Drop a template
		return;
	case TMPLT_ACT_INVALID:
		// Invalid operation (The template manager wrote out a warning)
		return;
	default:
		MSG_ERROR(msg_module, "Internal error: Unexpected type of an operation "
			"with a template (%s:%d)", __FILE__, __LINE__);
		ctx->fail = true;
		return;
	}

	// Only "TMPLT_ACT_PASS" can get here
	int ret_all, ret_tmplt;
	ret_all = bldr_add_template(ctx->cfg->builder_all, rec, rec_len, new_id,
		ctx->type);
	ret_tmplt = bldr_add_template(ctx->cfg->builder_tmplt, rec, rec_len, new_id,
		ctx->type);

	if (ret_all != 0 || ret_tmplt != 0) {
		MSG_ERROR(msg_module, "Failed to add a template (Template ID: "
			"%" PRIu16 ") into a new packet. Some flows will be probably lost "
			"in the future on one or more destinations.", new_id);
		ctx->fail = true;
	}
}

/**
 * \brief Process and add an (Options) Templates Set to the Packet builder
 * \param[in,out] cfg    Configuration of the plugin
 * \param[in]     msg    IPFIX packet which belongs to the Set
 * \param[in]     header Header of the (Options) Templates Set
 * \return On success returns 0. Otherwise returns non-zero value.
 */
static int fwd_process_template_set(struct plugin_config *cfg,
	const struct ipfix_message *msg, const struct ipfix_set_header *header)
{
	(void) msg;

	int type;
	switch (ntohs(header->flowset_id)) {
	case IPFIX_TEMPLATE_FLOWSET_ID:
		type = TM_TEMPLATE;
		break;
	case IPFIX_OPTION_FLOWSET_ID:
		type = TM_OPTIONS_TEMPLATE;
		break;
	default:
		MSG_ERROR(msg_module, "Unknown type of a set (Flowset ID: %" PRIu16 ") "
			"in an IPFIX packet from a source with ODID %" PRIu32 ".",
			ntohs(header->flowset_id),
			ntohl(msg->pkt_header->observation_domain_id));
		return 1;
	}

	// WARNING: const -> non const (ugly)
	struct ipfix_template_set *set = (struct ipfix_template_set *) header;
	struct template_ctx ctx = {type, cfg, msg->input_info, false};

	// Process templates in the Set
	template_set_process_records(set, type, fwd_process_template_func, &ctx);
	return (ctx.fail) ? 1 : 0;
}

/**
 * \brief Process and add a Data Set to the Packet builder
 * \param[in,out] cfg    Configuration of the plugin
 * \param[in]     msg    IPFIX packet which belongs to the Data Set
 * \param[in]     header Header of the Data Set
 * \return On success returns 0. Otherwise returns non-zero value.
 */
static int fwd_process_data_set(struct plugin_config *cfg,
	const struct ipfix_message *msg, const struct ipfix_set_header *header)
{
	if (ntohs(header->flowset_id) < IPFIX_MIN_RECORD_FLOWSET_ID) {
		MSG_WARNING(msg_module, "Unknown Set ID %" PRIu16 " skipped.",
			ntohs(header->flowset_id));
		return 0;
	}

	// Get a number of records in the Set
	int rec_cnt;
	rec_cnt = fwd_rec_cnt(msg, header);
	if (rec_cnt == 0) {
		// Empty Data set -> skip
		MSG_WARNING(msg_module, "Skipping a data set (Flowset ID: "
			"%" PRIu16 ") with no records from the ODID %" PRIu32 ".",
			ntohs(header->flowset_id), msg->input_info->odid);
		return 0;
	}

	if (rec_cnt == -1) {
		// Unknown template -> skip
		MSG_WARNING(msg_module, "Missing a template (Flowset ID: "
			"%" PRIu16 ") for a Data Set from the ODID %" PRIu32 ". "
			"Some records will be definitely lost.",
			ntohs(header->flowset_id), msg->input_info->odid);
		return 0;
	}

	// Add the Data Set to the Packet builder
	uint16_t new_id;
	new_id = tmplts_remap_data_set(cfg->tmplt_mgr, msg->input_info, header);
	if (new_id == 0) {
		// Template mapping is unknown -> skip
		MSG_WARNING(msg_module, "Template manager of the plugin doesn't have "
			"a template (Template ID: %" PRIu16 ") for a Data Set from "
			"a source with the ODID %" PRIu32 ". Some records will be "
			"definitely lost.",
			ntohs(header->flowset_id), msg->input_info->odid);
		return 0;
	}

	const struct ipfix_data_set *data_set;
	data_set = (const struct ipfix_data_set *) header;

	if (bldr_add_dataset(cfg->builder_all, data_set, new_id, rec_cnt)) {
		return 1;
	}

	return 0;
}

/**
 * \brief Add withdrawal templates for specified Observation Domain ID to
 * the packet
 *
 * Withdrawal templates are prepared by the Template manager.
 * \param[in,out] cfg  Plugin configuration (Template manager, packets, etc.)
 * \param[in]  odid  Observation Domain ID
 * \param[in]  type  Type of the template (TM_TEMPLATE or TM_OPTIONS_TEMPLATE)
 * \return On success returns 0. Otherwise return non-zero value.
 */
static int fwd_process_withdrawals(struct plugin_config *cfg, uint32_t odid,
	int type)
{
	uint16_t  ids_cnt;
	uint16_t *ids_data;
	int ret_all, ret_tmplt;

	ids_data = tmplts_withdraw_ids(cfg->tmplt_mgr, odid, type, &ids_cnt);
	if (!ids_data) {
		return 1;
	}

	for (unsigned int i = 0; i < ids_cnt; ++i) {
		const uint16_t id = ids_data[i];
		ret_all =   bldr_add_template_withdrawal(cfg->builder_all,   id, type);
		ret_tmplt = bldr_add_template_withdrawal(cfg->builder_tmplt, id, type);

		if (ret_all != 0 || ret_tmplt != 0) {
			free(ids_data);
			return 1;
		}
	}

	free(ids_data);
	return 0;
}

/*
//
 * \brief Remove all information about a flow source and prepare withdrawal
 *   templates
 * \param[in,out] cfg Plugin configuration
 * \param[in]     msg IPFIX message
 * \return On success returns 0. Otherwise returns non-zero value
//
static int fwd_remove_source(struct plugin_config *cfg,
	const struct ipfix_message *msg)
{
	// Prepare internal structures of packet builder for a new packet(s)
	uint32_t pkt_odid = 0;// TODO
	uint32_t pkt_exp_time = 0; // TODO: see advice below

	// TODO and good advice:
	// Prepare a structure info which you will store the latest timestamp
	// ("export_time" from a header of an IPFIX packet) for each ODID.
	// It is very important, because you must not send a templates withdrawal
	// with a timestamp before the latest timestamp!!!

	bldr_start(cfg->builder_all, pkt_odid, pkt_exp_time);
	bldr_start(cfg->builder_tmplt, pkt_odid, pkt_exp_time);

	if (tmplts_remove_source(cfg->tmplt_mgr, msg->input_info)) {
		return 1;
	}

	// Add withdrawal templates to the end of the message
	if (fwd_process_withdrawals(cfg, pkt_odid, TM_TEMPLATE)) {
		return 1;
	}

	if (fwd_process_withdrawals(cfg, pkt_odid, TM_OPTIONS_TEMPLATE)) {
		return 1;
	}

	// Generate packets
	if (bldr_end(cfg->builder_all, cfg->packet_size)) {
		return 1;
	}

	if (bldr_end(cfg->builder_tmplt, cfg->packet_size)) {
		return 1;
	}

	return 0;
}
*/

/**
 * \brief Parse IPFIX message and prepare packet(s)
 * \param[in,out] cfg Plugin configuration
 * \param[in] msg IPFIX message
 * \return On success returns 0. Otherwise returns non-zero value.
 */
static int fwd_parse_msg(struct plugin_config *cfg,
	const struct ipfix_message *msg)
{
	if (msg->pkt_header == NULL) {
		return 1;
	}

	uint16_t flowset_id;
	struct ipfix_set_header *set_header;
	uint16_t set_len;
	int (*parser)(struct plugin_config *, const struct ipfix_message *,
		const struct ipfix_set_header *);

	// Prepare internal structures of packet builder for a new packet(s)
	uint32_t pkt_odid = ntohl(msg->pkt_header->observation_domain_id);
	uint32_t pkt_exp_time = ntohl(msg->pkt_header->export_time);
	bldr_start(cfg->builder_all, pkt_odid, pkt_exp_time);
	bldr_start(cfg->builder_tmplt, pkt_odid, pkt_exp_time);
	bool any_templates = false;

	// Process IPFIX message
	uint8_t *pos = ((uint8_t *) msg->pkt_header) + IPFIX_HEADER_LENGTH;
	uint16_t pkt_len = ntohs(msg->pkt_header->length);

	while (pos < ((uint8_t *) msg->pkt_header) + pkt_len) {
		// Get a header of the set
		set_header = (struct ipfix_set_header *) pos;
		set_len = ntohs(set_header->length);

		// Check a length of the set
		if (pos + set_len > ((uint8_t *) msg->pkt_header) + pkt_len) {
			MSG_WARNING(msg_module, "Malformed IPFIX message detected (ODID: "
				"%"PRIu32") and skipped.", pkt_odid);
			return 1;
		}

		// Get a typ of the Set and add it into the Packet builder
		flowset_id = ntohs(set_header->flowset_id);
		if (flowset_id == IPFIX_TEMPLATE_FLOWSET_ID
				|| flowset_id == IPFIX_OPTION_FLOWSET_ID) {
			// Template Set
			parser = &fwd_process_template_set;
			any_templates = true;
		} else {
			// Data Set
			parser = &fwd_process_data_set;
		}

		if (parser(cfg, msg, set_header)) {
			return 1;
		}

		if (ntohs(set_header->length) == 0) {
			// Avoid infinite loop
			break;
		}

		pos += ntohs(set_header->length);
	}

	if (any_templates) {
		// Add withdrawal templates to the end of the message
		if (fwd_process_withdrawals(cfg, pkt_odid, TM_TEMPLATE)) {
			return 1;
		}

		if (fwd_process_withdrawals(cfg, pkt_odid, TM_OPTIONS_TEMPLATE)) {
			return 1;
		}
	}

	// Generate packets
	if (bldr_end(cfg->builder_all, cfg->packet_size)) {
		return 1;
	}

	if (bldr_end(cfg->builder_tmplt, cfg->packet_size)) {
		return 1;
	}

	return 0;
}

// Storage plugin initialization function.
int storage_init (char *params, void **config)
{
	MSG_DEBUG(msg_module, "Initilization...");
	if (!params) {
		MSG_ERROR(msg_module, "Missing plugin configuration.");
		return -1;
	}

	// Parse the configuration of the plugin
	struct plugin_config *cfg;
	cfg = config_parse(params);
	if (!cfg) {
		MSG_ERROR(msg_module, "Failed to parse the configuration.");
		return -1;
	}

	// Try to connect to all destinations & start automatic reconnector
	dest_reconnect(cfg->dest_mgr, true);
	if (dest_connector_start(cfg->dest_mgr, cfg->reconn_period)) {
		config_destroy(cfg);
		return -1;
	}

	// Save configuration
	*config = cfg;
	MSG_DEBUG(msg_module, "Initialization completed successfully.");
	return 0;
}

/*
 * Pass IPFIX data with supplemental structures from ipfixcol core into
 * the storage plugin.
 */
int store_packet (void *config, const struct ipfix_message *ipfix_msg,
	const struct ipfix_template_mgr *template_mgr)
{
	(void) template_mgr;
	struct plugin_config *cfg = (struct plugin_config *) config;

	// FIXME: add this function (see known_issues.txt)
	/*
	if (ipfix_msg->source_status == SOURCE_STATUS_CLOSED) {
		if (fwd_remove_source(cfg, ipfix_msg)) {
			MSG_ERROR(msg_module, "Processing of a source termination failed.");
			return 0;
		}
	} else
	*/

	// Process a message
	if (fwd_parse_msg(cfg, ipfix_msg)) {
		MSG_ERROR(msg_module, "Processing of IPFIX message failed.");
		return 0;
	}

	// Send new message(s)
	dest_send(cfg->dest_mgr, cfg->builder_all, cfg->builder_tmplt, cfg->mode);
	return 0;
}

// Announce willing to store currently processing data
int store_now (const void *config)
{
	(void) config;
	return 0;
}

// Storage plugin "destructor"
int storage_close (void **config)
{
	MSG_DEBUG(msg_module, "Closing...");
	struct plugin_config *cfg = (struct plugin_config *) *config;
	config_destroy(cfg);
	*config = NULL;
	return 0;
}

/**@}*/
