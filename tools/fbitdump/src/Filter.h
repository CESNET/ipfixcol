/**
 * \file Filter.h
 * \author Petr Velan <petr.velan@cesnet.cz>
 * \brief Header of class for filtering
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

#ifndef FILTER_H_
#define FILTER_H_

#include "typedefs.h"
#include "Configuration.h"
#include "Cursor.h"
#include "parser.h"

/* both parser.y and scanner.l use this, but there is
 * no logical connection to Filter class */
#define YY_DECL int yylex(parser::Parser::semantic_type *yylval, \
    parser::Parser::location_type *yylloc, \
    fbitdump::Filter &filter, \
    yyscan_t yyscanner)

/* this is defined in scanner.h, however including it here would result
 * in inluding it back in scanner.c which is BAD */
typedef void* yyscan_t;

/**
 * \brief Namespace of the fbitdump utility
 */
namespace fbitdump {

/* Configuration and Cursor classes transitively depend on this header */
class Configuration;
class Cursor;

/* Enum for parserStruct type */
enum partsType {
	PT_COLUMN,
	PT_GROUP,
	PT_RAWCOLUMN,
	PT_NUMBER,
	PT_CMP,
	PT_BITCOLVAL,
	PT_IPv4,
	PT_IPv4_SUB,
	PT_IPv6,
	PT_IPv6_SUB,
	PT_TIMESTAMP,
	PT_STRING,
	PT_HOSTNAME,
	PT_HOSTNAME6,
	PT_COMPUTED,
	PT_LIST
};

/* Struct for parsing data from bison parser */
typedef struct _parserStruct {
	partsType type;
	uint16_t nParts;
	std::string colType;
	stringSet baseCols;
	void (*parse)(char *input, char *out, void*);
	void *parseConf;
	std::vector<std::string> parts;
} parserStruct;

/**
 * \brief Class managing filter
 *
 * Parses and builds filter
 */
class Filter
{
public:

	/**
	 * \brief Print error with location
	 * @param loc location of the error
	 * @param msg error message
	 */
    void error(const parser::location &loc, const std::string &msg);

    /**
     * \brief Print error message
     * @param msg message to print
     */
    void error(const std::string &msg);


	/**
	 * \brief Constructor
	 *
	 * @param conf Configuration class
	 */
	Filter(Configuration &conf) throw (std::invalid_argument);

	/**
	 * \brief Empty Construtor
	 *
	 * Creates filter that does nothing.
	 * init() does nothing with this setup
	 */
	Filter();

	/**
	 * \brief Build and return filter string for fastbit query
	 *
	 * This should take specified time  windows into consideration
	 *
	 * @return Filter string
	 */
	const std::string getFilter() const;

	/**
	 * \brief Decides whether row specified by cursor matches the filter
	 *
	 * @param cur Cursor pointing to table row
	 * @return True when line specified by cursor matches the filter (passes)
	 */
	bool isValid(Cursor &cur) const;

	/**
	 * \brief Parses number (kK -> 000, mM -> 000000 ...) and fills parser structure
	 *
	 * @param ps Parser structure with information needed for creating expressions
	 * @param number
	 */
	void parseNumber(parserStruct *ps, std::string number) const throw (std::invalid_argument);

	/**
	 * \brief Parses number in hexadecimal format and fills parser structure
	 *
	 * @param ps Parser structure
	 * @param number Hexadecimal number
	 */
	void parseHex(parserStruct *ps, std::string number) const throw (std::invalid_argument);
        
        /**
         * \brief Parses float number
         * 
         * @param ps Parser structure
         * @param number Float number
         */
        void parseFloat(parserStruct *ps, std::string number) const;
        
	/**
	 * \brief Parses IPv4 address
	 *
	 * Converts IPv4 address from text into numeric format and saves it into parser structure
	 *
	 * @param ps Parser structure
	 * @param addr Address in text format
	 */
	void parseIPv4(parserStruct *ps, std::string addr) const throw (std::invalid_argument);

	/**
	 * \brief Parses IPv4 address and its subnet mask
	 *
	 * @param ps Parser structure
	 * @param addr Address in text format
	 */
	void parseIPv4Sub(parserStruct *ps, std::string addr) const throw (std::invalid_argument);

