/**
 * \file reader.h
 * \author Michal Kozubik <kozubik@cesnet.cz>
 * \brief Functions for reading IPFIX file
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

#ifndef READER_H
#define	READER_H

enum read_status {
    READ_EOF,
    READ_OK,
    READ_ERROR,
};

/**
 * \brief Read packet from file
 * 
 * @param fd file descriptor
 * @param status read status (READ_ERROR, EOF, ...)
 * @return packet
 */
char *read_packet(int fd, int *status);

/**
 * \brief Read packets from IPFIX file and store them into memory
 * 
 * @param input path to IPFIX file
 * @return array with packets
 */
char **read_packets(char *input);

/**
 * \brief Free allocated memory for packets
 * 
 * @param packets
 */
void free_packets(char **packets);

/**
 * \brief Read file
 * 
 * @param input input path to file
 * @param fsize file size
 * @return whole file content
 */
char *read_file(char *input, long *fsize);

#endif	/* READER_H */

