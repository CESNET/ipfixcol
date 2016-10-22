/**
 * \file config_struct.h
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

#ifndef CONFIG_STRUCT_H_
#define CONFIG_STRUCT_H_

extern "C" {
	#include <semaphore.h>
}

#include <string>
#include <map>
#include <vector>

#include "fastbit.h"

struct fastbit_config {
	/* Stores information on templates per flow data source (identified by
	 * exporter IP address and ODID).
	 */
	std::map<std::string, /* Exporter IP address */
			std::map<uint32_t, /* ODID */
					struct od_info>*> *od_infos;

	/* Stores elements that should be indexed */
	std::vector<std::string> *index_en_id;

	/* Directories for index & reorder thread */
	std::vector<std::string> *dirs;

	/* Specifies time interval for storage directory rotation
	 * (0 = no time based rotation)
	 */
	int time_window;

	/* Specifies record count for storage directory rotation
	 * (0 = no record based rotation)
	 */
	int records_window;

	bool new_dir; /* Is current directory a new one? */

	/* Holds type of name strategy for storage directory rotation */
	enum name_type dump_name;

	/* Path to directory where should be storage directory flushed */
	std::string sys_dir;

	/* Current window directory */
	std::string window_dir;

	/* User prefix for storage directory */
	std::string prefix;

	/* Time of last flush, used for time based rotation.
	 * Name is based on start of interval, not its end.
	 */
	time_t last_flush;

	/* Specifies whether stored data should be reordered */
	bool reorder;

	/* Specifies whether indexes should be build during storage.
	 * 0 = no indexes, 1 = index all, 2 = index only marked elements
	 */
	int indexes;

	/* Specifices whether .sp files should be created */
	bool create_sp_files;

	/* Specifies whether field lengths should be taken from template or ipfix-elements.xml */
	bool use_template_field_lengths;

	/* size of buffer (number of values)*/
	int buff_size;

	/* semaphore for index building thread */
	sem_t sem;
};

#endif /* CONFIG_STRUCT_H_ */