	/**
	 * \brief Parses IPv6 address
	 *
	 * Converts IPv6 address from text into numeric (two 64b values) and saves it into parser structure
	 *
	 * @param ps Parser structure
	 * @param addr Address in text format
	 */
	void parseIPv6(parserStruct *ps, std::string addr) const throw (std::invalid_argument);

	/**
	 * \brief Parses IPv6 address and its subnet mask
	 *
	 * @param ps Parser structure
	 * @param addr Address in text format
	 */
	void parseIPv6Sub(parserStruct *ps, std::string addr) const throw (std::invalid_argument);

	/**
	 * \brief Parses timestamp in format %Y/%m/%d.%H:%M:%S
	 *
	 * @param ps Parser structure
	 * @param timestamp
	 */
	void parseTimestamp(parserStruct *ps, std::string timesamp) const throw (std::invalid_argument);

	/**
	 * \brief Parses column
	 *
	 * Converts column alias into right name and saves all its parts into parser structure
	 *
	 * @param ps Parser structure
	 * @param alias Column alias
	 */
	virtual void parseColumn(parserStruct *ps, std::string alias) const;

	/**
	 * \brief Parses column group
	 *
	 * If column group with given alias is found, parseColumn is called on each member.
	 *
	 * @param ps Parser structure
	 * @param alias Column group alias
	 * @bool aggregate Aggregation option
	 * @return True when succesful
	 */
	bool parseColumnGroup(parserStruct *ps, std::string alias, bool aggregate) const;
	/**
	 * \brief Only fills parser structure with column name
	 *
	 * @param ps Parser structure
	 * @param strcol Column name
	 */
	virtual void parseRawcolumn(parserStruct *ps, std::string strcol) const;

	/**
	 * \brief Parses expression "column BITOPERATOR value" into parser structure
	 *
	 * @param ps Output parser structure
	 * @param left Input parser structure
	 * @param right Input parser structure
	 * @param op Operator
	 */
	void parseBitColVal(parserStruct *ps, parserStruct *left, std::string op, parserStruct *right) const throw (std::invalid_argument);

	/**
	 * \brief Parses final expression "column CMP value" into string
	 *
	 * @param left Input parser structure
	 * @param right Input parser structure
	 * @param cmp Comparison
	 * @return Filter text in string form
	 */
	std::string parseExp(parserStruct *left, std::string cmp, parserStruct *right) const throw (std::invalid_argument);

	/**
	 * \brief Adds comparison and calls parseExp(left, cmp, right)
	 *
	 * @param left Input parser structure
	 * @param right Input parser structure
	 * @return Filter text in string form
	 */
	std::string parseExp(parserStruct *left, parserStruct *right) const;

	/**
	 * \brief Parses final expression "column value" when IP with subnet is given
	 *
	 * @param left Input parser structure
	 * @param right Input parser structure
         * @param cmp comparison 
	 */
	std::string parseExpSub(parserStruct *left, std::string cmp, parserStruct *right) const throw (std::invalid_argument);

	/**
	 * \brief Parses final expression when IPv6 is given as hostname
	 *
	 * @param left Input parser structure
	 * @param right Input parser structure
	 * @param cmp comparison
	 */
	std::string parseExpHost6(parserStruct *left, std::string cmp, parserStruct *right) const throw (std::invalid_argument);

	yyscan_t scaninfo;	/**< lexer context */

        /**
	 * \brief Parses mac address (only fills parser structure)
	 *
	 * @param ps Parser structure
	 * @param text String from parser
	 */
	void parseMac(parserStruct *ps, std::string text) const;
        
	/**
	 * \brief Parses string (only fills parser structure)
	 *
	 * @param ps Parser structure
	 * @param text String from parser
	 */
	void parseString(parserStruct *ps, std::string text) const throw (std::invalid_argument);

	/**
	 * \brief Create new list and store cmp ("in" or "not in") behind last member of column->parts
	 *
	 * @param list Vector of all structures in list expression
	 * @param cmp "in" or "not in"
	 * @param column Parser structure to be added into list
	 */
	void parseListCreate(std::vector<parserStruct *> *list, std::string cmp, parserStruct *column) const throw (std::invalid_argument);

