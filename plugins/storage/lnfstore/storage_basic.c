/**
 * \file storage_basic.c
 * \author Imrich Stoffa <xstoff02@stud.fit.vutbr.cz>
 * \author Lukas Hutak <xhutak01@stud.fit.vutbr.cz>
 * \author Pavel Krobot <Pavel.Krobot@cesnet.cz>
 * \brief Storage management (source file)
 *
 * Copyright (C) 2015, 2016 CESNET, z.s.p.o.
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
#include <ipfixcol/profiles.h>

#include "storage_basic.h"
#include "translator.h"
#include "bitset.h"
#include <bf_index.h>
#include <libnf.h>
#include <string.h>
#include <libxml/xmlstring.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <stdbool.h>

// Module identification
static const char* msg_module = "lnfstore";



char *create_file_name(struct lnfstore_conf *conf, char **bf_index_fn)
{
	struct tm gm;
	if (gmtime_r(&conf->window_start, &gm) == NULL) {
		MSG_ERROR(msg_module, "Failed to convert time to UTC.");
		*bf_index_fn = NULL;
		return NULL;
	}

	char time_path[13];
	size_t length;

	length = strftime(time_path, sizeof(time_path), "/%Y/%m/%d/", &gm);
	if (length == 0) {
		MSG_ERROR(msg_module, "Failed to fill file path template.");
		*bf_index_fn = NULL;
		return NULL;
	}

	char file_suffix[1024];
	length = strftime(file_suffix, sizeof(file_suffix),
		conf->params->file_suffix, &gm);
	if (length == 0) {
		MSG_ERROR(msg_module, "Failed to fill file path template (suffix).");
		*bf_index_fn = NULL;
		return NULL;
	}

	length = strlen(time_path) + strlen(conf->params->file_prefix)
		+ strlen(file_suffix) + 1;

	char *file_name = (char *) malloc(length * sizeof(char));
	if (!file_name) {
		MSG_ERROR(msg_module, "Unable to allocate memory (%s:%d)",
			__FILE__, __LINE__);
		*bf_index_fn = NULL;
		return NULL;
	}

	snprintf(file_name, length, "%s%s%s", time_path, conf->params->file_prefix,
		file_suffix);

	// Index filename - bloom filter indexing
	if (conf->params->bf.indexing){
		length = strlen(time_path) + strlen(conf->params->bf.file_prefix)
			+ strlen(file_suffix) + 1;

		*bf_index_fn = (char *) malloc(length * sizeof(char));
		if (*bf_index_fn) {
			snprintf(*bf_index_fn, length, "%s%s%s", time_path,
						conf->params->bf.file_prefix, file_suffix);
		} else {
			MSG_WARNING(msg_module, "Unable to allocate memory (%s:%d)",
				__FILE__, __LINE__);
		}
	} else {
		*bf_index_fn = NULL;
	}

	return file_name;
}


int mkdir_hierarchy(const char* path)
{
	struct stat s;
	char* pos = NULL;
	char* realp = NULL;
	unsigned subs_len = 0;
	while ((pos = strchr(path + subs_len + 1, '/')) != NULL ) {
		subs_len = pos - path;
		int status;
		bool failed = false;
		realp = strndup(path, subs_len);
try_again:
		status = stat(realp, &s);
		if (status == -1) {
			if (ENOENT == errno) {
				if (mkdir(realp, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)) {
					if (!failed) {
						failed = true;
						goto try_again;
					}
					MSG_ERROR(msg_module, "Failed to create directory: %s", realp);
					return 1;
				}
			}
		} else if (!S_ISDIR(s.st_mode)) {
			MSG_ERROR(msg_module, "Failed to create directory, %s is file", realp);
			return 2;
		}
		free(realp);
	}
	return 0;
}


int prepare_index(struct lnfstore_index *lnf_index, struct index_params ind_par,
					char *path, char *filename)
{
	size_t size_ifn = strlen(filename) + strlen(path) + 2; // '/' + '\0'
	char *index_fn = (char *) malloc(size_ifn * sizeof(char));
	if (!index_fn) {
		MSG_ERROR(msg_module, "Unable to allocate memory (%s:%d)", __FILE__,
				__LINE__);
		return 1;
	}

	snprintf(index_fn, size_ifn, "%s/%s", path, filename);

/* No need since indexes are in the same folder as data
	if (mkdir_hierarchy(index_fn) != 0) {
		MSG_ERROR(msg_module, "Unable to create directory (%s:%d)",
					__FILE__, __LINE__);
		free(index_fn);
		return 1;
	}
*/

	switch (lnf_index->state){
		case BF_INIT:
		case BF_ERROR:
			if (lnf_index->index) {destroy_index(lnf_index->index);}
			lnf_index->index = create_index();
			if (!lnf_index->index){
				print_last_index_error();
				return 1;
			}
			if (init_index(ind_par, lnf_index->index) != BFI_OK){
				print_last_index_error();
				return 1;
			}
			set_index_filename(lnf_index->index, index_fn);
			break;

//		case BF_IN_PROGRESS:
//		case BF_IN_PROGRESS_FIRST:
//			lnf_index->index = create_index();
//			if (!lnf_index->index){
//				print_last_index_error();
//				return 1;
//			}
//			set_index_filename(lnf_index->index, index_fn);
//			if (load_index(lnf_index->index) != BFI_OK){
//				print_last_index_error();
//				/// TODO musi soubor vzdy existovat?
//				return 1;
//			}
//
//			break;

		case BF_CLOSING_FIRST:
		case BF_CLOSING:
			if (lnf_index->params_changed) {
				destroy_index(lnf_index->index);
				lnf_index->index = create_index();
				if (!lnf_index->index){
					print_last_index_error();
					return 1;
				}
				ind_par.est_item_cnt = lnf_index->unique_item_cnt;
				if (init_index(ind_par, lnf_index->index) != BFI_OK){
					print_last_index_error();
					return 1;
				}
				set_index_filename(lnf_index->index, index_fn);
				lnf_index->params_changed = false;
			} else {
				clear_index(lnf_index->index);
				set_index_filename(lnf_index->index, index_fn);
			}
			break;

		default:
			// should not happen
			MSG_WARNING(msg_module, "Prepare index error (unexpected state: %d) "
						"(%s:%d)", lnf_index->state, __FILE__, __LINE__);
			return 1;
			break;
	}

	return 0;
}


