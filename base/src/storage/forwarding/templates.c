/**
 * \file storage/forwarding/templates.c
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Template manager for forwarding plugin (source file)
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

#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "templates.h"

/** Module description */
static const char *msg_module = "forwarding(templates)";

/**
 * \def MEMBER_SIZE(_type_, _member_)
 * \brief Size of \a _member_ in \a _type_ structure/union
 */
#define MEMBER_SIZE(_type_, _member_) sizeof(((_type_ *)0)->_member_)

/** Number of IDs in a group */
#define GROUP_SIZE (256)
/** Number of groups of IDs (i.e. 2^16 / GROUP_SIZE) */
#define GROUP_CNT (256)

/** Default size of an internal array of ODIDs and Flow sources */
#define ARRAY_DEF_SIZE (8)

/** Maximal Template ID, i.e., maximal Record Set ID */
#define FWD_MAX_RECORD_FLOWSET_ID 65535

/**
 * \brief All templates of an Observation Domain ID
 *
 * Maintains specifications of templates in one Observation Domain ID
 * (ODID) shared among multiple flow sources.
 */
typedef struct fwd_odid_s {
	/** Observation Domain ID                                          */
	uint32_t odid;

	/** Number of Options Templates in the ODID                        */
	unsigned int templates_options;
	/** Number of normal Templates in the ODID                         */
	unsigned int templates_normal;

	/** Number of templates prepared for withdrawal                    */
	unsigned int to_remove;

	/**
	 * Sparse array (256 x 256) of templates. Unused parts
	 * (of 256 templates) can be substitute by NULL pointer.
	 */
	fwd_tmplt_t **tmplts[GROUP_CNT];
} fwd_odid_t;

/**
 * \brief Flow source and its remapping of templates
 */
typedef struct fwd_source_s {
	/** Observation Domain ID                                          */
	uint32_t odid;

	/**
	 * Identification of a Flow Source (ONLY for comparison!)
	 * TODO: make it better -> key?
	 */
	const void *src_id;

	/** Maintainer of shared templates among ODID                      */
	fwd_odid_t *maintainer;

	/**
	 * Sparse array (256 x 256 = 2^16) for mapping IDs of private
	 * source templates to new IDs shared among sources with same
	 * Observation Domain ID (ODID). Sparse array i.e. unused parts
	 * (of 256 IDs) can be substitute by NULL pointer.
	 */
	uint16_t *map[GROUP_CNT];
} fwd_source_t;

/**
 * \brief Template manager
 */
struct _fwd_tmplt_mgr {
	/** Array of Observation Domain IDs                                */
	fwd_odid_t **odid_arr;
	/** Number of valid records in the ODID array                      */
	size_t odid_arr_size;
	/** Preallocated size of the ODID array                            */
	size_t odid_arr_max;

	/** Array of Flow Sources                                          */
	fwd_source_t **src_arr;
	/** Number of valid records in the Flow Source array               */
	size_t src_arr_size;
	/** Preallocated size of the Flow Source array                     */
	size_t src_arr_max;
};

/**
 * \brief Type of a template definition
 */
enum TMPLT_DEF_TYPE {
	TYPE_INVALID,       /**< Invalid template                            */
	TYPE_NEW,           /**< New defintion                               */
	TYPE_WITHDRAWAL,    /**< Template withdrawal of a one template       */
	TYPE_WITHDRAWAL_ALL /**< Template withdrawal of all templates        */
};


// Prototypes
static int fwd_odid_template_increment(fwd_odid_t *odid, uint16_t shared_id);
static int fwd_odid_template_decrement(fwd_odid_t *odid, uint16_t shared_id);

/**
 * \brief Get a type of a template defition
 * \param[in] rec Template record
 * \param[in] length Length of the template record
 * \param[in] type Type of the template (#TM_TEMPLATE or #TM_OPTIONS_TEMPLATE)
 * \return The type
 */
static enum TMPLT_DEF_TYPE tmplts_aux_def_type(
	const struct ipfix_template_record *rec, size_t length, int type)
{
	if (length < 4) { // Minimum for a template record
		return TYPE_INVALID;
	}

	// ID of the template
	const uint16_t id = ntohs(rec->template_id);

	if (ntohs(rec->count) == 0) {
		// Withdrawal template
		if (length != 4) { // Size of template withdrawal is always 4 bytes
			return TYPE_INVALID;
		}
		if (type == TM_TEMPLATE && id == IPFIX_TEMPLATE_FLOWSET_ID) {
			return TYPE_WITHDRAWAL_ALL;
		}
		if (type == TM_OPTIONS_TEMPLATE && id == IPFIX_OPTION_FLOWSET_ID) {
			return TYPE_WITHDRAWAL_ALL;
		}
		if (id < IPFIX_MIN_RECORD_FLOWSET_ID) {
			return TYPE_INVALID;
		}
		return TYPE_WITHDRAWAL;
	}

	if (id < IPFIX_MIN_RECORD_FLOWSET_ID) {
		return TYPE_INVALID;
	}

	return TYPE_NEW;
}

/**
 * \brief Create a Flow Source
 * \param[in] src Input info about the Flow Source (ONLY for comparison!)
 * \param[in,out] maintainer Shared ODID maintainer
 * \return Pointer or NULL
 */
