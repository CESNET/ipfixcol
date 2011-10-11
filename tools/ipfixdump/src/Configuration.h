/**
 * \file Configuration.h
 * \author Petr Velan <petr.velan@cesnet.cz>
 * \brief Header of class for managing user input of ipfixdump
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

#ifndef CONFIGURATION_H_
#define CONFIGURATION_H_

#include "../3rdparty/pugixml.hpp"
#include "typedefs.h"
#include "Column.h"


namespace ipfixdump {

/** Acceptable command-line parameters */
#define OPTSTRING "hVaA:r:f:n:c:D:Ns:qIM:mR:o:v:Z:t:"

#define COLUMNS_XML "ipfixdump.xml"

/**
 * \brief Class handling command line configuration
 */
class Configuration
{
public:

	/**
	 * \brief Configuration contructor
	 */
	Configuration();

	/**
	 * \brief Parse user input and initialise tables and columns
	 *
	 * @param argc number of arguments
	 * @param argv array of arguments
	 * @return 0 on success, negative value otherwise (program should end)
	 */
    int init(int argc, char *argv[]);

    /**
     * \brief Returns vector of table parts names
     *
     * @return Vector of strings with parts names
     */
    stringVector getPartsNames();

    /**
     * \brief Returns filter string as passed by user
     *
     * @return Strings containing filter from user
     */
    std::string getFilter();

    /**
     * \brief Returns set of fastbit column names containing columns
     * to be used in select clause of aggregation query
     * These columns are used to aggregate by
     *
     * @return stringSet
     */
    stringSet getAggregateColumns();

    /**
     * \brief Returns set of column names containing columns
     * to be used in select clause of aggregation query
     * These columns are used with summary function (sum, min, max)
     *
     * @return stringSet
     */
    stringSet getSummaryColumns();

    /**
     * \brief Returns vector of order by columns
     *
     * @return Vector of order by column names
     */
    stringVector getOrder();

    /**
     * \brief Returns true when option for printing plain numbers was passed
     *
     * @return True when option for printing only plain numbers was passed,
     * else otherwise
     */
    bool getPlainNumbers();

    /**
     * \brief Returns ceil limit of records to print
     * @return Maximum records number to print
     */
    size_t getMaxRecords();

    /**
     * \brief Returns true when record aggregation is required
     *
     * @return True when agregation is on, false otherwise
     */
    bool getAggregate();

    /**
     * \brief Returns true when quiet mode is requested
     * No header and bottom statistics should be printed
     *
     * @return True when quiet mode is requested
     */
    bool getQuiet();

    /**
     * \brief Returns vector of Columns
     * Columns are created based on format string and configuration XML
     *
     * @return Vector of columns
     */
    columnVector& getColumns();

    /**
     * \brief Returns path to configuration XML
     * @return Path to configuration XML
     */
    char* getXmlConfPath();

    /**
     * \brief Returns string with time window start
     *
     * @return string with time window start, empty if none used
     */
    std::string getTimeWindowStart();

    /**
     * \brief Returns string with time window end
     *
     * @return string with time window end, empty if none used
     */
    std::string getTimeWindowEnd();

    /**
     * \brief Class destructor
     */
    ~Configuration();

private:

    /**
     * \brief Prints help to standard output
     */
    void help();

    /**
     * \brief Return string with current version
     * Currently version is #define-d
     *
     * @return string with current version
     */
    std::string version();

    /**
     * \brief Read parts from specified table directories
     *
     * Writes parts to instance variable "parts"
     *
     * @param tables fastbit tables to be used
     * @return 0 on success, negative value on failure
     */
    int searchForTableParts(stringVector &tables);

	/**
	 * \brief Parse output format string
	 * @param format output format string
	 */
	void parseFormat(std::string format);

    stringVector parts; /**< Fastbit parts paths to be used*/
    char *appName; /**< Application name, parsed from command line args*/
    stringSet aggregateColumnsAliases; /**< Aggregate columns aliases set */
	uint64_t maxRecords; /**< Limit number of printed records */
	bool plainNumbers; /**< Don't convert protocol numbers to strings*/
    bool aggregate; /**< Are we in aggreagate mode? */
	bool quiet; /**< Don't print header and statistics */
	std::string filter; /**< User specified filter string */
	stringVector order; /**< Ordering columns aliases */
	std::string format; /**< Output format*/
	columnVector columns; /**< Vector of columns to print */
	std::string timeWindow; /**< Time window for filtering columns */

}; /* end of Configuration class */

} /* end of ipfixdump namespace */

#endif /* CONFIGURATION_H_ */
