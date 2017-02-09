/**
 * \file storage_common.c
 * \author Lukas Hutak <xhutak01@stud.fit.vutbr.cz>
 * \brief Common function for storage managers (source file)
 *
 * Copyright (C) 2017 CESNET, z.s.p.o.
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
#include <string.h>
#include "storage_common.h"
#include "translator.h"

files_mgr_t *
stg_common_files_mgr_create(const struct conf_params *params, const char *dir)
{
	struct files_mgr_idx_param *param_idx_ptr = NULL;
	enum FILES_MODE mode = FILES_M_LNF; // We always want to create LNF files

	// Define an output directory and filenames
	struct files_mgr_paths paths;
	memset(&paths, 0, sizeof(paths));

	paths.dir = dir;
	paths.suffix_mask = params->files.suffix;

	paths.prefixes.lnf = params->file_lnf.prefix;
	if (params->file_index.en) {
		paths.prefixes.index = params->file_index.prefix;
	}

	// Define LNF parameters
	struct files_mgr_lnf_param param_lnf;
	memset(&param_lnf, 0, sizeof(param_lnf));

	param_lnf.compress = params->file_lnf.compress;
	param_lnf.ident = params->file_lnf.ident;

	// Define Index parameters
	struct files_mgr_idx_param param_idx;
	memset(&param_idx, 0, sizeof(param_idx));

	if (params->file_index.en) {
		param_idx.autosize = params->file_index.autosize;
		param_idx.item_cnt = params->file_index.est_cnt;
		param_idx.prob = params->file_index.fp_prob;

		param_idx_ptr = &param_idx;
		mode |= FILES_M_INDEX;
	}

	return files_mgr_create(mode, &paths, &param_lnf, param_idx_ptr);
}

int
stg_common_fill_record(const struct metadata *mdata, lnf_rec_t *record,
	uint8_t *buffer)
{
	int added = 0;

	uint16_t offset = 0;
	uint16_t length;

	struct ipfix_template *templ = mdata->record.templ;
	uint8_t *data_record = (uint8_t*) mdata->record.record;

	// Process all fields
	for (uint16_t count = 0, id = 0; count < templ->field_count; ++count, ++id) {
		// Create a key - get Enterprise and Element ID
		struct ipfix_lnf_map key, *item;

		key.ie = templ->fields[id].ie.id;
		length = templ->fields[id].ie.length;
		key.en = 0;

		if (key.ie & 0x8000) {
			key.ie &= 0x7fff;
			key.en = templ->fields[++id].enterprise_number;
		}

		// Find conversion function
		item = bsearch(&key, tr_table, MAX_TABLE, sizeof(struct ipfix_lnf_map),
				ipfix_lnf_map_compare);

		int conv_failed = 1;
		if (item != NULL) {
			// Convert
			conv_failed = item->func(data_record, &offset, &length, buffer, item);
			if (!conv_failed) {
				lnf_rec_fset(record, item->lnf_id, buffer);
				added++;
			}
		}

		if (conv_failed) {
			length = real_length(data_record, &offset, length);
		}

		offset += length;
	}

	return added;
}