static fwd_source_t *fwd_src_create(const struct input_info *src,
	fwd_odid_t *maintainer)
{
	if (maintainer->odid != src->odid) {
		MSG_ERROR(msg_module, "ODID of a Flow source (%" PRIu32 ") and "
			"an ODID maintainer (%" PRIu32 ") missmatch!",
			src->odid, maintainer->odid);
		return NULL;
	}

	fwd_source_t *res = calloc(1, sizeof(*res));
	if (!res) {
		MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)",
			__FILE__, __LINE__);
		return NULL;
	}

	res->maintainer = maintainer;
	res->odid = maintainer->odid;
	res->src_id = (const void *) src;

	MSG_DEBUG(msg_module, "A source with ODID %" PRIu32 " created.", src->odid);
	return res;
}

/**
 * \brief Destroy a Flow Source
 * \param[in,out] src Flow Source
 */
static void fwd_src_destroy(fwd_source_t *src)
{
	if (!src) {
		return;
	}

	uint16_t *group_ptr;

	for (int i = 0; i < GROUP_CNT; ++i) {
		// Delete a group
		group_ptr = src->map[i];

		if (!group_ptr) {
			continue;
		}

		free(group_ptr);
	}

	MSG_DEBUG(msg_module, "A source with ODID %"PRIu32" destroyed.", src->odid);
	free(src);
}

/**
 * \brief Get a mapping of private Template ID to shared Template ID
 * \param[in] src Flow source
 * \param[in] old_id Old Template ID
 * \return If the mapping doesn't exist, returns 0. Otherwise returns new ID
 * (>= 256).
 */
static uint16_t fwd_src_mapping_get(const fwd_source_t *src, uint16_t old_id)
{
	const uint16_t *group = src->map[old_id / GROUP_SIZE];
	if (!group) {
		return 0;
	}

	return group[old_id % GROUP_SIZE];
}

/**
 * \brief Set a mapping of private Template ID to shared Template ID
 * \param[in,out] src Flow source
 * \param[in] old_id Private Template ID
 * \param[in] new_id Shared Template ID
 */
static void fwd_src_mapping_set(fwd_source_t *src, uint16_t old_id,
	uint16_t new_id)
{
	// Set the mapping
	uint16_t *group = src->map[old_id / GROUP_SIZE];
	if (!group) {
		// Not found -> create a new group
		uint16_t **new_grp = &src->map[old_id / GROUP_SIZE];
		*new_grp = calloc(GROUP_SIZE, sizeof(*group));
		if (*new_grp == NULL) {
			MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)",
				__FILE__, __LINE__);
			return;
		}

		group = *new_grp;
	}

	if (fwd_odid_template_increment(src->maintainer, new_id)) {
		MSG_ERROR(msg_module, "Unable to update a number of references to "
			"a template (ID: %" PRIu16 ").", new_id);
	}

	MSG_DEBUG(msg_module, "A new template mapping of a source with ODID "
		"%" PRIu32 " (private: %" PRIu16 " -> share: %" PRIu16 ").",
		src->odid, old_id, new_id);
	group[old_id % GROUP_SIZE] = new_id;
}

/**
 * \brief Remove a mapping of private Template ID to shared Template ID
 * \param[in] src Flow source
 * \param[in] old_id Private Template ID
 */
static void fwd_src_mapping_remove(fwd_source_t *src, uint16_t old_id)
{
	uint16_t shared_id = 0;
	uint16_t *group = src->map[old_id / GROUP_SIZE];
	if (group) {
		shared_id = group[old_id % GROUP_SIZE];
	}

	if (shared_id == 0) {
		MSG_ERROR(msg_module, "Trying to remove a non-existent template "
			"mapping.");
		return;
	}

	if (fwd_odid_template_decrement(src->maintainer, shared_id)) {
		MSG_ERROR(msg_module, "Unable to update a number of references "
			"to a template.");
	}

	MSG_DEBUG(msg_module, "A template mapping of a source with ODID "
		"%" PRIu32 " (private: %" PRIu16 " -> share: %" PRIu16 ") removed.",
		src->odid, old_id, shared_id);
	group[old_id % GROUP_SIZE] = 0; // Set "invalid" value
}

/**
 * \brief Create a template record
 * \param[in] rec Binary template record
 * \param[in] length Length of the binary template record (octets)
 * \param[in] type Type of the template (#TM_TEMPLATE or #TM_OPTIONS_TEMPLATE)
 * \param[in] new_id New Template ID
 * \return Pointer or NULL
 */
static fwd_tmplt_t *fwd_tmplt_create(const struct ipfix_template_record *rec,
	size_t length, int type, uint16_t new_id)
{
	fwd_tmplt_t *res = calloc(1, sizeof(*res));
	if (!res) {
		MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)",
			__FILE__, __LINE__);
		return NULL;
	}

	// Copy the template record
	res->rec = malloc(length);
	if (!res->rec) {
		MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)",
			__FILE__, __LINE__);
		free(res);
		return NULL;
	}

	memcpy(res->rec, rec, length);
	res->rec->template_id = htons(new_id);

	res->length = length;
	res->id = new_id;
	res->ref_cnt = 0;
	res->type = type;

	return res;
}

