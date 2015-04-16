/**
 * \file AggregateFilter.h
 * \author Michal Kozubik <kozubik@cesnet.cz>
 * \brief Class for management of result filtering
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


#ifndef AGGREGATEFILTER_H
#define	AGGREGATEFILTER_H

#include "Filter.h"
#include "Configuration.h"

namespace fbitdump {

/**
 * \brief Class for parsing post-aggregate filter
 * 
 * Most of methods are inherited from Filter
 */
class AggregateFilter: public Filter {
public:
    /**
     * \brief Constructor
     * 
     * @param conf configuration class
     */
    AggregateFilter(Configuration &conf);
    
    /**
     * \brief Parse column alias - check whether it is in aggregated table
     * 
     * @param ps parser structure to be filled
     * @param alias column alias
     */
    void parseColumn(parserStruct* ps, std::string alias) const;
    
    /**
    * \brief Parse column name - check whether it is in aggregated table
    *
    * @param ps parser structure to be filled
    * @param colname Column name
    */
    void parseRawcolumn(parserStruct *ps, std::string colname) const;
    
    virtual ~AggregateFilter();
    
private:
    
    /**
     * \brief Get column from aggregated table by it's select name
     * 
     * @param name Column name
     * @return Column
     */
    Column *getAggregateColumnBySelectName(std::string name) const;
    
    /**
     * \brief Get column from aggregated table by it's element
     * 
     * @param element Column element
     * @return Column
     */
    Column *getAggregateColumnByElement(std::string element) const;
    
    /**
     * \brief Get column from aggregated table by it's alias
     * 
     * @param alias Column alias
     * @return Column
     */
    Column *getAggregateColumnByAlias(std::string alias) const;
    
    /**
     * \brief Fill parser structure with informations about column
     * 
     * @param ps parser structure
     * @param col Column
     */
    void setParserStruct(parserStruct *ps, Column *col) const;
    
    columnVector aggregateColumns;     /**< columns in aggregated table */
};

} /* namespace fbitdump */
#endif	/* AGGREGATEFILTER_H */

