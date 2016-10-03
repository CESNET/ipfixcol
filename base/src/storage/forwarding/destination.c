/**
 * \file storage/forwarding/destination.c
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Destination manager (source file)
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

#include <ipfixcol.h>

#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <time.h>

#include "destination.h"
#include "sender.h"

/** Plugin identification string                                             */
static const char *msg_module = "forwarding(dst)";

/** Default max. size of a packet with templates only                        */
#define DEF_MAX_TMPTL_PACKET_SIZE (512)
/** Default max. count of elements in a group                                */
#define DEF_GRP_SIZE (8)
/** Default size of an array for sequence numbers of ODIDs                   */
#define DEF_SEQ_ARRAY_SIZE (8)

/** \brief Auxiliary array for sequence number per ODID                      */
struct seq_per_odid {
	/** Observation Domain ID (ODID)                                         */
	uint32_t odid;
	/** Sequence number                                                      */
	uint32_t number;
};

/** \brief The destination */
struct dst_client {
	/** Sender                                                               */
	fwd_sender_t *sender;

	struct seq_per_odid *seq_data;/**< An array of sequence numbers per ODID */
	size_t  seq_cnt;              /**< ODIDs in the array                    */
	size_t  seq_max;              /**< Max size of the array                 */
};

/** \brief Group of destinations */
struct group {
	struct dst_client *arr;    /**< Array of destinations                    */
	size_t cnt;                /**< Destinations in the array                */
	size_t max;                /**< Max size of the array                    */
};

/** \brief Main structure for destination manager                            */
struct _fwd_dest {
	/** Index of next destination (for RoundRobin)                           */
	int dst_idx;

	/** Connected destinations                                               */
	struct group *conn;
	/** Disconneted destinations                                             */
	struct group *disconn;
	/** Reconnected destinations (will be moved into connected)              */
	struct group *ready;
	/** Status flag of reconnected dest. (for fast status check for threads) */
	volatile bool ready_empty;

	/**
	 * Mutex for moving senders between groups.
	 * - Main thread moves senders from connected to disconnected (in case
	 *   of destination disconnection) or from ready to connected (in case
	 *   of destination reconnection).
	 * - Connector thread moves reconnected senders from disconnected to
	 *   ready.
	 */
	pthread_mutex_t group_mtx;

	/** Connector thread                                                     */
	pthread_t thread_conn;
	/** Connector thread status (0 = stopped, 1 = running)                   */
	int thread_enabled;
	/** Reconnection period (for connector thread)                           */
	struct timespec reconn_period;

	/** Template manager                                                     */
	fwd_tmplt_mgr_t *tmplt_mgr;
};

/**
 * \brief  Auxiliary structure for templates for reconnected clients
 */
struct tmplts_per_odid {
	/** An Observation Domain ID (ODID)                                      */
	uint32_t      odid;
	/** A packet builder                                                     */
	fwd_bldr_t   *odid_packet;
};

struct tmplts_for_reconnected {
	/** An array of packets with templates for each ODID                     */
	struct tmplts_per_odid *templates;
	/** A size of the array                                                  */
	uint32_t cnt;
};

/**
 * \brief Callback function prototype
 * \param[in,out] sndr Destination
 * \param[in,out] data Processing function data
 * \return True or False
 */
typedef bool (*group_cb_t) (struct dst_client *dst, void *data);

// Prototypes
static enum SEND_STATUS dest_packet_sender(struct dst_client *dst,
	fwd_bldr_t *bldr, bool req_flg);

/**
 * \brief Get an sequence number for defined Observation Domain ID (ODID)
 *
 * Try to find the number for the ODID. When a record doesn't exist,
 * the function will create a new record with nulled sequence number.
 * \param[in,out] dst  Destination
 * \param[in]     odid Observation Domain ID
 * \return On success returns a pointer to the sequence number that can be
 *   modified by external functions. Otherwise returns NULL.
 */
