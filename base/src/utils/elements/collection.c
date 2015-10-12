/**
 * \file collection.c
 * \author Lukas Hutak <xhutak01@stud.fit.vutbr.cz>
 * \brief Functions for handling definitions of IPFIX elements
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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#include <ipfixcol.h>
#include "collection.h"
#include "element.h"


/** Maximal number of collections */
#define ELEM_COLL_MAX (8)
/** Identification of empty collection */
#define ELEM_COLL_EMPTY (-1)

/* Global name of this compoment */
static const char *msg_module = "elements_collection";

/** Path to the current configuration file */
static char *current_path = NULL;
/** Timestamp of the file with description of IPFIX elements */
static time_t last_change = 0;

/** Buffer of old collections and current collection */
static struct elem_groups *collections[ELEM_COLL_MAX] = {NULL};
/** Index of active collection */
static int collection_id = ELEM_COLL_EMPTY;

/**
 * \brief Load new collection
 *
 * \param[in] path Path to the configuration file
 * \return If the file is unchanged, it'll return 0. If the file changed and
 * new configuration is reloaded, returns positive value. Otherwise returns
 * negative value.
 */
int elem_coll_reload(const char *path)
{
	if (!path) {
		MSG_ERROR(msg_module, "File with a description of IPFIX elements is not"
			" specified.");
		return -1;
	}
	
	// Open file
	int fd = open(path, O_RDONLY);
	if (fd == -1) {
		MSG_ERROR(msg_module, "Unable to open configuration file '%s' (%s)",
			path, strerror(errno));
		return -1;
	}

	// Get status of configuration file
	struct stat st;
	if (fstat(fd, &st) == -1) {
		MSG_WARNING(msg_module, "Unable to get status of configuration file "
			"'%s' (%s)", path, strerror(errno));
		close(fd);
		return -1;
	}

	if (collection_id != ELEM_COLL_EMPTY) {
		// Compare timestamps of new and old file
		if (last_change == st.st_mtim.tv_sec && !strcmp(path, current_path)) {
			// Nothing changed
			close(fd);
			return 0;
		}
	}
	
	// Load elements
	struct elem_groups *new_desc = elements_init();
	if (!new_desc) {
		close(fd);
		return -1;
	}
	
	if (elements_load(fd, new_desc) != 0) {
		MSG_ERROR(msg_module, "Failed to parse a description of IPFIX elements "
			"in the file '%s'", path);
		elements_destroy(new_desc);
		close(fd);
		return -1;
	}
	close(fd);
	
	if (!current_path || !strcmp(path, current_path)) {
		// Save the path to the new configuration
		if (current_path) {
			free(current_path);
		}
		
		char *new_path = strdup(path);
		if (!new_path) {
			MSG_ERROR(msg_module, "strdup failed: %s", strerror(errno));
			elements_destroy(new_desc);
			return -1;
		}
		
		current_path = new_path;
	}
	
	// Store a timestamp of last file change
	last_change = st.st_mtim.tv_sec;
	
	// Add/replace old description of elements
	collection_id = (collection_id + 1) % ELEM_COLL_MAX;
	struct elem_groups **desc_ptr = &collections[collection_id];
	
	if (*desc_ptr != NULL) {
		elements_destroy(*desc_ptr);
	}
	
	*desc_ptr = new_desc;
	return 1;
}

/**
 * \brief Destroy all collections
 */
void elem_coll_destroy()
{
	collection_id = ELEM_COLL_EMPTY;	
	
	for (int i = 0; i < ELEM_COLL_MAX; ++i) {
		if (collections[i] != NULL) {
			elements_destroy(collections[i]);
			collections[i] = NULL;
		}
	}
	
	if (current_path) {
		free(current_path);
		current_path = NULL;
	}
}

/**
 * \brief Get a pointer to current collection
 * \return Pointer to the collection or NULL.
 */
const struct elem_groups *elem_coll_get()
{
	if (collection_id == ELEM_COLL_EMPTY) {
		return NULL;
	}
	
	return collections[collection_id];
}


