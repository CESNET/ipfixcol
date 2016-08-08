/**
 * \file storage/forwarding/packet.h
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Packet builder (header file)
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

#ifndef PACKET_H
#define PACKET_H

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>       // size_t
#include <sys/socket.h>   // struct iovec
#include <ipfixcol.h>

/**
 * \defgroup packet IPFIX packet builder
 * \ingroup forwardingStoragePlugin
 *
 * @{
 */

/**
 * Functions for zero-copy building of new IPFIX packet(s).
 *
 * The packets are automatically build by adding references to their parts
 * i.e whole Data Sets or Template Records (each template must be processed
 * seperately). When all parts are added, one or more packets will be prepared.
 * More than one packet are prepared only when total size of added parts exceeds
 * recommended size per one packet, see the description of bldr_end().
 * Before inserting of first parts use bldr_start(). When all required parts
 * are inserted, you will have to use bldr_end() to prepare (one or more)
 * IPFIX packets.
 *
 * Warning:
 * You MUST make sure that added parts exists for whole time before next use
 * of bldr_start() or none of these functions are used, when at least one added
 * part was freed. Otherwise this can cause a segmentation fault.
 *
 * How to use:
 *   -# bldr_create()
 *   -# bldr_start()
 *   -# repeate N times:
 *      - bldr_add_dataset()
 *      - bldr_add_template()
 *      - bldr_add_template_withdrawal()
 *   -# bldr_end()
 *   -# repeate N times:
 *     - bldr_pkts_cnt()
 *     - bldr_pkts_raw()
 *     - bldr_pkts_iovec()
 *     - bldr_pkts_get_odid()
 *   -# New message? Go to the 2. step
 *   -# bldr_destroy()
 */

/** Prototype */
typedef struct _fwd_bldr fwd_bldr_t;

/**
 * \brief Create a packet builder
 * \return Pointer or NULL
 */
fwd_bldr_t *bldr_create();

/**
 * \brief Destroy a packet builder
 * \param[in,out] pkt Structure of packet builder
 */
void bldr_destroy(fwd_bldr_t *pkt);

/**
 * \brief Start of a new packet(s)
 * \warning After this call, use any bldr_add_... function or bldr_pkts_end()
 * \param[in,out] pkt      Packet builder
 * \param[in]     odid     ODID of the packet
 * \param[in]     exp_time Export time
 */
void bldr_start(fwd_bldr_t *pkt, uint32_t odid, uint32_t exp_time);

/**
 * \brief End of a new packet(s)
 * \warning After this call, do not use any bldr_add_... functions!
 * \param[in,out] pkt Packet builder
 * \param[in]     len Maximum size per packet (just recommendation)
 * \return On success returns 0. Otherwise returns non-zero value and
 *   a content of the builder is NOT defined until call bldr_pkts_start().
 */
int bldr_end(fwd_bldr_t *pkt, uint16_t len);

/**
 * \brief Add a Data set
 *
 * If a Flowset ID of the Data set (pointed by \p data) is different than
 * a value of \p new_id, a header of the set will be replaced with custom
 * one with Flowset ID corresponding to the \p new_id.
 * \param[in,out] pkt Packet builder
 * \param[in] data    Pointer to the Data set
 * \param[in] new_id  New Flowset ID (>= 256)
 * \param[in] rec     Number of data records in the set
 * \return On success returns 0. Otherwise returns non-zero value and the
 *   content of the builder is undefined until calling function bldr_start().
 * \remark A size of the Data set is derived from a header of the set.
 */
int bldr_add_dataset(fwd_bldr_t *pkt, const struct ipfix_data_set *data,
	uint16_t new_id, unsigned int rec);

/**
 * \brief Add a template
 *
 * If a Template ID of the template (pointed by \p data) is different than
 * a value of \p new_id, a header of the template will be replaced with custom
 * one with Template ID corresponding to the \p new_id.
 * \param[in,out] pkt Packet builder
 * \param[in] data   Pointer to a header of the template
 * \param[in] size   Size of the template
 * \param[in] new_id New Template ID (>= 256)
 * \param[in] type   Type of the template (TM_TEMPLATE or TM_OPTIONS_TEMPLATE)
 * \return On success returns 0. Otherwise returns non-zero value and the
 *   content of the builder is undefined until calling function bldr_start().
 */
int bldr_add_template(fwd_bldr_t *pkt, const void *data, size_t size,
	uint16_t new_id, int type);

/**
 * \brief Add a template withdrawal
 * \param[in,out] pkt Packet builder
 * \param[in] id    Template ID
 * \param[in] type  Type of the template (TM_TEMPLATE or TM_OPTIONS_TEMPLATE)
 * \return On success returns 0. Otherwise returns non-zero value and the
 *   content of the builder is undefined until calling function bldr_start().
 */
int bldr_add_template_withdrawal(fwd_bldr_t *pkt, uint16_t id, int type);

/**
 * \brief Get a number of generated packets
 * \param[in] pkt  Packet builder
 * \return On error returns -1. Otherwise returns number of generated packets.
 */
int bldr_pkts_cnt(const fwd_bldr_t *pkt);

/**
 * \brief Get an Observation Domain ID (ODID) of packets
 * \param[in] pkt Packet builder
 * \return ODID
 */
uint32_t bldr_pkts_get_odid(const fwd_bldr_t *pkt);

/**
 * \brief Get a packet defined by index in binary format
 * \param[in,out] pkt Packet builder
 * \param[in]  seq_num    Sequence number of the packet
 * \param[in]  idx        Index of the packet i.e. idx < bldr_pkts_cnt()
 * \param[in]  offset     Drop first N bytes of the packet
 * \param[out] new_packet Generated packet
 * \param[out] size       Size of the generated packet (in octets)
 * \param[out] rec_cnt    Number of data records in the packet
 * \warning Generated packet MUST be free by calling function free
 * \return On success returns 0. Otherwise returns non-zero value.
 */
int bldr_pkts_raw(fwd_bldr_t *pkt, uint32_t seq_num, size_t idx, size_t offset,
	char **new_packet, size_t *size, size_t *rec_cnt);

/**
 * \brief Get a packet defined by index in the format suitable for sendmsg()
 * \param[in,out] pkt  Packet builder
 * \param[in]  seq_num Sequence number of the packet
 * \param[in]  idx     Index of the packet i.e. idx < bldr_pkts_cnt()
 * \param[out] io      Array of packet parts
 * \param[out] size    Number of parts
 * \param[out] rec_cnt Number of data records in the packet
 * \return On success returns 0. Otherwise returns non-zero value.
 * \warning Only one packet can be generated and used by a user at one time,
 *   because \p io points to internal structures that changes by calling
 *   this function.
 */
int bldr_pkts_iovec(fwd_bldr_t *pkt, uint32_t seq_num, size_t idx,
	struct iovec **io, size_t *size, size_t *rec_cnt);

#endif // PACKET_H

/**@}*/