static uint32_t *source_odids_get_seq(struct dst_client *dst, uint32_t odid)
{
	if (dst->seq_data) {
		// Try to find the ODID
		for (size_t i = 0; i < dst->seq_cnt; ++i) {
			struct seq_per_odid *tmp = &dst->seq_data[i];
			if (tmp->odid == odid) {
				return &tmp->number;
			}
		}
	}

	// Not found, add new...
	if (dst->seq_cnt == dst->seq_max) {
		// Array is full, reallocation...
		size_t new_size = (dst->seq_max == 0)
			? DEF_SEQ_ARRAY_SIZE
			: 2 * dst->seq_max;
		struct seq_per_odid *new_arr;

		new_arr = realloc(dst->seq_data, new_size * sizeof(*new_arr));
		if (!new_arr) {
			MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)",
				__FILE__, __LINE__);
			return NULL;
		}

		dst->seq_max = new_size;
		dst->seq_data = new_arr;
	}

	// Fill
	struct seq_per_odid *tmp = &dst->seq_data[dst->seq_cnt++];
	tmp->odid = odid;
	tmp->number = 0;
	return &tmp->number;
}

/**
 * \brief Remove records with sequence numbers for all ODIDs
 * \param[in,out] dst Destination
 */
static void source_odids_remove(struct dst_client *dst)
{
	if (!dst) {
		return;
	}

	free(dst->seq_data);
	dst->seq_data = NULL;
	dst->seq_max = 0;
	dst->seq_cnt = 0;
}

/**
 * \brief Create a group of destinations
 * \return On success returns a pointer. Otherwise returns NULL.
 */
static struct group *group_create()
{
	struct group *grp = calloc(1, sizeof(*grp));
	if (!grp) {
		MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)",
			__FILE__, __LINE__);
		return NULL;
	}

	grp->cnt = 0;
	grp->max = DEF_GRP_SIZE;

	grp->arr = calloc(grp->max, sizeof(*grp->arr));
	if (!grp->arr) {
		MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)",
			__FILE__, __LINE__);
		free(grp);
		return NULL;
	}

	return grp;
}

/**
 * \brief Destroy a group of destinations
 * \warning All present senders will be disconnected and freed
 * \param[in,out] grp Group
 */
static void group_destroy(struct group *grp)
{
	if (!grp) {
		return;
	}

	// Disconnect & destroy all senders
	for (unsigned int i = 0; i < grp->cnt; ++i) {
		sender_destroy(grp->arr[i].sender);
		source_odids_remove(&grp->arr[i]);
	}

	free(grp->arr);
	free(grp);
}

/**
 * \brief Append a new sender to the group of destinations
 * \param[in,out] grp Group
 * \param[in] sndr Sender
 * \return On success returns 0. Otherwise returns non-zero value.
 */
static int group_append(struct group *grp, fwd_sender_t *sndr)
{
	if (!grp || !sndr) {
		return 1;
	}

	if (grp->cnt == grp->max) {
		// The array is full -> realloc
		size_t new_max = 2 * grp->max;
		struct dst_client *new_arr;

		new_arr = realloc(grp->arr, new_max * sizeof(*new_arr));
		if (!new_arr) {
			MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)",
				__FILE__, __LINE__);
			return 1;
		}

		grp->arr = new_arr;
		grp->max = new_max;
	}

	struct dst_client new_client = {sndr, NULL, 0, 0};
	grp->arr[grp->cnt++] = new_client;
	return 0;
}

/**
 * \brief Remove a sender from the group of destinations
 *
 * The order of senders will be preserved. Removed sender will NOT be freed.
 * \param[in,out] grp Group
 * \param[in] sndr Sender
 * \return On success returns 0. Otherwise (not present) returns non-zero value.
 */
static int group_remove(struct group *grp, fwd_sender_t *sndr)
{
	if (!grp || !sndr) {
		return 1;
	}

	// Try to find a given sender
	unsigned int idx;
	for (idx = 0; idx < grp->cnt; ++idx) {
		if (grp->arr[idx].sender != sndr) {
			continue;
		}

		// Found
		source_odids_remove(&grp->arr[idx]);
		break;
	}

	if (idx == grp->cnt) {
		// Not found
		return 1;
	}

	// Shift remaining senders to the left
	while (idx < grp->cnt - 1) {
		grp->arr[idx] = grp->arr[idx + 1];
		++idx;
	}

	grp->cnt--;
	return 0;
}