/**
 * \brief Destroy a template record
 * \param[in,out] tmplt Template record
 */
static void fwd_tmplt_destroy(fwd_tmplt_t *tmplt)
{
	if (!tmplt) {
		return;
	}

	free(tmplt->rec);
	free(tmplt);
}

/**
 * \brief Compare a parsed template record and a binary (raw) template record
 * \warning This function ignores Template IDs
 * \param[in] rec Binary (raw) record
 * \param[in] rec_len Length of the binary template record
 * \param[in] rec_type Type of the template (#TM_TEMPLATE or #TM_OPTIONS_TEMPLATE)
 * \param[in] tmplt Parsed template record
 * \return If template records are equal, returns 0. Otherwise returns non-zero
 * value.
 */
static int fwd_tmplt_cmp(const struct ipfix_template_record *rec,
	size_t rec_len, int rec_type, const fwd_tmplt_t *tmplt)
{
	if (tmplt->type != rec_type) {
		return 1;
	}

	if (tmplt->length != rec_len) {
		return 1;
	}

	// Check the content of templates (skip the field with Template ID)
	const uint8_t *rec1_ptr = (const uint8_t *) rec;
	const uint8_t *rec2_ptr = (const uint8_t *) tmplt->rec;

	const size_t id_size = MEMBER_SIZE(struct ipfix_template_record, template_id);
	rec1_ptr += id_size;
	rec2_ptr += id_size;

	return memcmp(rec1_ptr, rec2_ptr, rec_len - id_size);
}

/**
 * \brief Create new maintainer of an Observation Domain ID
 * \param[in] odid Observation Domain ID (ODID)
 * \return Pointer or NULL
 */
static fwd_odid_t *fwd_odid_create(uint32_t odid)
{
	fwd_odid_t *res = calloc(1, sizeof(*res));
	if (!res) {
		MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)",
			__FILE__, __LINE__);
		return NULL;
	}

	res->odid = odid;

	MSG_DEBUG(msg_module, "ODID %" PRIu32 " maintainer created.", odid);
	return res;
}

/**
 * \brief Destroy an Observation Domaind ID (ODID) maintainer
 * \param[in,out] odid Maintainer
 */
static void fwd_odid_destroy(fwd_odid_t *odid)
{
	if (!odid) {
		return;
	}

	fwd_tmplt_t **group_ptr;
	fwd_tmplt_t *tmplt_ptr;

	for (int group = 0; group < GROUP_CNT; ++group) {
		// Delete a group
		group_ptr = odid->tmplts[group];

		if (!group_ptr) {
			continue;
		}

		for (int i = 0; i < GROUP_SIZE; ++i) {
			// Delete a template
			tmplt_ptr = group_ptr[i];

			if (!tmplt_ptr) {
				continue;
			}

			fwd_tmplt_destroy(tmplt_ptr);
		}

		free(group_ptr);
	}

	MSG_DEBUG(msg_module, "ODID %" PRIu32 " maintainer destroyed.", odid->odid);
	free(odid);
}

/**
 * \brief Get a template definition with defined Template ID
 * \param[in] odid Observation Domain OID (ODID) maintainer
 * \param[in] id Template ID
 * \return If the template exists, returns a pointer. Otherwise returns NULL.
 */
static fwd_tmplt_t *fwd_odid_template_get(const fwd_odid_t *odid, uint16_t id)
{
	fwd_tmplt_t **group = odid->tmplts[id / GROUP_SIZE];
	if (!group) {
		return NULL;
	}

	return group[id % GROUP_SIZE];
}

/**
 * \brief Get an unused Template ID
 * \remark Returned ID still remains as unused.
 * \param[in] odid Observation Domain ID (ODID) maintainer
 * \param[in] hint Preferred ID
 * \return On success returns valid Template ID (>= 256).
 * If all Template IDs are used, returns 0.
 */
static uint16_t fwd_odid_template_unused_id(const fwd_odid_t *odid, uint16_t hint)
{
	// Try to use preferred Template ID
	if (hint >= IPFIX_MIN_RECORD_FLOWSET_ID) {
		if (fwd_odid_template_get(odid, hint) == NULL) {
			return hint;
		}
	}

	// Iterate
	unsigned int new_id = IPFIX_MIN_RECORD_FLOWSET_ID;
	while (new_id <= FWD_MAX_RECORD_FLOWSET_ID) {
		if (fwd_odid_template_get(odid, new_id) == NULL) {
			return new_id;
		}

		++new_id;
	}

	// All IDs are used
	return 0;
}

/**
 * \brief Insert a template definition
 *
 * Find an unused Template ID among shared templates in ODID and then insert
 * the template with this new ID to the ODID maintainer.
 * \param[in,out] odid Observation Domain (ODID) maintainer
 * \param[in] rec Template record
 * \param[in] rec_len Length of the template record
 * \param[in] rec_type Type of the template (#TM_TEMPLATE or #TM_OPTIONS_TEMPLATE)
 * \return On success returns new Template ID assigned to the template (>= 256).
 * Otherwise returns 0.
 */
