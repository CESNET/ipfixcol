/**
 * \file storage/ipfix/files.c
 * \author Lukas Hutak <xhutak01@stud.fit.vutbr.cz>
 * \brief File manager (source file)
 */
/* Copyright (C) 2017 CESNET, z.s.p.o.
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
* This software is provided ``as is``, and any express or implied
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
*/

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <libgen.h>
#include <inttypes.h>
#include <time.h>

#include <ipfixcol.h>

#include "files.h"
#include "ipfix_file.h"
#include "odid.h"

/**
 * \brief Thread-safety strerror that store a message to local variable
 *
 * It defines a local buffer \p var_name of length \p size and fills the error
 * string in the buffer.
 * \param[in] var_name Name of the buffer
 * \param[in] size     Buffer size
 *
 * Example usage:
 * \code{.c}
 *   LOCAL_STRERROR(str, 128);
 *   printf("Error: %s\n", str);
 * \endcode
 */
#define LOCAL_STRERROR(var_name, size) \
	char var_name[size]; \
	var_name[0] = '\0'; \
	strerror_r(errno, var_name, size);

/**
 * \brief Main structure of this module
 */
struct files_s {
	/** Output file pattern                                                 */
	char *pattern;
	/** Template mapper (to solve ID collisions)                            */
	tmapper_t *mapper;
	/** Current output file                                                 */
	FILE *file;
	/** ODID information (the last sequence number and export time)         */
	odid_t *odid_info;
};

/** Auxiliary information about templates that meet the limit */
struct templates_limit {
	uint16_t cnt;  /**< Number of the templates                             */
	size_t size;   /**< Total size of the templates                         */
};


/** Auxiliary structure for parsing (option) templates */
struct files_templates_ctx {
	/** Template mapper                                                */
	tmapper_t *mapper;
	/** Identification of a flow source                                */
	const struct input_info *src_info;
	/** Type of the template (TM_TEMPLATE or TM_OPTIONS_TEMPLATE)      */
	int type;
};

/**
 * \brief Create recursively a directory
 * \param[in] path Full directory path
 * \return On success returns 0. Otherwise returns a non-zero value and errno
 *   is set appropriately.
 */
static int
files_mkdir(const char* path)
{
	// Access rights: RWX for a user and his group. R_X for others.
	const mode_t dir_mode = S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH;
	const char ch_slash = '/';
	bool add_slash = false;

	// Check the parameter
	size_t len = strlen(path);
	if (path[len - 1] != ch_slash) {
		len++; // We have to add another slash
		add_slash = true;
	}

	if (len > PATH_MAX - 1) {
		errno = ENAMETOOLONG;
		return 1;
	}

	// Make a copy
	char *path_cpy = malloc((len + 1) * sizeof(char)); // +1 for '\0'
	if (!path_cpy) {
		errno = ENOMEM;
		return 1;
	}

	strcpy(path_cpy, path);
	if (add_slash) {
		path_cpy[len - 1] = ch_slash;
		path_cpy[len] = '\0';
	}

	struct stat info;
	char *pos;

	// Create directories from the beginning
	for (pos = path_cpy + 1; *pos; pos++) {
		// Find a slash
		if (*pos != ch_slash) {
			continue;
		}

		*pos = '\0'; // Temporarily truncate pathname

		// Check if a subdirectory exists
		if (stat(path_cpy, &info) == 0) {
			// Check if the "info" is about directory
			if (!S_ISDIR(info.st_mode)) {
				free(path_cpy);
				errno = ENOTDIR;
				return 1;
			}

			// Fix the pathname and continue with the next subdirectory
			*pos = ch_slash;
			continue;
		}

		// Errno is filled by stat()
		if (errno != ENOENT) {
			free(path_cpy);
			return 1;
		}

		// Required directory doesn't exist -> create new one
		if (mkdir(path_cpy, dir_mode) != 0 && errno != EEXIST) {
			// Failed (by the way, EEXIST because of race condition i.e.
			// multiple applications creating the same folder)
			free(path_cpy);
			return 1;
		}

		*pos = ch_slash;
	}

	free(path_cpy);
	return 0;
}