/**
 * \brief Number of senders in the group
 * \param[in] grp Group
 * \return Count
 */
static size_t group_cnt(const struct group *grp)
{
	return grp->cnt;
}

/**
 * \brief Move selected senders from a source group to a destination group when
 * a condition (defined by a callback function) is satisfied.
 * \param[in,out] src  Source group
 * \param[in,out] dst  Destination group
 * \param[in]     cb   Callback function
 * \param[in,out] data Auxiliary data for callback function (can be NULL)
 * \warning Source and destination MUST be different groups.
 * \return On success returns 0. Otherwise returns non-zero value.
 */
static int group_move(struct group *src, struct group *dst, group_cb_t cb,
	void *data)
{
	if (!src || !dst) {
		return 1;
	}

	if (group_cnt(src) == 0) {
		// Source group is empty -> nothing to do
		return 0;
	}

	unsigned int idx = 0;
	while (idx < src->cnt) {
		struct dst_client *client = &src->arr[idx];
		bool res = cb(client, data);
		if (res == false) {
			// Keep
			++idx;
			continue;
		}

		fwd_sender_t *sender = client->sender;

		// Move the sender
		if (group_remove(src, sender)) {
			MSG_ERROR(msg_module, "Unexpected internal error (%s:%d)",
				__FILE__, __LINE__);
			return 1;
		}

		if (group_append(dst, sender)) {
			MSG_ERROR(msg_module, "Unrecoverable internal error (%s:%d)",
				__FILE__, __LINE__);
			// We can only destroy this sender
			sender_destroy(sender);
			return 1;
		}

		// Do not change idx because on the index is already next value!
	}

	return 0;
}

/**
 * \brief Call a callback function on each sender in a group
 * \param[in] grp Group
 * \param[in] cb Callback function
 * \param[in] data Auxiliary data for callback function (can be NULL).
 */
static void group_foreach(struct group *grp, group_cb_t cb, void *data)
{
	for (unsigned int i = 0; i < grp->cnt; ++i) {
		struct dst_client *client =  &grp->arr[i];
		cb(client, data);
	}
}

/**
 * \brief Try to connect to a destination
 * \param[in] client Sender for the destination
 * \param[in] data Should be NULL (not used)
 * \return On success returns true. Otherwise returns false.
 */
bool aux_connector(struct dst_client *client, void *data)
{
	(void) data;
	return (sender_connect(client->sender) == 0) ? true : false;
}

/**
 * \brief Error message about unsuccessful destination connection
 * \param[in] client Sender for the destination
 * \param[in] data Should be NULL (not used)
 * \return Always returns true
 */
bool aux_conn_failed(struct dst_client *client, void *data)
{
	(void) data;
	MSG_WARNING(msg_module, "Connection to '%s:%s' failed.",
		sender_get_address(client->sender),
		sender_get_port(client->sender));
	return true;
}

/**
 * \brief Success message about succesful destination connection
 * \param[in] client Sender for the destination
 * \param[in] data   Shoud be NULL (not used)
 * \return Always returns true
 */
bool aux_conn_success(struct dst_client *client, void *data)
{
	(void) data;
	MSG_WARNING(msg_module, "Connection to '%s:%s' established.",
		sender_get_address(client->sender),
		sender_get_port(client->sender));
	return true;
}

/**
 * \brief Send templates to a reconnected destination
 * \param[in,out] client Destination
 * \param[in]     data   Packet builders with templates for each ODID
 * \return On success returns true. Otherwise (usually the destination was
 *   disconnected again) returns false.
 */