/**
 * \brief Fill new record
 * \param[in] mdata IPFIX record
 * \param[out] record Record
 * \param[in,out] buffer Pointer to temporary buffer
 * \param[in,out] index Pointer to bloom filter index structure
 * \return Number of converted fields
 */
int fill_record(const struct metadata *mdata, lnf_rec_t *record, uint8_t *buffer)
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
		item = bsearch(&key, tr_table, MAX_TABLE , sizeof(struct ipfix_lnf_map),
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


void store_to_file(lnf_file_t *file, struct lnfstore_conf *conf, struct lnfstore_index *lnf_index)
{
	if (!file) {
		return;
	}

	lnf_write(file, conf->rec_ptr);

	// Add index for source and destination IP addresses
	if (conf->params->bf.indexing && lnf_index->state != BF_ERROR){
		const size_t len = 16;
		char buffer [16];

		memset(buffer, 0, len);
		if (lnf_rec_fget(conf->rec_ptr, LNF_FLD_SRCADDR, buffer) != LNF_OK)
		{
			/// TODO legalni situace kdy zaznam nema adresu?
			MSG_WARNING(msg_module, "Unable to get source IP address (lnf get), last data file will not be indexed");
			lnf_index->state = BF_ERROR;
			return;
		}
		// source IP (v4 or v6)
		add_addr_index(lnf_index->index, (const unsigned char *) buffer, len);

		memset(buffer, 0, len);
		if (lnf_rec_fget(conf->rec_ptr, LNF_FLD_DSTADDR, buffer) != LNF_OK)
		{
			/// TODO legalni situace kdy zaznam nema adresu?
			MSG_WARNING(msg_module, "Unable to get destination IP address (lnf get), last data file will not be indexed");
			lnf_index->state = BF_ERROR;
			return;
		}
		// destination IP (v4 or v6)
		add_addr_index(lnf_index->index, (const unsigned char *) buffer, len);
	}
}



// Basic storage only ------------------------------------------------------ >>>
static int open_storage_files(struct lnfstore_conf *conf)
{
	// Prepare filenames
	char *index_file;
	char *file_str = create_file_name(conf, &index_file);
	if (!file_str) {
		return 1;
	}

	// Data file
	size_t file_len = strlen(file_str);
	unsigned int flags = LNF_WRITE;
	if (conf->params->compress) {
		flags |= LNF_COMP;
	}

	size_t size = file_len + strlen(conf->params->storage_path) + 2; // '/' + '\0'
	char *total_path = (char *) malloc(size *sizeof(char));
	if (!total_path) {
		MSG_ERROR(msg_module, "Unable to allocate memory (%s:%d)",
			__FILE__, __LINE__);
		free(file_str);
		return 1;
	}

	snprintf(total_path, size, "%s/%s", conf->params->storage_path, file_str);
	if (mkdir_hierarchy(total_path) != 0) {
		MSG_ERROR(msg_module, "Unable to create directory (%s:%d)",
				__FILE__, __LINE__);
		free(total_path);
		free(file_str);
		return 1;
	}

	int status = lnf_open(&conf->file_ptr, total_path, flags,
		conf->params->file_ident);
	if (status != LNF_OK) {
		MSG_ERROR(msg_module, "Failed to create new file '%s'",
			total_path);
		conf->file_ptr = NULL;
		free(file_str);
		free(total_path);
		return 1;
	}

	free(file_str);
	free(total_path);

	// Index file
	if (conf->params->bf.indexing) {
		if (index_file){
			if (prepare_index(conf->lnf_index, conf->params->bf, conf->params->storage_path,
								index_file) != BFI_OK){
				MSG_WARNING(msg_module, "Unable to prepare index, last data "
							"file will not be indexed");
				conf->lnf_index->state = BF_ERROR;
			} else {
				if (conf->lnf_index->state == BF_ERROR){
					conf->lnf_index->state = BF_IN_PROGRESS;
				}
			}
		} else {
			MSG_WARNING(msg_module, "Unable to get index file name, last data "
						"file will not be indexed");
			conf->lnf_index->state = BF_ERROR;
		}
	}

	return 0;
}

static void close_storage_files(struct lnfstore_conf *conf)
{
	if (!conf->file_ptr) {
		return;
	}

	lnf_close(conf->file_ptr);
	conf->file_ptr = NULL;

	if (conf->lnf_index->state == BF_CLOSING || // standard situation
		conf->lnf_index->state == BF_CLOSING_FIRST || // standard situation (after first window)
		conf->lnf_index->state == BF_CLOSING_LAST) // standard situation (cleaning up)
	{
		if (store_index(conf->lnf_index->index) != BFI_OK){
			print_last_index_error();
			MSG_WARNING(msg_module, "Storing index error,"
						"last data file will not be indexed");
			conf->lnf_index->state = BF_ERROR;
		}
	}
}

static void new_window(time_t now, struct lnfstore_conf *conf)
{
	// Close file
	close_storage_files(conf);

	if (conf->params->bf_index_autosize && conf->params->bf.indexing &&
			(conf->lnf_index->state == BF_CLOSING ||
			conf->lnf_index->state == BF_CLOSING_FIRST)){
		unsigned int act_cnt = stored_item_cnt(conf->lnf_index->index);
		unsigned int coeff = BF_TOL_COEFF(act_cnt);
		if ((act_cnt + (unsigned int)BF_UPPER_TOLERANCE(act_cnt, coeff)) >
			conf->lnf_index->unique_item_cnt ||
			(conf->lnf_index->state == BF_CLOSING &&
				(act_cnt + (unsigned int)BF_LOWER_TOLERANCE(act_cnt, coeff)) <
				conf->lnf_index->unique_item_cnt
			))
		{
			// Higher act_cnt = make bigger bloom filter
			// Lower act_cnt = save space, make smaller bloom filter
			conf->lnf_index->unique_item_cnt = act_cnt * coeff;
			conf->lnf_index->params_changed = true;
		}
	}

	// Update time
	conf->window_start = now;
	if (conf->params->window_align) {
		conf->window_start = (now / conf->params->window_time) *
			conf->params->window_time;
	}

	// Create new file
	open_storage_files(conf);

	MSG_INFO(msg_module, "New time window created.");
}



void cleanup_storage_basic(struct lnfstore_conf *conf){
	if (conf->params->bf.indexing){
		if (conf->lnf_index->state == BF_IN_PROGRESS ||
				conf->lnf_index->state == BF_IN_PROGRESS_FIRST){
			conf->lnf_index->state = BF_CLOSING_LAST;
		}
	}

	close_storage_files(conf);
}
// <<< Basic storage only -------------------------------------------------- <<<


void store_record_basic(const struct metadata *mdata, struct lnfstore_conf *conf)
{
	// Fill record
	lnf_rec_clear(conf->rec_ptr);
	if (fill_record(mdata, conf->rec_ptr, conf->buffer) <= 0) {
		// Nothing to store
		return;
	}

	// Decide whether close files and create new time window
	time_t now = time(NULL);
	if (difftime(now, conf->window_start) > conf->params->window_time) {
		if (conf->lnf_index->state == BF_IN_PROGRESS){
			conf->lnf_index->state = BF_CLOSING;
		} else if (conf->lnf_index->state == BF_IN_PROGRESS_FIRST){
			conf->lnf_index->state = BF_CLOSING_FIRST;
		}
		new_window(now, conf);
		if (conf->lnf_index->state == BF_CLOSING
				|| conf->lnf_index->state == BF_CLOSING_FIRST){
			conf->lnf_index->state = BF_IN_PROGRESS;
		} else if (conf->lnf_index->state == BF_INIT){
			conf->lnf_index->state = BF_IN_PROGRESS_FIRST;
		}
	}

	store_to_file(conf->file_ptr, conf, conf->lnf_index);
}
