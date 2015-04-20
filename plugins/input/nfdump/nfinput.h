/**
 * \file nfinput.h
 * \author Michal Kozubik <kozubik@cesnet.cz>
 * \brief Input plugin for nfdump file format.
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

#ifndef NFINPUT_H_
#define NFINPUT_H_

#include <libnfdump.h>
#include <ipfixcol.h>

/**
 * \struct nfinput_config
 * \brief  nfdump input plugin specific "config" structure 
 */
struct nfinput_config {
	int fd;                  /**< file descriptor */
	xmlChar *xml_file;       /**< input file URI from XML configuration 
	                          * file. (e.g.: "file://tmp/ipfix.dump") */
	char *file;              /**< path where to look for IPFIX files. same as
	                          * xml_file, but without 'file:' */
	char *dir;               /**< directory where to look for ipfix files.
	                          * basically it is dirname(file) */
	char *filename;          /**< name of the input file. it may contain asterisk
	                          * (e.g.: "ipfix-2011-03-*.dump) */
	char *file_copy;         /**< auxiliary variable, copy of the "file" for purpose 
	                          * of basename() */
	char **input_files;      /**< list of all input files */
	int findex;              /**< index to the current file in the list of files */
	struct input_info_file_list	*in_info_list;
	struct input_info_file *in_info; /**< info structure about current input file */
        
        nfdump_iter_t iter;
        master_record_t *rec;
};

#endif /* NFINPUT_H_ */