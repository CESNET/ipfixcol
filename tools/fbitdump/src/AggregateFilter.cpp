/**
 * \file AggregateFilter.cpp
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

#include "AggregateFilter.h"
#include "Filter.h"
#include "Verbose.h"
#include "scanner.h"

namespace fbitdump {

AggregateFilter::AggregateFilter(Configuration &conf)
{
	std::string input = conf.getAggregateFilter();
	
	/* Get vector of columns in aggregated table */
	for (auto col: conf.getAggregateColumns()) {
		this->aggregateColumns.push_back(col);
	}
	for (auto col: conf.getSummaryColumns()) {
		this->aggregateColumns.push_back(col);
	}
	
	/* We need access to configuration in parseColumn() function */
	this->actualConf = &conf;

	if (input.empty()) {
		this->setFilterString("1 = 1");
	} else {
		/* Initialise lexer structure (buffers, etc) */
		yylex_init(&this->scaninfo);

		YY_BUFFER_STATE bp = yy_scan_string(input.c_str(), this->scaninfo);
		yy_switch_to_buffer(bp, this->scaninfo);

		/* create the parser, AggregateFilter is its param (it will provide scaninfo to the lexer) */
		parser::Parser parser(*this);

		/* run run parser */
		if (parser.parse() != 0) {
			throw std::invalid_argument(std::string("Error while parsing filter!"));
		}

		yy_flush_buffer(bp, this->scaninfo);
		yy_delete_buffer(bp, this->scaninfo);

		/* clear the context */
		yylex_destroy(this->scaninfo);
	}
	MSG_FILTER("Aggregate filter", this->filterString.c_str());
}

void AggregateFilter::setParserStruct(parserStruct *ps, Column *col) const
{
	/* Set parsing function, type etc. */
	if (this->actualConf->plugins.find(col->getSemantics()) != this->actualConf->plugins.end()) {
		ps->parse = this->actualConf->plugins[col->getSemantics()].parse;
	} else {
		ps->parse = NULL;
	}

	ps->nParts = 1;
	ps->type = PT_COLUMN;
	ps->colType = col->getSemantics();
	ps->parts.push_back(col->getSelectName());
}

void AggregateFilter::parseColumn(parserStruct* ps, std::string alias) const
{
	if (ps == NULL) {
		throw std::invalid_argument(std::string("Cannot parse column, NULL parser structure"));
	}
	
	/* Get right column (find entered alias in aggregated columns) */
	Column *col = this->getAggregateColumnByAlias(alias);

	if (!col) {
		throw std::invalid_argument("Filter column '" + alias + "' not found in aggregated table!");
	}
	
	this->setParserStruct(ps, col);
}

void AggregateFilter::parseRawcolumn(parserStruct* ps, std::string colname) const
{
	if (ps == NULL) {
		throw std::invalid_argument(std::string("Cannot parse raw column, NULL parser structure"));
	}

	/* Get right column (find entered element in aggregated columns) */
	Column *col = this->getAggregateColumnByElement(colname);
	
	if (!col) {
		throw std::invalid_argument("Filter column '" + colname + "' not found in aggregated table!");
	}
	
	this->setParserStruct(ps, col);
}

Column *AggregateFilter::getAggregateColumnBySelectName(std::string name) const
{
	for (auto col: this->aggregateColumns) {
		if (col->getSelectName() == name) {
			return col;
		}
	}
	
	return NULL;
}

Column *AggregateFilter::getAggregateColumnByElement(std::string element) const
{
	for (auto col: this->aggregateColumns) {
		if (col->getElement() == element) {
			return col;
		}
	}
	
	return NULL;
}

Column *AggregateFilter::getAggregateColumnByAlias(std::string alias) const
{
	for (auto col: this->aggregateColumns) {
		for (auto colAlias: col->getAliases()) {
			if (colAlias == alias) {
				return col;
			}
		}
	}
	
	return NULL;
}

AggregateFilter::~AggregateFilter()
{
}

} /* namespace fbitdump */