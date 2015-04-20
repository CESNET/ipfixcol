/**
 * \file ipfixcol_file.h
 * \author Radek Krejci <rkrejci@cesnet.cz>
 * \brief Structures describing ipfixcol's File Format
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

#ifndef IPFIXCOL_FILE_H_
#define IPFIXCOL_FILE_H_

/*
 * ipfixcol file layout
 * ====================
 * Each data file starts with a file header, which identifies the file as an
 * ipfixcol data file. The magic 16bit integer at the beginning of each file
 * identifies ipfixcol file via value 0xC330. This also guarantees that endian
 * dependant files are read correct.
 *
 * Principal layout of the uncompressed ipfixcol file (FILE_LAYOUT_STANDARD):
 *
 * +--------+---------+-----------+--------+--------+-----+--------+---------+
 * |  File  | Records |  Record   | Record | Record | ... | Record | Bitmaps |
 * | Header |  Index  | Templates |   1    |   2    |     |   n    |         |
 * +--------+---------+-----------+--------+--------+-----+--------+---------+
 *
 *
 * Principal layout of the compressed ipfixcol file (FILE_LAYOUT_COMPRESSED):
 *
 * +--------+---------+-----------+--------+--------+-----+--------+---------+
 * |  File  | Columns |  Record   | Column | Column | ... | Column | Bitmaps |
 * | Header |  Index  | Templates |   1    |   2    |     |   n    |         |
 * +--------+---------+-----------+--------+--------+-----+--------+---------+
 */

/*
 * File Header
 */
typedef struct file_header_s {
	uint16_t	magic;
#define MAGIC 0xC330

	uint16_t	layout;
	uint32_t	flags;

	/*
	 * number of records/columns in the file, i.e. number of items in the index
	 */
	uint32_t	index_size;

	/*
	 * start of records/columns section (real flow data) in the file
	 */
	uint32_t	data_offset;
} file_header_t;

/*
 * Possible file layouts - header must be always the same, but the rest can be
 * changed
 */
typedef enum FILE_LAYOUT {
	FILE_LAYOUT_STANDARD,
	FILE_LAYOUT_COMPRESSED,

	FILE_LAYOUT_COUNT /* number of defined FILE_LAYOUTs */
} FILE_LAYOUT;

/*
 * possible flags - can be used in combination with the file layout, so only
 * some of the flags will be used with the specific file layout
 */
#define	FILE_FLAG_COMPRESS_LZO 0x1



/*
 * Record Templates
 */

/*
 * Record Index (offsets)
 *
 * Record index is an array of uint32_t offsets of each record (since records
 * have variable length. Offsets are relative to the start of the first record.
 * The last offset is the offset of the Bitmaps.
 *
 * The same rules are applied to the Columns Index in case of
 * FILE_LAYOUT_COMPRESSED. Column record size is variable due to a compression.
 */

/*
 * Record Format
 *
 */

/*
 * Columns Format
 */

/*
 * Bitmaps Format
 */

#endif /* IPFIXCOL_FILE_H_ */
