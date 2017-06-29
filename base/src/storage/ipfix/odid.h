/**
 * \file storage/ipfix/odid.h
 * \author Lukas Hutak <xhutak01@stud.fit.vutbr.cz>
 * \brief ODID information (header file)
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

#ifndef ODID_H
#define ODID_H

#include <stdint.h>

/**
 * \brief Information about an ODID
 */
struct odid_record {
	uint32_t odid;        /**< Observation Domain ID          */
	uint32_t seq_num;     /**< The last sequence number       */
	uint32_t export_time; /**< The last export time           */
};

typedef struct odid_s odid_t;

/**
 * \brief Create an ODID info maintainer
 * \return On success returns a pointer to the maintainer. Otherwise (memory
 *   allocation error) returns NULL.
 */
odid_t *
odid_create();

/**
 * \brief Destroy an ODID info maintainer
 * \param[in] odid ODID maintainer
 */
void
odid_destroy(odid_t *odid);

/**
 * \brief Find an ODID record
 *
 * \param[in] odid ODID maintainer
 * \param[in] id   ID
 * \return If the record is not present, the function returns NULL.
 *   Otherwise returns a pointer to the record.
 */
struct odid_record *
odid_find(odid_t *odid, uint32_t id);

/**
 * \brief Get an ODID record
 *
 * If the record is not present, the function will create a new record and all
 * appropriate values will be set to zero.
 * \param[in,out] odid ODID maintainer
 * \param[in]     id   ID
 * \return On success returns a pointer to the record. Otherwise (usually
 *   memory allocation error) returns NULL.
 */
struct odid_record *
odid_get(odid_t *odid, uint32_t id);

/**
 * \brief Remove an ODID record
 *
 * \param[in,out] odid ODID maintainer
 * \param[in]     id   ID
 * \return On success returns 0. Otherwise (the record doesn't exists) returns
 *   a non-zero value.
 */
int
odid_remove(odid_t *odid, uint32_t id);

#endif // ODID_H
