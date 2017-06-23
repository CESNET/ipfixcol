/**
 * \file storage/ipfix/files.h
 * \author Lukas Hutak <xhutak01@stud.fit.vutbr.cz>
 * \brief File manager (header file)
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

#ifndef STORAGE_H
#define STORAGE_H

#include <time.h>
#include <ipfixcol.h>

/*
 * FIXME:
 * Because we don't have ability to properly recognize when a source is
 * connected and disconnected, we are not able to delete ODID records
 * in the internal data structures. When this ability is available, add
 * function to add/remove an ODID or add/remove references to the ODID.
 * Add functions such as files_source_add(...) and files_source_remove(...).
 */

/**
 * \brief Internal type
 */
typedef struct files_s files_t;

/**
 * \brief Create an output file manager
 *
 * \warning An output file will not be created. Call files_new_window() to
 *   create the file, otherwise the manager will drop all packets.
 * \param[in] path_pattern Pattern for output files (path + time specifiers)
 * \return On success returns a pointer to the manager. Otherwise returns NULL.
 */
files_t *
files_create(const char *path_pattern);

/**
 * \brief Destroy an output file manager
 *
 * Close an output file and free all internal structures.
 * \param[in,out] files Manager
 */
void
files_destroy(files_t *files);

/**
 * \brief Create a new time window
 *
 * First, if there is already an output file, it will be closed. Then use
 * \p timestamp and a path pattern to generate a filename of a new file.
 * Finally try to create the file. This function also adds all currently known
 * templates into the file.
 *
 * \note In case of failure, you can try to call this function later again
 *   to create the file later.
 * \param[in,out] files     Files manager
 * \param[in]     timestamp Timestamp of the window start
 * \return On success returns 0. Otherwise returns a non-zero value and the
 *   previous file is closed and the new file is not opened.
 */
int
files_new_window(files_t *files, time_t timestamp);

/**
 * \brief Add a IPFIX message to an output file
 *
 * Because (options) templates are necessary for interpretation flow records
 * in IPFIX files and this manager is able to create a file per window,
 * the function handles all templates in the \p msg and stores them into
 * internal structures. When the (options) templates are processed, the
 * function will store the packet into the current output file.
 *
 * \param[in,out] files Files manager
 * \param[in]     msg   IPFIX message
 * \return On success returns 0. Otherwise (unable to write to the file)
 *   returns a non-zero value.
 */
int
files_add_packet(files_t *files, const struct ipfix_message *msg);


#endif // STORAGE_H
