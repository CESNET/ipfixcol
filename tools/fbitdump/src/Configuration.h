/**
 * \file Configuration.h
 * \author Petr Velan <petr.velan@cesnet.cz>
 * \brief Header of class for managing user input of fbitdump
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
#include "Resolver.h"


namespace fbitdump {

/** Acceptable command-line parameters */
#define OPTSTRING "hVaA:r:f:n:c:D:Ns:qIM:m::R:o:v:Z:t:"

#define COLUMNS_XML "/usr/share/fbitdump/fbitdump.xml"

/**
 * \brief Class handling command line configuration
 */
class Configuration
{
public:

	/**
	 * \brief Configuration constructor
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
     * \brief Returns column to order by
     *
     * Column might be null when order by is not set by -m option
     *
     * @return Column to order by
     */
    Column* getOrderByColumn();

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
     * \brief This method returns true if user started application with -m option
     *
     * @return true if option "-m" was specified, false otherwise
     */
    bool getOptionm();

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
     * \brief Returns resolver
     *
     * @return object which provides DNS resolving functionality
     */
    Resolver *getResolver();


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
     *
     * Currently version is in VERSION file
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

    /**
     * \brief Check whether argument is a directory
     *
     * @return true if given argument is a directory, false otherwise
     */
    bool isDirectory(std::string dir);

    /**
     * \brief Sanitize path
     *
     * Add slash on the end of the path
     *
     * @return nothing
     */
    void sanitizePath(std::string &path);

    /**
     * \brief Process -M option from getopt()
     *
     * @param tables vector containing names of input directories
     * @param optarg optarg for -M option
     *
     * Local variable "tables", specified in init() method, will contain input
     * directories specified by -M option.
     *
     * @return true, if no error occurred, false otherwise
     */
    bool processMOption(stringVector &tables, const char *optarg);

    /**
     * \brief Process -R option from getopt()
     *
     * @param tables vector containing names of input directories
     * @param optarg optarg for -R option
     *
     * Local variable "tables", specified in init() method, will contain input
     * directories specified by -R option.
     *
     * @return true, if no error occurred, false otherwise
     */
    bool processROption(stringVector &tables, const char *optarg);

    /**
     * \brief Process optional param of -m option
     *
     * Create column to order by
     *
     * @param order name of the column to order by
     */
    void processmOption(std::string order);


    stringVector parts;                /**< Fastbit parts paths to be used*/
    char *appName;                     /**< Application name, parsed from command line args*/
    stringSet aggregateColumnsAliases; /**< Aggregate columns aliases set */
	uint64_t maxRecords;               /**< Limit number of printed records */
	bool plainNumbers;                 /**< Don't convert protocol numbers to strings*/
	bool aggregate;                    /**< Are we in aggreagate mode? */
	bool quiet;                        /**< Don't print header and statistics */
	std::string filter;                /**< User specified filter string */
	std::string format;                /**< Output format*/
	columnVector columns;              /**< Vector of columns to print */
	std::string firstdir;              /**< First table (directory) user wants to work with */
	std::string lastdir;               /**< Last table (directory) user wants to work with */
	bool optm;                         /**< Indicates whether user specified "-m" option or not */
	Column *orderColumn;				   /**< Column specified using -m value, default is %ts */
	std::string timeWindow;            /**< Time window */
	std::string rOptarg;               /**< Optarg for -r option */
	std::string ROptarg;               /**< Optarg for -R option */
	Resolver *resolver;                /**< DNS resolver */
}; /* end of Configuration class */

} /* end of fbitdump namespace */

#endif /* CONFIGURATION_H_ */