static uint16_t fwd_odid_template_insert(fwd_odid_t *odid,
	const struct ipfix_template_record *rec, size_t rec_len, int rec_type)
{
	// Create a template record
	const uint16_t old_id = ntohs(rec->template_id);
	const uint16_t new_id = fwd_odid_template_unused_id(odid, old_id);
	if (new_id == 0) {
		MSG_ERROR(msg_module, "Unable to add a new template to the Observation "
			"Domain ID %" PRIu32 ". All available Template IDs are already "
			"used. Some flows will be definitely lost.", odid->odid);
		return 0;
	}

	if (rec_type != TM_TEMPLATE && rec_type != TM_OPTIONS_TEMPLATE) {
		MSG_ERROR(msg_module, "Unable to add a new template to the Observation "
			"Domain ID %" PRIu32 ". Invalid type (%d) of the template .",
			odid->odid, rec_type);
		return 0;
	}

	fwd_tmplt_t *tmplt = fwd_tmplt_create(rec, rec_len, rec_type, new_id);
	if (!tmplt) {
		return 0;
	}

	// Store the template
	fwd_tmplt_t **group = odid->tmplts[new_id / GROUP_SIZE];
	if (!group) {
		// Not found -> create a new group
		fwd_tmplt_t ***new_grp = &odid->tmplts[new_id / GROUP_SIZE];
		*new_grp = calloc(GROUP_SIZE, sizeof(*group));
		if (*new_grp == NULL) {
			MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)",
				__FILE__, __LINE__);
			fwd_tmplt_destroy(tmplt);
			return 0;
		}

		group = *new_grp;
	}

	odid->to_remove++; // There are no references to this template yet
	if (rec_type == TM_TEMPLATE) {
		odid->templates_normal++;
	} else {
		odid->templates_options++;
	}

	MSG_DEBUG(msg_module, "New template (ID: %" PRIu16 ") added to ODID "
		"%" PRIu32 ".", new_id, odid->odid);
	group[new_id % GROUP_SIZE] = tmplt;
	return new_id;
}

/**
 * \brief Remove a template definition
 * \param[in,out] odid Observation Domain (ODID) maintainer
 * \param[in] id Template ID
 */
static void fwd_odid_template_remove(fwd_odid_t *odid, uint16_t id)
{
	fwd_tmplt_t **group = odid->tmplts[id / GROUP_SIZE];
	fwd_tmplt_t *rec = NULL;

	if (group) {
		rec = group[id % GROUP_SIZE];
	}

	if (!rec) {
		MSG_ERROR(msg_module, "Unable to find and delete a shared template "
			"record (ID %" PRIu16 ") from ODID %" PRIu32 ".", id, odid->odid);
		return;
	}

	MSG_DEBUG(msg_module, "A template (ID: %" PRIu16 ") removed from ODID "
		"%" PRIu32 ".", id, odid->odid);

	switch (rec->type) {
	case TM_TEMPLATE:
		odid->templates_normal--;
		break;
	case TM_OPTIONS_TEMPLATE:
		odid->templates_options--;
		break;
	default:
		MSG_ERROR(msg_module, "Internal error: Invalid type (%d) of a template "
			"(ODID: %" PRIu32 ", Template ID: %" PRIu16 ")", rec->type,
			odid->odid, rec->id);
		break;
	}

	fwd_tmplt_destroy(rec);
	group[id % GROUP_SIZE] = NULL;
}

/**
 * \brief Find a raw template record among shared templates in an ODID
 * \param[in] odid Observation Domain ID maintainer
 * \param[in] rec Binary (raw) record
 * \param[in] rec_len Length of the binary template record
 * \param[in] rec_type Type of the template (#TM_TEMPLATE or #TM_OPTIONS_TEMPLATE)
 * \return If the template is not present, returns 0. Otherwise returns
 * new Template ID (>= 256) used in the ODID maintainer.
 */
static uint16_t fwd_odid_template_find(const fwd_odid_t *odid,
	const struct ipfix_template_record *rec, size_t rec_length, int rec_type)
{
	fwd_tmplt_t **group;
	const fwd_tmplt_t *tmplt;

	// For each group
	for (uint16_t i = 0; i < GROUP_CNT; ++i) {
		group = odid->tmplts[i];
		if (!group) {
			continue;
		}

		// For each record
		for (uint16_t j = 0; j < GROUP_SIZE; ++j) {
			tmplt = group[j];
			if (!tmplt) {
				continue;
			}

			// Compare
			if (fwd_tmplt_cmp(rec, rec_length, rec_type, tmplt) == 0) {
				// Match found
				return ((i * GROUP_SIZE) + j);
			}
		}
	}

	// Not found
	return 0;
}

/**
 * \brief Increment a number of references of a template
 * \param[in,out] odid Observation Domain ID maintainer
 * \param[in] shared_id Template ID
 * \return On success returns 0. Otherwise (usually because the template doesn't
 * exist) returns non-zero value.
 */
static int fwd_odid_template_increment(fwd_odid_t *odid, uint16_t shared_id)
{
	fwd_tmplt_t *tmplt = fwd_odid_template_get(odid, shared_id);
	if (!tmplt) {
		return 1;
	}

	if (tmplt->ref_cnt == 0) {
		odid->to_remove--;
	}

	tmplt->ref_cnt++;
	return 0;
}