/**
 * \brief Create a new file
 *
 * Based on a \p pattern and a \p timestamp, the function will generate a name
 * of the file and it will also try to create it.
 * \warning The file MUST be later closed via standard fclose function.
 * \param[in] pattern   Filename pattern (for strftime)
 * \param[in] timestamp Timestamp
 * \return On success returns a pointer to the file. Otherwise returns NULL.
 */
static FILE *
files_file_create(const char *pattern, time_t timestamp)
{
	char *path;
	char *path_cpy = NULL;
	FILE *file;

	// Generate new filename
	path = (char *) calloc(1, PATH_MAX * sizeof(char));
	if (!path) {
		MSG_ERROR(msg_module, "Unable to allocate memory (%s:%d)",
			__FILE__, __LINE__);
		goto error;
	}

	struct tm gm_timestamp;
	if (gmtime_r(&timestamp, &gm_timestamp) == NULL) {
		MSG_ERROR(msg_module, "Failed to convert current time to UTC", NULL);
		goto error;
	}

	if (strftime(path, PATH_MAX, pattern, &gm_timestamp) == 0) {
		MSG_ERROR(msg_module, "Failed to generate a name of a new output "
			"file based on the pattern. The name is probably too long.", NULL);
		goto error;
	}

	// Try to create an output directory (or make sure it exists)
	path_cpy = strdup(path);
	if (!path_cpy) {
		MSG_ERROR(msg_module, "Unable to allocate memory (%s:%d)",
			__FILE__, __LINE__);
		goto error;
	}

	char *dir = dirname(path_cpy);
	if (files_mkdir(dir)) {
		LOCAL_STRERROR(err_buff, 128);
		MSG_ERROR(msg_module, "Failed to create the directory '%s' (%s).",
			dir, err_buff);
		goto error;
	}

	// Create an output file
	file = fopen(path, "wb");
	if (!file) {
		LOCAL_STRERROR(err_buff, 128);
		MSG_ERROR(msg_module, "Failed to create output file '%s' (%s).",
			path, err_buff);
		goto error;
	}

	free(path);
	free(path_cpy);
	return file;
error:
	free(path);
	free(path_cpy);
	return NULL;
}



/**
 * \brief Get the number of templates that can be send in a message
 *
 * \warning
 *   If the \p limit is too small that no template can fit, the \p limit is
 *   ignored and information about exactly one template is returned.
 *
 * \param[in] arr   Array of the templates
 * \param[in] size  Size of the array
 * \param[in] limit Maximal size of all templates (sum) for the message
 * \return Number and total size (in bytes) of templates
 */
static struct templates_limit
files_templates_limit(tmapper_tmplt_t **arr, uint16_t size, size_t limit)
{
	uint16_t cnt;
	size_t total_size = 0;

	for (cnt = 0; cnt < size; ++cnt) {
		// Get template
		const tmapper_tmplt_t *tmplt = arr[cnt];
		const size_t tmplt_size = tmplt->length;

		// Is it still in the limit
		if (cnt > 0 && total_size + tmplt_size > limit) {
			break;
		}

		total_size += tmplt_size;
	}

	struct templates_limit result = {cnt, total_size};
	return result;
}

/**
 * \brief Write a IPFIX packet header into a file
 *
 * \param[in,out] files    Files manager
 * \param[in]     odid     Observation Domain ID (of the packet)
 * \param[in]     exp_time Export time (of the packet)
 * \param[in]     size     Total size (of the packet)
 * \param[in]     seq_num  Sequence number
 * \return On success returns 0. Otherwise returns a non-zero value.
 */
