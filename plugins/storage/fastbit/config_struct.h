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
#include "fastbit_table.h"
#include "FlowWatch.h"

class template_table;

struct fastbit_config{
	/*ob_dom stores data buffers based on received templates
	 * (observation ids -> template id -> template data */
	std::map<uint32_t,std::map<uint16_t,template_table*>* > *ob_dom;

	std::map<uint32_t,FlowWatch> *flowWatch;

	/* element info from ipfix-elements.xml is loaded into elements_types
	 * (Enterprise id -> element id -> element storage type) */
	std::map<uint32_t,std::map<uint16_t,enum store_type> > *elements_types;

	/* index_en_id stores elements which should be indexed.
	 * elements stored as column names ('e0id4') */
	std::vector<std::string> *index_en_id;

	/* directories for index & reorder thread */
	std::vector<std::string> *dirs;

	/* time_window specifies time interval for storage directory rotation
	 * (0 = no time based rotation ) */
	int time_window;

	/* records_window specifies record count for storage directory rotation
	 * (0 = no record based rotation ) */
	int records_window;

	bool new_dir; /** Is current directory a new one? */

	/* hold type of name strategy for storage directory rotation */
	enum name_type dump_name;

	/* path to directory where should be storage directory flushed */
	std::string sys_dir;

	/* current window directory */
	std::string window_dir;

	/* user prefix for storage directory */
	std::string prefix;

	/* time of last flush (used for time based rotation,
	 * name is based on start of interval not its end!) */
	time_t last_flush;

	/* specifies if stored data should be reordered */
	int reorder;

	/* specifies if indexes should be build during storage.
	 * 0 = no indexes, 1 = index all, 2 = index only marked elements*/
	int indexes;

	/* size of buffer (number of values)*/
	int buff_size;

	/* semaphore for index building thread */
	sem_t sem;
};

#endif /* CONFIG_STRUCT_H_ */