/**
 * \brief Decrement a number of references of a template
 * \param[in,out] odid Observation Domain ID maintainer
 * \param[in] shared_id Template ID
 * \return On success returns 0. Otherwise (usually because the template doesn't
 * exist) returns non-zero value.
 */
static int fwd_odid_template_decrement(fwd_odid_t *odid, uint16_t shared_id)
{
	fwd_tmplt_t *tmplt = fwd_odid_template_get(odid, shared_id);
	if (!tmplt) {
		return 1;
	}

	if (tmplt->ref_cnt == 0) {
		return 1;
	}

	tmplt->ref_cnt--;
	if (tmplt->ref_cnt == 0) {
		odid->to_remove++;
	}

	return 0;
}

// Create a template manager
fwd_tmplt_mgr_t *tmplts_create()
{
	fwd_tmplt_mgr_t *res = calloc(1, sizeof(*res));
	if (!res) {
		MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)",
			__FILE__, __LINE__);
		return NULL;
	}

	res->odid_arr_size = 0;
	res->odid_arr_max = ARRAY_DEF_SIZE;
	res->odid_arr = calloc(res->odid_arr_max, sizeof(*res->odid_arr));
	if (!res->odid_arr) {
		MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)",
			__FILE__, __LINE__);
		free(res);
		return NULL;
	}

	res->src_arr_size = 0;
	res->src_arr_max = ARRAY_DEF_SIZE;
	res->src_arr = calloc(res->src_arr_max, sizeof(*res->src_arr));
	if (!res->src_arr) {
		MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)",
			__FILE__, __LINE__);
		free(res->odid_arr);
		free(res);
		return NULL;
	}

	return res;
}

// Destroy a template manager
void tmplts_destroy(fwd_tmplt_mgr_t *mgr)
{
	if (!mgr) {
		return;
	}

	// Detele Flow sources (must go first before ODIDs)
	for (size_t i = 0; i < mgr->src_arr_size; ++i) {
		fwd_src_destroy(mgr->src_arr[i]);
	}

	// Delete ODIDs
	for (size_t i = 0; i < mgr->odid_arr_size; ++i) {
		fwd_odid_destroy(mgr->odid_arr[i]);
	}

	free(mgr->odid_arr);
	free(mgr->src_arr);
	free(mgr);
}


/**
 * \brief Add new Observation domain ID (ODID) maintainer to a template manager
 * \warning First, make sure that there is not another ODID maintainer with
 * the same ODID.
 * \param[in,out] mgr Template manager
 * \param[in] odid ODID
 * \return On success returns a pointer to the new ODID maintainer. Otherwise
 * returns NULL.
 */
static fwd_odid_t *tmplts_odid_add(fwd_tmplt_mgr_t *mgr, uint32_t odid)
{
	fwd_odid_t *odid_struct = fwd_odid_create(odid);
	if (!odid_struct) {
		return NULL;
	}

	if (mgr->odid_arr_size == mgr->odid_arr_max) {
		// The array is full -> realloc
		size_t new_max = 2 * mgr->odid_arr_max;
		fwd_odid_t **new_odid;

		new_odid = realloc(mgr->odid_arr, new_max * sizeof(*new_odid));
		if (!new_odid) {
			MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)",
				__FILE__, __LINE__);
			fwd_odid_destroy(odid_struct);
			return NULL;
		}

		mgr->odid_arr = new_odid;
		mgr->odid_arr_max = new_max;
	}

	mgr->odid_arr[mgr->odid_arr_size++] = odid_struct;
	return odid_struct;
}

/**
 * \brief Remove a Observation Domain ID (ODID) maintainer
 * \param[in,out] mgr Template manager
 * \param[in] odid ODID
 */
static void tmplts_odid_remove(fwd_tmplt_mgr_t *mgr, uint32_t odid)
{
	size_t pos;
	for (pos = 0; pos < mgr->odid_arr_size; ++pos) {
		fwd_odid_t *odid_struc = mgr->odid_arr[pos];
		if (odid_struc->odid != odid) {
			continue;
		}

		// Found
		fwd_odid_destroy(odid_struc);
		break;
	}

	if (pos == mgr->odid_arr_size) {
		MSG_ERROR(msg_module, "Unable to find and delete an Observation "
			"Domain ID maintainer for ODID %" PRIu32 ".", odid);
		return;
	}

	// Shift remaining values to the left
	while (pos < mgr->odid_arr_size - 1) {
		mgr->odid_arr[pos] = mgr->odid_arr[pos + 1];
		++pos;
	}

	mgr->odid_arr_size--;
}

/**
 * \brief Find an Observation Domain ID (ODID) maintainer
 * \param[in] mgr Template manager
 * \param[in] odid ODID
 * \return On success returns a pointer. Otherwise returns NULL.
 */
static fwd_odid_t *tmplts_odid_find(fwd_tmplt_mgr_t *mgr, uint32_t odid)
{
	for (size_t i = 0; i < mgr->odid_arr_size; ++i) {
		fwd_odid_t *odid_struct = mgr->odid_arr[i];
		if (odid_struct->odid == odid) {
			return odid_struct;
		}
	}

	return NULL;
}

/**
 * \brief Get Observation Domain ID (ODID) maintainer
 *
 * If the maintainer doesn't exist, create a new one.
 * \param[in] mgr Template manager
 * \param[in] odid ODID
 * \return Should always returns a pointer to the maintainer. On error (usually
 * memory allocation) returns NULL.
 */