static bool aux_reconn_tmplt(struct dst_client *client, void *data)
{
	struct tmplts_for_reconnected *tmplts = data;
	enum SEND_STATUS ret_val;

	for (unsigned int i = 0; i < tmplts->cnt; ++i) {
		// Send templates of defined ODID
		fwd_bldr_t *packet_builder = tmplts->templates[i].odid_packet;

		ret_val = dest_packet_sender(client, packet_builder, true);
		if (ret_val == STATUS_OK) {
			continue;
		}

		MSG_WARNING(msg_module, "Reconnection of '%s:%s' failed (unable to "
			"send all templates). A new reconnection attempt to follow.",
			sender_get_address(client->sender),
			sender_get_port(client->sender));

		// Failed to send all templates -> disconnected
		return false;
	}

	aux_conn_success(client, NULL);
	return true;
}

/**
 * \brief Dummy function for moving destinations between groups
 * \param[in] client Unused (can be NULL)
 * \param[in] data   Unused (can be NULL)
 * \return Always returns true
 */
static bool aux_dummy_true(struct dst_client *client, void *data)
{
	(void) client;
	(void) data;
	return true;
}

/**
 * \brief Thread for reconnection of disconnected destinations
 * \param[in,out] arg Configuration of Destination Manager
 * \return Nothing
 */
static void *connector_func(void *arg)
{
	fwd_dest_t *cfg = (fwd_dest_t *) arg;
	int old_state = 0;

	while (1) {
		// Cancellation point (see pthreads(7))
		nanosleep(&cfg->reconn_period, NULL);

		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &old_state);
		dest_reconnect(cfg, false);
		pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &old_state);
	}

	pthread_exit(NULL);
}

/** Enable automatic reconnection of disconnected destination */
int dest_connector_start(fwd_dest_t *dst_mgr, int period)
{
	pthread_attr_t attr;
	int res;

	if (dst_mgr->thread_enabled == 1) {
		MSG_ERROR(msg_module, "Connector start failed (already running).");
		return 1;
	}

	dst_mgr->reconn_period.tv_sec = period / 1000;
	dst_mgr->reconn_period.tv_nsec = (period % 1000) * 1000000L;

	res = pthread_attr_init(&attr);
	if (res != 0) {
		MSG_ERROR(msg_module, "pthread_attr_init() error (%d)", res);
		return 1;
	}

	res = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	if (res != 0) {
		MSG_ERROR(msg_module, "pthread_attr_setdetachstate() error (%d)", res);
		pthread_attr_destroy(&attr);
		return 1;
	}

	res = pthread_create(&dst_mgr->thread_conn, &attr, connector_func, dst_mgr);
	if (res != 0) {
		MSG_ERROR(msg_module, "pthread_create() error (%d)", res);
		pthread_attr_destroy(&attr);
		return 1;
	}

	pthread_attr_destroy(&attr);
	dst_mgr->thread_enabled = 1;
	return 0;
}

/** Disable automatic reconnection of disconnected clients */
int dest_connector_stop(fwd_dest_t *dst_mgr)
{
	int res;
	void *ret_val = NULL;

	if (dst_mgr->thread_enabled == 0) {
		return 0;
	}

	res = pthread_cancel(dst_mgr->thread_conn);
	if (res != 0) {
		MSG_ERROR(msg_module, "pthread_cancel() error (%d)", res);
		return 1;
	}

	res = pthread_join(dst_mgr->thread_conn, &ret_val);
	if (res != 0) {
		MSG_ERROR(msg_module, "pthread_join() error (%d)", res);
		return 1;
	}

	if (ret_val != PTHREAD_CANCELED) {
		MSG_ERROR(msg_module, "Connector thread wasn't cancelled correctly.");
		return 1;
	}

	dst_mgr->thread_enabled = 0;
	return 0;
}

/** Create structure for all remote destinations */
fwd_dest_t *dest_create(fwd_tmplt_mgr_t *tmplt_mgr)
{
	if (!tmplt_mgr) {
		return NULL;
	}

	fwd_dest_t *res = calloc(1, sizeof(*res));
	if (!res) {
		MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)",
			__FILE__, __LINE__);
		return NULL;
	}

	// Prepare mutexes & start "connector" thread
	if (pthread_mutex_init(&res->group_mtx, NULL) != 0) {
		MSG_ERROR(msg_module, "Failed to initialize a mutex.");
		free(res);
		return NULL;
	}

	// Initialize other values
	res->ready_empty = true;
	res->conn = group_create();
	res->disconn = group_create();
	res->ready = group_create();
	res->tmplt_mgr = tmplt_mgr;

	if (!res->conn || !res->disconn || !res->ready) {
		// Initilazation of one or more groups failed
		dest_destroy(res);
		return NULL;
	}

	return res;
}

