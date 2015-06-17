/**
 * \file fastbit.h
 * \author Petr Kramolis <kramolis@cesnet.cz>
 * \brief
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

#ifndef FASTBIT_H_
#define FASTBIT_H_

/* Get defines from configure */
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

/* We need be64toh macro */
#ifndef HAVE_BE64TOH
# if __BYTE_ORDER == __LITTLE_ENDIAN
#  define be64toh(x) __bswap_64 (x)
# else
#  define be64toh(x) (x)
# endif
#endif

/* size of elements buffer (number of stored elements) */
const unsigned int RESERVED_SPACE = 75000;

/** Identifier to MSG_* macros */
#define msg_module "fastbit storage"

/* this enum specifies types of file naming strategy */
enum name_type{TIME,INCREMENTAL,PREFIX};

/* this enum specifies types of elements */
enum store_type{UINT,INT,BLOB,TEXT,FLOAT,IPv6,UNKNOWN};

#endif /* FASTBIT_H_ */