static fwd_odid_t *tmplts_odid_get(fwd_tmplt_mgr_t *mgr, uint32_t odid)
{
	fwd_odid_t *res = tmplts_odid_find(mgr, odid);
	if (res) {
		// Found
		return res;
	}

	// Create a new one
	res = tmplts_odid_add(mgr, odid);
	return res;
}

/**
 * \brief Add a new Flow Source to the template manager
 * \param[in,out] mgr Template manager
 * \param[in] src Input info about the Flow source
 * \return On success returns a pointer to the new Flow Source. Otherwise
 * returns NULL.
 */
static fwd_source_t *tmplts_src_add(fwd_tmplt_mgr_t *mgr,
	const struct input_info *src)
{
	// Find an Observation Domain ID maintainer & create the Flow source
	fwd_odid_t *odid = tmplts_odid_get(mgr, src->odid);
	if (!odid) {
		return NULL;
	}

	fwd_source_t *flow_src = fwd_src_create(src, odid);
	if (!flow_src) {
		return NULL;
	}

	if (mgr->src_arr_size == mgr->src_arr_max) {
		// The array is full -> realloc
		size_t new_max = 2 * mgr->src_arr_max;
		fwd_source_t **new_arr;

		new_arr = realloc(mgr->src_arr, new_max * sizeof(*new_arr));
		if (!new_arr) {
			MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)",
				__FILE__, __LINE__);
			fwd_src_destroy(flow_src);
			return NULL;
		}

		mgr->src_arr = new_arr;
		mgr->src_arr_max = new_max;
	}

	mgr->src_arr[mgr->src_arr_size++] = flow_src;
	return flow_src;
}

/**
 * \brief Remove a description of a Flow source
 * \param[in,out] mgr Template manager
 * \param[in] src Input info about the Flow source
 */
static void tmplts_src_remove(fwd_tmplt_mgr_t *mgr, const struct input_info *src)
{
	size_t pos;
	for (pos = 0; pos < mgr->src_arr_size; ++pos) {
		fwd_source_t *flow_src = mgr->src_arr[pos];
		if (flow_src->src_id != (const void *) src) {
			continue;
		}

		fwd_src_destroy(flow_src);
		break;
	}

	if (pos == mgr->src_arr_size) {
		MSG_ERROR(msg_module, "Unable to find and delete a description of "
			"a Flow source.");
		return;
	}

	// Shift remaining values to the left
	while (pos < mgr->src_arr_size - 1) {
		mgr->src_arr[pos] = mgr->src_arr[pos + 1];
		++pos;
	}

	mgr->src_arr_size--;
}

/**
 * \brief Find a description of the Flow source
 * \param[in] mgr Template manager
 * \param[in] src Input info about the Flow source
 * \return On success returns a pointer. Otherwise returns NULL.
 */
static fwd_source_t *tmplts_src_find(fwd_tmplt_mgr_t *mgr,
	const struct input_info *src)
{
	for (size_t i = 0; i < mgr->src_arr_size; ++i) {
		fwd_source_t *flow_src = mgr->src_arr[i];
		if (flow_src->src_id == (const void *) src) {
			return flow_src;
		}
	}

	return NULL;
}

/**
 * \brief Get a description of a Flow source
 *
 * If the Flow source doesn't exist, create a new one.
 * \param[in] mgr Template manager
 * \param[in] info Input info about the Flow source
 * \return Should always returns a pointer to the maintainer. On error (usually
 * memory allocation) returns NULL.
 */
static fwd_source_t *tmplts_src_get(fwd_tmplt_mgr_t *mgr,
	const struct input_info *info)
{
	fwd_source_t *res = tmplts_src_find(mgr, info);
	if (res) {
		// Found
		return res;
	}

	// Create a new one
	res = tmplts_src_add(mgr, info);
	return res;
}


// Get new Set ID of a Data Set
uint16_t tmplts_remap_data_set(fwd_tmplt_mgr_t *mgr,
	const struct input_info *src, const struct ipfix_set_header *header)
{
	// Find Flow source
	fwd_source_t *flow_src = tmplts_src_find(mgr, (const void *) src);
	if (!flow_src) {
		return 0;
	}

	// Find mapping
	const uint16_t private_id = ntohs(header->flowset_id);
	return fwd_src_mapping_get(flow_src, private_id);
}

/**
 * \brief Add a new template from a Flow source
 * \param[in,out] src Flow source
 * \param[in] rec Binary (raw) record
 * \param[in] rec_len Length of the binary template record
 * \param[in] rec_type Type of the template (#TM_TEMPLATE or #TM_OPTIONS_TEMPLATE)
 * \param[out] new_id New ID (only valid when the return code is TMPLT_ACT_PASS)
 * \return Same as tmplts_process_template()
 */