	/**
	 * \brief Insert new structure into list (%column in value value value....)
	 *
	 * @param list Vector of all structures in list expression
	 * @param value Parser structure to be added into list
	 */
	void parseListAdd(std::vector<parserStruct *> *list, parserStruct *value) const throw (std::invalid_argument);

	/**
	 * \brief Parse list expression - goes throught all structures in list and parses them with column
	 *
	 * @param list Vector of all structures in list expression
	 */
	std::string parseExpList(std::vector<parserStruct *> *list) const throw (std::invalid_argument);
	
	/**
	 * \brief Parse 'exists column' expression
	 * 
	 * @param ps Parser structure
	 */
	std::string parseExists(parserStruct *ps) const throw (std::invalid_argument);

	/**
	 * \brief Sets new filter string
	 *
	 * @param newFilter New filter string
	 */
	void setFilterString(std::string newFilter);
        
        /**
         * \brief Check whether there are no errors in filter
         * 
         * @return true when filter is ok
         */
        bool checkFilter();

protected:
	/**
	 * \brief Initialise the filter
	 *
	 * Uses filter string and XML source from configuration
	 *
	 * @param conf Configuration passed to constructor
	 */
	void init(Configuration &conf) throw (std::invalid_argument);

	/**
	 * \brief Parses IPv4 address and returns its numerical value as string
	 *
	 * @param[in] addr IPv4 address
	 * @return Numerical value as string
	 */
	std::string parseIPv4(std::string addr) const;

	/**
	 * \brief Parses IPv6 address and return its numerical value as string
	 *
	 * @param[in] addr IPv6 address
	 * @param[out] part1 First part of address
	 * @param[out] part2 Second part of address
	 */
	void parseIPv6(std::string addr, std::string& part1, std::string& part2) const;

	/**
	 * \brief Parse timestamp to number of seconds
	 *
	 * @param str String with text representation of the timestamp
	 * @return Number of seconds in timestamp
	 */
	time_t parseTimestamp(std::string str) const throw (std::invalid_argument);

	/**
	 * \brief Decides how to parse string according to type
	 *
	 * @param ps Parser structure
         * @param col Parser structure for column
	 * @param[in] type Column type
	 */
	void parseStringType(parserStruct *ps, parserStruct *col, std::string &cmp) const throw (std::invalid_argument);

	/**
	 * \brief Parses hostname and converts it into ip addresses
	 *
	 * @param ps Parser struct
	 * @param af_type Type of address (AF_INET or AF_INET6)
	 */
	void parseHostname(parserStruct *ps, uint8_t af_type) const throw (std::invalid_argument);

	/**
	 * \brief Applies plugin specific parsing to the value.
	 *
	 * left->parse must be valid pointer to parse function
	 *
	 * @params left Parser struct with parse function
	 * @params right Parser struct with value to be parsed
	 * @return true if parsing was successful, false otherwise
	 */
	void parsePlugin(parserStruct *left, parserStruct *right) const throw (std::invalid_argument);

	/**
	 * \brief If type is PT_BITCOLVAL, return only column part of this expression
	 *
	 * @param name Expression
	 * @param type Type of column
	 */
	std::string onlyCol(std::string &fdfname, partsType type) const;

	/**
	 * \brief Create EXISTS(column) expression
	 *
	 * @param left Parser structure with column(s)
	 * @param i index of part in parser structure
	 * @param neq true when comparison is '!='
	 * @return EXISTS expression
	 */
	std::string createExists(parserStruct *left, int i, bool neq) const;

	/**
	 * \brief Create EXISTS(column) expression
	 *
	 * @param left Parser structure with column(s)
	 * @param i index of part in parser structure
	 * @param exists clause of expression (EXISTS or NOT EXISTS)
	 * @param op logic operator (and/or)
	 * @return EXISTS expression
	 */
	std::string createExists(parserStruct *left, int i, std::string exists, std::string op) const;

	Configuration *actualConf; /**< Used configuration for getting column names while parsing filter */

	std::string filterString; /**< String for fastbit condition */
};

}  // namespace fbitdump


#endif /* FILTER_H_ */