/** Destroy structure for all remote destinations */
void dest_destroy(fwd_dest_t *dst_mgr)
{
	if (!dst_mgr) {
		return;
	}

	// Stop the connector (if running)
	dest_connector_stop(dst_mgr);

	// Delete all groups & disconnect everyone
	group_destroy(dst_mgr->conn);
	group_destroy(dst_mgr->disconn);
	group_destroy(dst_mgr->ready);
	pthread_mutex_destroy(&dst_mgr->group_mtx);
	free(dst_mgr);
}

/** Add new destination */
int dest_add(fwd_dest_t *dst_mgr, fwd_sender_t *sndr)
{
	if (!dst_mgr || !sndr) {
		return 1;
	}

	pthread_mutex_lock(&dst_mgr->group_mtx);
	int res = group_append(dst_mgr->disconn, sndr);
	pthread_mutex_unlock(&dst_mgr->group_mtx);

	return (res == 0) ? 0 : 1;
}


/** Try to reconnect to all disconnected destinations */
void dest_reconnect(fwd_dest_t *dst_mgr, bool verbose)
{
	pthread_mutex_lock(&dst_mgr->group_mtx);

	/* Move senders from the group of disconnected to the group of ready only
	 * on successful connection  */
	group_move(dst_mgr->disconn, dst_mgr->ready, &aux_connector, NULL);
	if (group_cnt(dst_mgr->ready) > 0) {
		dst_mgr->ready_empty = false;
	}

	if (verbose) {
		// Print warning message about still disconnected destinations
		group_foreach(dst_mgr->disconn, &aux_conn_failed, NULL);
	}

	pthread_mutex_unlock(&dst_mgr->group_mtx);
}

/**
 * \brief Add templates of a given type and Observation Domain ID to a packet
 * \param[in,out] bldr Packet builder
 * \param[in] mgr  Template manager
 * \param[in] odid Observation Domain ID
 * \param[in] type Type of the template (#TM_TEMPLATE or #TM_OPTIONS_TEMPLATE)
 * \return On success returns 0. Otherwise returns non-zero value.
 */
static int dest_templates_aux_fill(fwd_bldr_t *bldr, fwd_tmplt_mgr_t *mgr,
	uint32_t odid, int type)
{
	uint16_t      tmplt_cnt;
	fwd_tmplt_t **tmplt_arr;

	tmplt_arr = tmplts_get_templates(mgr, odid, type, &tmplt_cnt);
	if (!tmplt_arr) {
		return 1;
	}

	for (unsigned int i = 0; i < tmplt_cnt; ++i) {
		const fwd_tmplt_t *tmp = tmplt_arr[i];
		if (bldr_add_template(bldr, tmp->rec, tmp->length, tmp->id, type)) {
			free(tmplt_arr);
			return 1;
		}
	}

	// We can free the array because templates are stored in the Template mngr.
	free(tmplt_arr);
	return 0;
}

/**
 * \brief Free packets with templates for all Observation Domain IDs
 * \param[in,out] templates An array of the templates per ODID
 * \param[in]     cnt       A size of the array
 */
static void dest_templates_free(struct tmplts_per_odid *templates, uint32_t cnt)
{
	for (unsigned int i = 0; i < cnt; ++i) {
		// Destroy all builders
		bldr_destroy(templates[i].odid_packet);
	}

	free(templates);
}

/**
 * \brief Prepare packets with templates for all Observation Domain IDs
 * \param[in] mgr   A template manager
 * \param[in] odids An array of required ODIDs
 * \param[in] cnt   A size of the array
 * \return On success returns an array of the templates per ODID with the same
 *   number of fields as \p cnt. Otherwise returns NULL.
 * \warning The structure MUST be freed by dest_templates_free()
 */