static enum TMPLT_MGR_ACTION fwd_src_add_tmplt(fwd_source_t *src,
	const struct ipfix_template_record *rec, size_t rec_len, int rec_type,
	uint16_t *new_id)
{
	// Check if there is a mapping for this Flow source & template
	const uint16_t private_id = ntohs(rec->template_id);
	uint16_t shared_id = fwd_src_mapping_get(src, private_id);

	if (shared_id > 0) {
		// Template mapping already exists
		fwd_tmplt_t *tmplt = fwd_odid_template_get(src->maintainer, shared_id);
		if (!tmplt) {
			MSG_ERROR(msg_module, "Unable to find a template record "
				"(ID: %" PRIu16 ") used by template mapping.", shared_id);
			return TMPLT_ACT_INVALID;
		}

		// Compare templates
		int res = fwd_tmplt_cmp(rec, rec_len, rec_type, tmplt);
		if (res == 0) {
			// Same templates -> OK
			return TMPLT_ACT_DROP;
		}

		/*
		 * Different templates (usually only for UDP)
		 * Decrement a number of references to the old template and add the new
		 * template with a new mapping
		 */
		fwd_src_mapping_remove(src, private_id);
	}

	// Add the new template with an unknown mapping
	shared_id = fwd_odid_template_find(src->maintainer, rec, rec_len, rec_type);
	if (shared_id > 0) {
		// The same template already exists in the ODID maintainer
		fwd_src_mapping_set(src, private_id, shared_id);
		return TMPLT_ACT_DROP;
	}

	// Store the template
	shared_id = fwd_odid_template_insert(src->maintainer, rec, rec_len, rec_type);
	if (shared_id == 0) {
		return TMPLT_ACT_DROP;
	}

	// Configure the mapping of the template
	fwd_src_mapping_set(src, private_id, shared_id);
	*new_id = shared_id;
	return TMPLT_ACT_PASS;
}

/**
 * \brief Remove all templates of a defined type from a Flow source
 * \param[in,out] src Flow source
 * \param[in] type Type of the templates (#TM_TEMPLATE or #TM_OPTIONS_TEMPLATE)
 */
static void fwd_src_withdraw_type(fwd_source_t *src, int type)
{
	const unsigned int min = IPFIX_MIN_RECORD_FLOWSET_ID;
	const unsigned int max = FWD_MAX_RECORD_FLOWSET_ID;

	if (type != TM_TEMPLATE && type != TM_OPTIONS_TEMPLATE) {
		MSG_ERROR(msg_module, "Trying to delete invalid type of templates.");
		return;
	}

	unsigned int private_id;
	for (private_id = min; private_id <= max; ++private_id) {
		// Get a mapping of the ID
		uint16_t shared_id = fwd_src_mapping_get(src, private_id);
		if (shared_id == 0) {
			continue;
		}

		// Check a type of a template
		const fwd_tmplt_t *tmplt;
		tmplt = fwd_odid_template_get(src->maintainer, shared_id);
		if (!tmplt) {
			MSG_ERROR(msg_module, "Unable to get a reference to a shared "
				"template (ID: %" PRIu16 ").", shared_id);
			continue;
		}

		if (tmplt->type != type) {
			continue;
		}

		fwd_src_mapping_remove(src, private_id);
	}
}

/**
 * \brief Remove a template from a Flow source
 * \param[in,out] src Flow source
 * \param[in] type Type of the template (#TM_TEMPLATE or #TM_OPTIONS_TEMPLATE)
 * \param[in] id Template ID
 */
static void fwd_src_withdraw_id(fwd_source_t *src, int type, uint16_t id)
{
	const uint16_t share_id = fwd_src_mapping_get(src, id);
	if (share_id == 0) {
		MSG_WARNING(msg_module, "Skipping a template withdrawal of an unknown "
			"template ID %" PRIu16 ".", id);
		return;
	}

	const fwd_tmplt_t *tmplt = fwd_odid_template_get(src->maintainer, share_id);
	if (!tmplt) {
		MSG_ERROR(msg_module, "Trying to remove a non-existent template "
			"mapping.");
		return;
	}

	if (type != tmplt->type) {
		MSG_WARNING(msg_module, "Received a template withdrawal of mismatch "
			"types of templates (Template vs. Options Template) for "
			"Template ID %" PRIu16 " from source with ODID %" PRIu32 ".",
			id, src->odid);
	}

	fwd_src_mapping_remove(src, id);
}

// Process a template record
enum TMPLT_MGR_ACTION tmplts_process_template(fwd_tmplt_mgr_t *mgr,
	const struct input_info *src, const struct ipfix_template_record *rec,
	int type, size_t length, uint16_t *new_id)
{
	// Get a description of a flow source
	fwd_source_t *flow_src = tmplts_src_get(mgr, src);
	if (!flow_src) {
		MSG_ERROR(msg_module, "Unable to get an internal representation of "
			"a flow source. A template will be probably lost.");
		return TMPLT_ACT_DROP;
	}

	// Get a type of a template & process it
	enum TMPLT_DEF_TYPE def_type;
	def_type = tmplts_aux_def_type(rec, length, type);

	switch (def_type) {
	case TYPE_NEW:
		return fwd_src_add_tmplt(flow_src, rec, length, type, new_id);

	case TYPE_WITHDRAWAL:
		fwd_src_withdraw_id(flow_src, type, ntohs(rec->template_id));
		return TMPLT_ACT_DROP;

	case TYPE_WITHDRAWAL_ALL:
		fwd_src_withdraw_type(flow_src, type);
		return TMPLT_ACT_DROP;

	case TYPE_INVALID:
		// Drop the template
		MSG_ERROR(msg_module, "Invalid template from a source (ODID: " PRIu32
			") skipped.", src->odid);
		return TMPLT_ACT_INVALID;

	default:
		MSG_ERROR(msg_module, "Unknown type of a template.");
		return TMPLT_ACT_INVALID;
	}
}