static int
files_templates_write_header(files_t *files, uint32_t odid, uint32_t exp_time,
	uint16_t size, uint32_t seq_num)
{
	// Prepare the packet header
	struct ipfix_header packet_header;
	packet_header.version = htons(IPFIX_VERSION);
	packet_header.length = htons(size);
	packet_header.export_time = htonl(exp_time);
	packet_header.sequence_number = htonl(seq_num);
	packet_header.observation_domain_id = htonl(odid);

	// Write the header
	if (fwrite(&packet_header, sizeof(packet_header), 1, files->file) != 1) {
		return 1;
	}

	// Success
	return 0;
}

/**
 * \brief Write an IPFIX (options) template set into a file
 * \param[in,out] files     Files manager
 * \param[in]     type      Set type (#TM_TEMPLATE or #TM_OPTIONS_TEMPLATE)
 * \param[in]     array     Array of pointers to the templates
 * \param[in]     array_cnt Number of templates in the array
 * \param[in]     size      Total size of the set (i.e. with the set header)
 * \return On success returns 0. Otherwise returns a non-zero value.
 */
static int
files_templates_write_set(files_t *files, int type, tmapper_tmplt_t **array,
	uint16_t array_cnt, uint16_t size)
{
	// Prepare & write the set header
	uint16_t set_id = (type == TM_TEMPLATE)
		? IPFIX_TEMPLATE_FLOWSET_ID
		: IPFIX_OPTION_FLOWSET_ID;

	struct ipfix_set_header set_header;
	set_header.length = htons(size);
	set_header.flowset_id = htons(set_id);

	if (fwrite(&set_header, sizeof(set_header), 1, files->file) != 1) {
		return 1;
	}

	// Write the templates
	for (uint16_t i = 0; i < array_cnt; ++i) {
		const tmapper_tmplt_t *tmplt = array[i];
		if (fwrite(tmplt->rec, tmplt->length, 1, files->file) != 1) {
			return 1;
		}
	}

	return 0;
}

/**
 * \brief Insert templates of an ODID into a file
 *
 * The function takes all templates stored in a template mapper that belong
 * to the specific \p odid and inserts them into the file as new packets.
 * \param[in,out] files     Files manager
 * \param[in]     odid_info Information about the Observation Domain ID
 * \param[in]     type      Type of the templates (#TM_TEMPLATE or
 *                          #TM_OPTIONS_TEMPLATE)
 * \param[in]     exp_time  Export time (of packets)
 * \return On success returns 0. Otherwise returns a non-zero value and the
 *   output file is probably corrupted.
 */
static int
files_templates_insert(files_t *files, const struct odid_record *odid_info,
	int type)
{
	uint16_t tmplt_cnt;
	tmapper_tmplt_t **tmplt_arr;

	if (type != TM_TEMPLATE && type != TM_OPTIONS_TEMPLATE) {
		// Invalid type
		return 1;
	}

	// Get templates for the ODID
	tmplt_arr = tmapper_get_templates(files->mapper, odid_info->odid, type,
		&tmplt_cnt);
	if (!tmplt_arr) {
		// Failed
		return 1;
	}

	if (tmplt_cnt == 0) {
		free(tmplt_arr);
		return 0;
	}

	// Maximal size of the packet to be generated
	const size_t size_max = 512U;
	// Size of headers (i.e. packet header + template set header)
	const size_t size_headers = sizeof(struct ipfix_header) +
		sizeof(struct ipfix_set_header);
	// Max. size of templates that will be inserted into the packet
	const size_t size_limit = size_max - size_headers;

	uint16_t pos = 0;

	// While there are any unprocessed templates...
	while (pos < tmplt_cnt) {
		tmapper_tmplt_t **ptr = &tmplt_arr[pos];
		// Get the number of templates that will be inserted into a packet
		struct templates_limit res;
		res = files_templates_limit(ptr, tmplt_cnt - pos, size_limit);

		// Write the packet header
		const size_t packet_len = size_headers + res.size;
		if (files_templates_write_header(files, odid_info->odid,
				odid_info->export_time, packet_len, odid_info->seq_num)) {
			free(tmplt_arr);
			return 1;
		}

		// Add the templates to the packet
		const size_t set_len = sizeof(struct ipfix_set_header) + res.size;
		if (files_templates_write_set(files, type, ptr, res.cnt, set_len)) {
			free(tmplt_arr);
			return 1;
		}

		pos += res.cnt;
	}

	// Success
	free(tmplt_arr);
	return 0;
}

