/**
 * \file template_table.h
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

#ifndef _TEMPLATE_TABLE_H_
#define _TEMPLATE_TABLE_H_

extern "C" {
#include <ipfixcol/storage.h>
#include <semaphore.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/stat.h>
}
#include <map>
#include <vector>
#include <iostream>
#include <string>

#include <fastbit/ibis.h>

#include "pugixml.hpp"

#include "fastbit_element.h"
#include "config_struct.h"

class element; // Needed because of circular dependency

uint64_t get_rows_from_part(const char *);

/* For each uniq template is created instance of template_table object.
 * The object is used to parse data records belonging to the template
 * */
class template_table 
{
private:
	uint32_t _buff_size; /* Number of values */
	uint64_t _rows_in_window;
	uint64_t _rows_count;
	uint16_t _template_id;
	uint16_t _min_record_size;
	char _name[10];
	char _orig_name[10]; /**< Saves the _name when renamed due to template collision*/
	bool _new_dir; /**< Remember that the directory is supposed to be new */
	char _index;
	time_t _first_transmission; /**< First transmission of the template. Used to detect changes. */

public:
	/* vector of elements stored in data record (based on template)
	 * element polymorphs to necessary data type
	 */
	std::vector<element *> elements;
	std::vector<element *>::iterator el_it;

	template_table(uint16_t template_id, uint32_t buff_size);
	int rows() {return _rows_count;}
	void rows(int rows_count) {_rows_count = rows_count;}
	std::string name(){return std::string(_name);}
	int parse_template(struct ipfix_template *tmp,struct fastbit_config *config);

	/**
	 * \brief Parse data_set and store its data in memory
	 *
	 * If memory usage is about to exceed memory limit, data is flushed to disk.
	 *
	 * @param data_set ipfixcol data set
	 * @param path path to direcotry where should be data flushed
	 * @param new_dir does the path lead to new directory?
	 */
	int store(ipfix_data_set *data_set, std::string path, bool new_dir);

	int update_part(std::string path);

	/**
	 * \brief Checks whether specified directory exists and creates it if not
	 *
	 * When new direcotry is expected and it already exists,
	 * creates new direcotry suffixed with 'a', 'b', 'c', ...
	 * whatever is the first unused one.
	 *
	 * Changes table property _name
	 *
	 * @param path to the directory to create
	 * @param new_dir does the path lead to new directory?
	 * @return 0 when directory is created, 1 when directory existed,
	 * 	2 when directory cannot be created
	 */
	int dir_check(std::string path, bool new_dir);

	void reset_rows() {
		_rows_in_window = 0;
	}

	/**
	 * \brief Flush data to disk and clean memory
	 *
	 * @param path path to direcotry where should be data flushed
	 */
	void flush(std::string path);

	time_t get_first_transmission() {
		return _first_transmission;
	}

	~template_table();
};

#endif