// Remove a flow source and its mapping from the template manager
int tmplts_remove_source(fwd_tmplt_mgr_t *mgr, const struct input_info *src)
{
	fwd_source_t *flow_src = tmplts_src_find(mgr, src);
	if (!flow_src) {
		MSG_ERROR(msg_module, "Unable to remove a description and templates of "
			"a flow source (The description of the source is missing).");
		return 1;
	}

	fwd_src_withdraw_type(flow_src, TM_TEMPLATE);
	fwd_src_withdraw_type(flow_src, TM_OPTIONS_TEMPLATE);
	tmplts_src_remove(mgr, src);
	return 0;
}

// Get a Template ID of the template to withdraw
/**
 * This function also frees templates and ODIDs that are no longer required.
 */
uint16_t *tmplts_withdraw_ids(fwd_tmplt_mgr_t *mgr, uint32_t odid, int type,
	uint16_t *cnt)
{
	fwd_odid_t *odid_grp = tmplts_odid_find(mgr, odid);
	if (!odid_grp) {
		// Unknown ODID
		return NULL;
	}

	const uint16_t rec_cnt = odid_grp->to_remove;
	// We use "rec_cnt + 1" to avoid a zero size array allocation
	uint16_t *result = calloc(rec_cnt + 1, sizeof(*result));
	if (!result) {
		MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)",
			__FILE__, __LINE__);
		return NULL;
	}

	if (rec_cnt == 0) {
		// Nothing to withdraw
		*cnt = 0;
		result[0] = 0;
		return result;
	}

	const unsigned int min = IPFIX_MIN_RECORD_FLOWSET_ID;
	const unsigned int max = FWD_MAX_RECORD_FLOWSET_ID;
	fwd_tmplt_t *tmplt;

	unsigned int add_cnt = 0;
	for (unsigned int id = min; id < max && rec_cnt > 0; ++id) {
		tmplt = fwd_odid_template_get(odid_grp, id);
		if (!tmplt) {
			continue;
		}

		if (tmplt->type != type) {
			continue;
		}

		if (tmplt->ref_cnt != 0) {
			continue;
		}

		result[add_cnt++] = id;
		fwd_odid_template_remove(odid_grp, id);
		odid_grp->to_remove--;

		if (odid_grp->to_remove == 0) {
			// Nothing to remove -> stop
			break;
		}
	}

	if (odid_grp->templates_normal == 0 && odid_grp->templates_options == 0) {
		// Remove the ODID maintainer
		tmplts_odid_remove(mgr, odid);
	}

	*cnt = add_cnt;
	result[add_cnt] = 0;
	return result;
}

// Get a pointer to templates defined by an ODID and a type
fwd_tmplt_t **tmplts_get_templates(fwd_tmplt_mgr_t *mgr, uint32_t odid,
	int type, uint16_t *cnt)
{
	const unsigned int min = IPFIX_MIN_RECORD_FLOWSET_ID;
	const unsigned int max = FWD_MAX_RECORD_FLOWSET_ID;

	fwd_odid_t *odid_grp = tmplts_odid_find(mgr, odid);
	if (!odid_grp) {
		// Unknown ODID
		return NULL;
	}

	if (type != TM_TEMPLATE && type != TM_OPTIONS_TEMPLATE) {
		// Invalid type
		return NULL;
	}

	unsigned rec_cnt = (type == TM_TEMPLATE)
			? odid_grp->templates_normal : odid_grp->templates_options;

	// Create and fill an array
	fwd_tmplt_t **result = calloc(rec_cnt + 1, sizeof(*result));
	if (!result) {
		MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)",
			__FILE__, __LINE__);
		return NULL;
	}

	unsigned int add_cnt = 0;
	for (unsigned int id = min; id < max && rec_cnt > 0; ++id) {
		fwd_tmplt_t *tmplt = fwd_odid_template_get(odid_grp, id);
		if (!tmplt) {
			continue;
		}

		if (tmplt->type != type) {
			continue;
		}

		result[add_cnt++] = tmplt;
		if (add_cnt == rec_cnt) {
			// We have all templates -> stop
			break;
		}
	}

	*cnt = add_cnt;
	result[add_cnt] = NULL;
	return result;
}

// Get all Observation Domain IDs (ODIDs)
uint32_t *tmplts_get_odids(const fwd_tmplt_mgr_t *mgr, uint32_t *cnt)
{
	const uint32_t rec_cnt = mgr->odid_arr_size;

	// We use "rec_cnt + 1" to avoid a zero size array allocation
	uint32_t *result = calloc(rec_cnt + 1, sizeof(*result));
	if (!result) {
		MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)",
			__FILE__, __LINE__);
		return NULL;
	}

	unsigned int add_cnt = 0;
	for (unsigned int i = 0; i < mgr->odid_arr_size; ++i) {
		result[add_cnt++] = mgr->odid_arr[i]->odid;
	}

	*cnt = add_cnt;
	return result;
}