/**
 * \brief Add templates from all ODIDs to the current output file.
 * \param[in] file     File manager
 * \return On success returns 0. Otherwise returns a non-zero number and
 *   the file is unchanged.
 */
static int
files_file_add_templates(files_t *files)
{
	if (!files->file) {
		// Empty file
		return 1;
	}

	// Get all ODIDs
	uint32_t  odid_cnt;
	uint32_t *odid_ids;

	odid_ids = tmapper_get_odids(files->mapper, &odid_cnt);
	if (!odid_ids) {
		// Failed to get the list of ODIDs
		MSG_ERROR(msg_module, "Failed to create and add templates to the "
			"current output file", NULL);
		return 1;
	}

	if (odid_cnt == 0) {
		// No templates in the mapper -> skip
		free(odid_ids);
		return 0;
	}

	// For each ODID add packets with templates
	for (uint32_t i = 0; i < odid_cnt; ++i) {
		const uint32_t odid = odid_ids[i];
		const struct odid_record *odid_rec = odid_find(files->odid_info, odid);
		if (!odid_rec) {
			MSG_ERROR(msg_module, "Failed to add templates of ODID %" PRIu32
				" into the new file. Some records will not be interpretable!",
				odid);
			continue;
		}

		if (files_templates_insert(files, odid_rec, TM_TEMPLATE)) {
			free(odid_ids);
			return 1;
		}

		if (files_templates_insert(files, odid_rec, TM_OPTIONS_TEMPLATE)){
			free(odid_ids);
			return 1;
		}
	}

	free(odid_ids);
	return 0;
}

/**
 * \brief Callback function for adding a template to the template manager
 * \param[in]     rec     Template record
 * \param[in]     rec_len Template length
 * \param[in,out] data    Callback context (see the files_templates_ctx
 *   structure)
 */
static void
files_templates_process_template(uint8_t *rec, int rec_len, void *data)
{
	struct files_templates_ctx *ctx = (struct files_templates_ctx *) data;
	const struct ipfix_template_record *tmplt;
	tmplt = (const struct ipfix_template_record *) rec;

	enum TMAPPER_ACTION action;
	uint16_t new_id;

	// We just want to store new templates in the mapper
	action = tmapper_process_template(ctx->mapper, ctx->src_info, tmplt,
		ctx->type, rec_len, &new_id);

	/*
	 * We don't care about the return value. Just check if the template ID is
	 * still the same.
	 */
	if (action != TMAPPER_ACT_PASS) {
		return;
	}

	if (new_id != ntohs(tmplt->template_id)) {
		MSG_ERROR(msg_module, "Multiple sources of the ODID %" PRIu32" caused "
			"template collision i.e. different templates with the same ID "
			"%" PRIu16". The output files will be broken!",
			ctx->src_info->odid, ntohs(tmplt->template_id));
	}
}

/**
 * \brief Add all (options) templates from an IPFIX message to the template
 *   mapper
 * \param[in,out] files Files manager
 * \param[in]     msg   IPFIX message
 */