static struct tmplts_per_odid *dest_templates_prepare(
	fwd_tmplt_mgr_t *mgr, const uint32_t *odids, uint32_t cnt)
{
	bool failure = false;

	// Get a list of ODIDs and create packet builders (for each)
	struct tmplts_per_odid *result = calloc(cnt, sizeof(*result));
	if (!result) {
		MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)",
			__FILE__, __LINE__);
		return NULL;
	}

	/*
	 * Prepare the template packets
	 * To each packet we must add time of the packet export. To avoid
	 * possible collisions with future templates (for example
	 * a new template with earlier export time than these templates
	 * created from the Template manager), we will date the template back
	 * 10 minutes (6OO seconds).
	 */
	time_t export_time = time(NULL);
	export_time -= 600; // seconds

	for (unsigned int i = 0; i < cnt; ++i) {
		// Create a packet builder
		fwd_bldr_t *bldr = bldr_create();
		if (!bldr) {
			MSG_ERROR(msg_module, "Failed to create a new packet builder for "
				"templates required by reconnected client(s).");
			failure = true;
			break;
		}

		struct tmplts_per_odid *group = &result[i];
		const uint32_t odid = odids[i];

		group->odid_packet = bldr;
		group->odid = odid;

		bldr_start(bldr, odid, export_time);

		// Add templates to the builder
		if (dest_templates_aux_fill(bldr, mgr, odid, TM_TEMPLATE)) {
			failure = true;
			break;
		}

		if (dest_templates_aux_fill(bldr, mgr, odid, TM_OPTIONS_TEMPLATE)) {
			failure = true;
			break;
		}

		if (bldr_end(bldr, DEF_MAX_TMPTL_PACKET_SIZE)) {
			failure = true;
			break;
		}
	}

	if (failure) {
		dest_templates_free(result, cnt);
		return NULL;
	}

	return result;
}

// Check and move reconnected destinations to connected destinations
void dest_check_reconnected(fwd_dest_t *dst_mgr)
{
	if (dst_mgr->ready_empty) {
		return;
	}

	// Create packets with templates for each Observation Domain ID
	uint32_t *odid_ids;
	uint32_t  odid_cnt;

	odid_ids = tmplts_get_odids(dst_mgr->tmplt_mgr, &odid_cnt);
	if (!odid_ids) {
		MSG_ERROR(msg_module, "Failed to create templates for reconnected "
			"client(s).");
		return;
	}

	if (odid_cnt == 0) {
		// Template manager is empty
		pthread_mutex_lock(&dst_mgr->group_mtx);
		group_move(dst_mgr->ready, dst_mgr->conn, &aux_conn_success, NULL);

		dst_mgr->ready_empty = true;
		pthread_mutex_unlock(&dst_mgr->group_mtx);

		free(odid_ids);
		return;
	}

	struct tmplts_per_odid *templates;
	templates = dest_templates_prepare(dst_mgr->tmplt_mgr, odid_ids, odid_cnt);
	if (!templates) {
		MSG_ERROR(msg_module, "Failed to create templates for reconnected "
			"client(s).");
		free(odid_ids);
		return;
	}

	free(odid_ids); // We don't need it anymore!
	struct tmplts_for_reconnected data = {templates, odid_cnt};

	// Send all templates...
	pthread_mutex_lock(&dst_mgr->group_mtx);
	group_move(dst_mgr->ready, dst_mgr->conn, &aux_reconn_tmplt, &data);

	if (group_cnt(dst_mgr->ready) > 0) {
		// Some destinations are disconnected again
		group_move(dst_mgr->ready, dst_mgr->disconn, &aux_dummy_true, NULL);
	}

	dst_mgr->ready_empty = true;
	pthread_mutex_unlock(&dst_mgr->group_mtx);

	// Delete templates
	dest_templates_free(templates, odid_cnt);
}

