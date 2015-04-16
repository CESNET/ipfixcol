/**
 * \file Printer.h
 * \author Petr Velan <petr.velan@cesnet.cz>
 * \brief Header of class for printing fastbit tables
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

#ifndef PRINTER_H_
#define PRINTER_H_

#include "typedefs.h"
#include "Configuration.h"
#include "TableManager.h"

/**
 * \brief Namespace of the fbitdump utility
 */
namespace fbitdump {

/**
 * \brief Class printing tables
 *
 * Handles output formatting
 * Retrieves statistics and prints them on demand
 * Handles conversion of timestamps, IP addresses, etc.
 */
class Printer
{
public:

	/**
	 * \brief Constructor
	 *
	 * @param out ostream to print to
	 * @param conf configuration class
	 */
	Printer(std::ostream &out, Configuration &conf);

	/**
	 * \brief Prints output in specified format
	 *
	 * @param tm TableManager of tables to print
	 * @return true on success, false otherwise
	 */
	bool print(TableManager &tm);

private:

	/**
	 * \brief print one row
	 *
	 * @param cur cursor poiting to the row
	 */
	void printRow(const Cursor *cur) const;

	/**
	 * \brief Print table header
	 */
	void printHeader() const;

	/**
	 * \brief Print short summary after flows output
	 * @param numPrinted Number of printed rows
	 */
	void printFooter(uint64_t numPrinted) const;

	/**
	 * \brief Return printable value of column on row specified by cursor
	 *
	 * Applies semantics and other formatting requirements
	 *
	 * @param col Column to print
	 * @param cur Cursor with values to print
	 * @return String to print
	 */
	const std::string printValue(const Column *col, const Cursor *cur) const;

	/**
	 * \brief Print formatted IPv4 address
	 *
	 * @param address integer format address
	 * @return String with printable address
	 */
//	const char * printIPv4(union plugin_arg * val);

	/**
	 * \brief Print formatted IPv6 address
	 *
	 * @param part1 first half of ipv6 address
	 * @param part2 second half of ipv6 address
	 * @return String with printable address
	 */
//	const char * printIPv6(union plugin_arg * val);

	/**
	 * \brief Print formatted timestamp
	 *
	 * This is called from printTimestamp[32|64]
	 *
	 * @param tm datetime structure
	 * @param msec miliseconds
	 * @return String with printable timestamp
	 */
//	const char * printTimestamp(union plugin_arg * val);

	/**
	 * \brief Print formatted timestamp from seconds
	 *
	 * @param timestamp uint32_t number of seconds
	 * @return String with printable timestamp
	 */
//	const char * printTimestamp32(union plugin_arg * val);

	/**
	 * \brief Print formatted timestamp from miliseconds
	 *
	 * @param timestamp uint64_t number of miliseconds
	 * @return String with printable timestamp
	 */
//	const char * printTimestamp64(union plugin_arg * val);

	/**
	 * \brief Print formatted TCP flags
	 *
	 * @param flags unsigned char value
	 * @return String with printable flags
	 */
//	const char * printTCPFlags(unsigned char flags);

	/**
	 * \brief Print duration as decimal number
	 *
	 * @param duration
	 * @return String with duration
	 */
//	const char * printDuration(union plugin_arg * val);

//	const char * printProtocol(union plugin_arg * val);

	std::ostream &out; /**< Stream to write to */
	Configuration &conf; /**< program configuration */
	TableManager *tableManager; /**< table manager used in print function (provide easy access to others) */
	const int percentageWidth;	/**< Width of percentage printed after values in statistics mode */
};

}  // namespace fbitdump


#endif /* PRINTER_H_ */