static void
files_templates_process(files_t *files, const struct ipfix_message *msg)
{
	tset_callback_f func = &files_templates_process_template;
	struct ipfix_template_set *t_set;
	struct ipfix_options_template_set *ot_set;

	// Prepare a context
	struct files_templates_ctx ctx;
	ctx.src_info = msg->input_info;
	ctx.mapper = files->mapper;

	// "Normal" Templates
	ctx.type = TM_TEMPLATE;
	for (size_t idx = 0; (t_set = msg->templ_set[idx]) != NULL; ++idx) {
		// Warning: t_set = const to non-const
		template_set_process_records(t_set, TM_TEMPLATE, func, &ctx);
	}

	// Options templates
	ctx.type = TM_OPTIONS_TEMPLATE;
	for (size_t idx = 0; (ot_set = msg->opt_templ_set[idx]) != NULL; ++idx) {
		// Warning: t_set = const to non-const
		t_set = (struct ipfix_template_set *) ot_set;
		template_set_process_records(t_set, TM_OPTIONS_TEMPLATE, func, &ctx);
	}
}


files_t *
files_create(const char *path_pattern)
{
	// Check parameter(s)
	if (!path_pattern) {
		return NULL;
	}

	// Initialize internal members of this component
	files_t *files = calloc(1, sizeof(*files));
	if (!files) {
		MSG_ERROR(msg_module, "Unable to allocate memory (%s:%d)",
				__FILE__, __LINE__);
		goto error;
	}

	files->pattern = strdup(path_pattern);
	if (!files->pattern) {
		MSG_ERROR(msg_module, "Unable to allocate memory (%s:%d)",
			__FILE__, __LINE__);
		goto error;
	}

	files->mapper = tmapper_create();
	if (!files->mapper) {
		goto error;
	}

	files->odid_info = odid_create();
	if (!files->odid_info) {
		MSG_ERROR(msg_module, "Unable to allocate memory (%s:%d)",
			__FILE__, __LINE__);
		goto error;
	}

	// Success
	return files;

error:
	files_destroy(files);
	return NULL;
}

void
files_destroy(files_t *files)
{
	if (!files) {
		return;
	}

	free(files->pattern);

	if (files->mapper) {
		tmapper_destroy(files->mapper);
	}

	if (files->file) {
		fclose(files->file);
	}

	if (files->odid_info) {
		odid_destroy(files->odid_info);
	}

	free(files);
}

int
files_new_window(files_t *files, time_t timestamp)
{
	// First, close the previous file/window
	if (files->file) {
		fclose(files->file);
		files->file = NULL;
	}

	// Create a new file
	files->file = files_file_create(files->pattern, timestamp);
	if (!files->file) {
		// Failed
		return 1;
	}

	// Add all known templates to the file
	if (files_file_add_templates(files)) {
		// Failed -> close the file
		fclose(files->file);
		files->file = NULL;
		return 1;
	}

	return 0;
}

int
files_add_packet(files_t *files, const struct ipfix_message *msg)
{
	// Add all templates to the template mapper
	if (msg->templ_records_count != 0 || msg->opt_templ_records_count != 0) {
		files_templates_process(files, msg);
	}

	// Store information about the ODID (last export time + sequence number)
	const struct ipfix_header *header = msg->pkt_header;
	const uint32_t odid = ntohl(header->observation_domain_id);
	struct odid_record *rec = odid_get(files->odid_info, odid);
	if (rec) {
		/*
		 * Store the export time of the latest packet and the next sequence
		 * number so we can use them during storing all templates to output
		 * file when the new window is created.
		 */
		const uint16_t rec_in_msg = msg->data_records_count;
		rec->export_time = ntohl(msg->pkt_header->export_time);
		rec->seq_num = ntohl(header->sequence_number) + rec_in_msg;
	}

	if (!files->file) {
		// The file is broken -> do not store
		return 1;
	}
	// Copy the packet to the output file
	const size_t pkt_len = ntohs(msg->pkt_header->length);
	if (fwrite(msg->pkt_header, pkt_len, 1, files->file) != 1) {
		MSG_ERROR(msg_module, "Failed to write a packet into the output file."
			"The file is probably broken and will be closed.", NULL);
		fclose(files->file);
		files->file = NULL;
		return 1;
	}

	return 0;
}