/**
 * \brief Send all packets to a destination (auxiliary function)
 * \param[in,out] dst  Destination
 * \param[in,out] bldr Packet builder
 * \param[in] req_flg  Required delivery (usually for packets with templates)
 * \return Status code. When all packets were sent, returns STATUS_OK.
 */
static enum SEND_STATUS dest_packet_sender(struct dst_client *dst,
	fwd_bldr_t *bldr, bool req_flg)
{
	int pkt_cnt = bldr_pkts_cnt(bldr);
	enum SEND_STATUS stat;

	struct iovec *pkt_parts;
	size_t size;
	size_t rec_cnt;

	// Get a sequence number
	uint32_t odid = bldr_pkts_get_odid(bldr);
	uint32_t *seq_num = source_odids_get_seq(dst, odid);
	if (!seq_num) {
		return STATUS_INVALID;
	}

	// Send packets
	for (int i = 0; i < pkt_cnt; ++i) {
		// Prepare the packet
		if (bldr_pkts_iovec(bldr, *seq_num, i, &pkt_parts, &size,
			&rec_cnt)) {
			// Internal Error
			return STATUS_INVALID;
		}

		// Send the packet
		stat = sender_send_parts(dst->sender, pkt_parts, size,
			MODE_NON_BLOCKING, req_flg);

		if (stat != STATUS_OK) {
			return stat;
		}

		*seq_num += rec_cnt;
		req_flg = true; // Remaining packets are always required
	}

	return STATUS_OK;
}

/**
 * \brief Move a sender from a group of connected to a group of disconnected
 * \param[in,out] dst_mgr Destination manager
 * \param[in] sndr Sender
 * \return On success returns 0. Otherwise returns non-zero value.
 */
int dest_move_to_dc(fwd_dest_t *dst_mgr, fwd_sender_t *sndr)
{
	if (group_remove(dst_mgr->conn, sndr)) {
		MSG_ERROR(msg_module, "Unexpected internal error (%s:%d)",
			__FILE__, __LINE__);
		return 1;
	}

	pthread_mutex_lock(&dst_mgr->group_mtx);
	int res = group_append(dst_mgr->disconn, sndr);
	pthread_mutex_unlock(&dst_mgr->group_mtx);

	if (res) {
		MSG_ERROR(msg_module, "Unrecoverable internal error (%s:%d)",
			__FILE__, __LINE__);
		// We can only destroy this sender
		sender_destroy(sndr);
		return 1;
	}

	return 0;
}

/**
 * \brief Send to all destinations except one
 *
 * To send the messages to all destinations just use negative index (e.g. -1)
 * of \p except_idx
 * \param[in,out] dst_mgr Destination manager
 * \param[in,out] bldr Packet builder
 * \param[in] except_idx Exception index (of the destination)
 * \param[in] req_flg Required delivery
 */
static void dest_send_except_one(fwd_dest_t *dst_mgr, fwd_bldr_t *bldr,
	int except_idx, bool req_flg)
{
	enum SEND_STATUS stat;
	unsigned int idx = 0;

	while (idx < dst_mgr->conn->cnt) {
		if (except_idx >= 0 && ((unsigned int) except_idx) == idx) {
			// Skip
			++idx;
			continue;
		}

		// Send data to the destination
		struct dst_client *client = &dst_mgr->conn->arr[idx];
		stat = dest_packet_sender(client, bldr, req_flg);

		switch (stat) {
		case STATUS_BUSY:
			// Destination is busy, but still connected.
			MSG_INFO(msg_module, "Destination '%s:%s' is busy. Unable to "
				"send some flow data.", sender_get_address(client->sender),
				sender_get_port(client->sender));
			// No "break" here!

		case STATUS_OK:
			// Successfull
			++idx;
			break;

		case STATUS_CLOSED:
			// Destination disconnected
			if (dest_move_to_dc(dst_mgr, client->sender)) {
				return;
			}

			if (dst_mgr->conn->cnt == 0) {
				MSG_WARNING(msg_module, "All destination disconnected! Flow "
					"data will be lost.");
			}

			// Do not change idx, because on the index is already next client!
			--except_idx;
			break;

		default:
			MSG_ERROR(msg_module, "Internal error (unknown status of sender: "
				"%d).", (int) stat);
			++idx;
			break;
		}
	}
}

