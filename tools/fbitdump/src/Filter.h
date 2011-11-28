/**
 * \file Filter.h
 * \author Petr Velan <petr.velan@cesnet.cz>
 * \brief Header of class for filtering
 *
 * Copyright (C) 2011 CESNET, z.s.p.o.
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

#ifndef FILTER_H_
#define FILTER_H_

#include "typedefs.h"
#include "Configuration.h"
#include "Cursor.h"

/**
 * \brief Namespace of the fbitdump utility
 */
namespace fbitdump {

/* Configuration and Cursor classes transitively depend on this header */
class Configuration;
class Cursor;

/**
 * \brief Class managing filter
 *
 * Parses and builds filter
 */
class Filter
{
public:

	/**
	 * \brief Constructor
	 *
	 * @param conf Configuration class
	 */
	Filter(Configuration &conf);

	/**
	 * \brief Initialise the filter
	 *
	 * @return Zero on success, non-zero otherwise
	 */
	int init();

	/**
	 * \brief Build and return filter string for fastbit query
	 *
	 * This should take specified time  windows into consideration
	 *
	 * @return Filter string
	 */
	std::string getFilter();

	/**
	 * \brief Decides whether row specified by cursor matches the filter
	 *
	 * @param cur Cursor pointing to table row
	 * @return True when line specified by cursor matches the filter (passes)
	 */
	bool isValid(Cursor &cur);

private:
	/**
	 * \brief Parse timestamp to number of seconds
	 *
	 * @param str String with text representation of the timestamp
	 * @return Number of seconds in timestamp
	 */
	time_t parseTimestamp(std::string str);

	Configuration &conf; /**< Program configuration */
	std::string filterString; /**< String for fastbit condition */
};

}  // namespace fbitdump


#endif /* FILTER_H_ */