/**
 * \brief Send to the next destination in the order (RoundRobin distribution)
 * \param[in,out] dst_mgr Destination manager
 * \param[in,out] bldr    Packet builder
 * \param[in]     req_flg Required delivery
 * \return On error returns -1. Otherwise returns an index of used destination.
 */
static int dest_send_next(fwd_dest_t *dst_mgr, fwd_bldr_t *bldr, bool req_flg)
{
	int attempts = dst_mgr->conn->cnt;
	int idx = dst_mgr->dst_idx;
	bool stop = false;
	enum SEND_STATUS stat;

	if (attempts == 0) {
		return -1;
	}

	while (!stop && attempts > 0) {
		// Make sure that destination index is not out of range
		idx %= dst_mgr->conn->cnt;

		// Send data to one selected destination
		struct dst_client *client = &dst_mgr->conn->arr[idx];
		stat = dest_packet_sender(client, bldr, req_flg);

		switch (stat) {
		case STATUS_BUSY:
			// Destination is busy, but still connected.
			MSG_DEBUG(msg_module, "Destination '%s:%s' is busy. Sending to "
				"another destination.", sender_get_address(client->sender),
				sender_get_port(client->sender));
			++idx;
			break;

		case STATUS_OK:
			// Successfull
			++idx;
			stop = true;
			break;

		case STATUS_CLOSED:
			// Destination disconnected
			if (dest_move_to_dc(dst_mgr, client->sender)) {
				return -1;
			}

			/*
			 * Do not change idx, because on the index is already next client
			 * or the index is out or range
			 */
			break;

		default:
			MSG_ERROR(msg_module, "Internal error (unexpected 'sender' status: "
				"%d).", (int) stat);
			++idx;
			break;
		}

		--attempts;
	}

	// Store new index !!
	dst_mgr->dst_idx = (dst_mgr->conn->cnt > 0) ? (idx % dst_mgr->conn->cnt) : 0;

	if (!stop) {
		MSG_WARNING(msg_module, "Unable to send flow data (%s).",
			(dst_mgr->conn->cnt > 0)
			? "connected destinations are busy"
			: "all destinations disconnected");
		return -1;
	}

	return idx - 1;
}

/**
 * \brief Send using RoundRobin distribution
 *
 * Templates are send to all destinations.
 * \param[in,out] dst_mgr     Destination manager
 * \param[in,out] bldr_all    Packet builder (all parts)
 * \param[in,out] bldr_tmplts Packet builder (only templates)
 */
static void dest_send_rr(fwd_dest_t *dst_mgr, fwd_bldr_t *bldr_all,
	fwd_bldr_t *bldr_tmplts)
{
	// Are there any templates i.e. required delivery?
	bool new_templates = (bldr_pkts_cnt(bldr_tmplts) > 0);

	if (new_templates) {
		// Send template(s) + data to the destination in the order
		int index = dest_send_next(dst_mgr, bldr_all, true);
		if (index < 0) {
			return;
		}

		// Send template(s) to remaining destination
		dest_send_except_one(dst_mgr, bldr_tmplts, index, true);
	} else {
		// No templates -> send to the next destination in the order
		dest_send_next(dst_mgr, bldr_all, false);
	}
}

/* Send prepared packet(s) */
void dest_send(fwd_dest_t *dst_mgr, fwd_bldr_t *bldr_all,
	fwd_bldr_t *bldr_tmplts, enum DIST_MODE mode)
{
	bool res;

	// Distribution
	switch (mode) {
	case DIST_ALL:
		res = (bldr_pkts_cnt(bldr_tmplts) > 0);
		dest_send_except_one(dst_mgr, bldr_all, -1, res);
		break;

	case DIST_ROUND_ROBIN:
		dest_send_rr(dst_mgr, bldr_all, bldr_tmplts);
		break;

	default:
		MSG_ERROR(msg_module, "Unknown distribution model.");
		break;
	}
}
