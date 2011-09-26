/**
 * \file AST.h
 * \author Petr Velan <petr.velan@cesnet.cz>
 * \brief Header of class for managing abstract syntax tree
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

#ifndef AST_H_
#define AST_H_

#include "typedefs.h"

namespace ipfixdump {

#define MAX_PARTS 2

/**
 * \brief structure for passing values of unknown type
 */
struct values
{
	ibis::TYPE_T type;
	union
	{
		char int8;
		unsigned char uint8;
		int16_t int16;
		uint16_t uint16;
		int32_t int32;
		uint32_t uint32;
		int64_t int64;
		uint64_t uint64;
		float flt;
		double dbl;
	} value[MAX_PARTS];
	std::string string;

	uint64_t toULong(int part=0)
	{
		switch (type) {
		case ibis::BYTE:
			return (uint64_t) this->value[part].int8;
			break;
		case ibis::UBYTE:
			return (uint64_t) this->value[part].uint8;
			break;
		case ibis::SHORT:
			return (uint64_t) this->value[part].int16;
			break;
		case ibis::USHORT:
			return (uint64_t) this->value[part].uint16;
			break;
		case ibis::INT:
			return (uint64_t) this->value[part].int32;
			break;
		case ibis::UINT:
			return (uint64_t) this->value[part].uint32;
			break;
		case ibis::LONG:
			return (uint64_t) this->value[part].int64;
			break;
		case ibis::ULONG:
			return this->value[part].uint64;
			break;
		default: return 0;
		}
	}

	double toDouble(int part=0)
	{
		switch (type) {
		case ibis::FLOAT:
			return (double) this->value[part].flt;
			break;
		case ibis::DOUBLE:
			return this->value[part].dbl;
			break;
		default:
			return (double) this->toULong(part);
		}
	}

	std::string toString()
	{
		std::string valStr;
		/* this is static for preformance reason */
		static std::ostringstream ss;

		/* print by type */
		/* TODO what to do with more parts here? */
		switch (this->type) {
		case ibis::BYTE:
			ss << (int16_t) this->value[0].int8;
			valStr = ss.str();
			break;
		case ibis::UBYTE:
			ss << (uint16_t) this->value[0].uint8;
			valStr = ss.str();
			break;
		case ibis::SHORT:
			ss << this->value[0].int16;
			valStr = ss.str();
			break;
		case ibis::USHORT:
			ss << this->value[0].uint16;
			valStr = ss.str();
			break;
		case ibis::INT:
			ss << this->value[0].int32;
			valStr = ss.str();
			break;
		case ibis::UINT:
			ss << this->value[0].uint32;
			valStr = ss.str();
			break;
		case ibis::LONG:
			ss << this->value[0].int64;
			valStr = ss.str();
			break;
		case ibis::ULONG:
			ss << this->value[0].uint64;
			valStr = ss.str();
			break;
		case ibis::FLOAT:
			ss << this->value[0].flt;
			valStr = ss.str();
			break;
		case ibis::DOUBLE:
			ss << this->value[0].dbl;
			valStr = ss.str();
			break;
		case ibis::TEXT:
		case ibis::CATEGORY:
		case ibis::OID:
		case ibis::BLOB:
		case ibis::UNKNOWN_TYPE:
			valStr = this->string;
			break;
		default:
			break;
		}
		/* clear string stream buffer for next usage */
		ss.str("");

		return valStr;
	}
};

/**
 * \brief types for AST structure
 */
enum astTypes
{
	value,   //!< value
	operation//!< operation
};

/**
 * \brief Abstract syntax tree structure
 *
 * Describes the way that column value is constructed from database columns
 */
struct AST
{
	astTypes type; /**< AST type */
	unsigned char operation; /**< one of '/', '*', '-', '+' */
	std::string semantics; /**< semantics of the column */
	std::string value; /**< value (column name) */
	std::string aggregation; /**< how to aggregate this column */
	int parts; /**< number of parts of column (ipv6 => e0id27p0 and e0id27p1)*/
	AST *left; /**< left subtree */
	AST *right; /**< right subtree */

	stringSet astColumns; /**< Cached columns set (computed in Column::getColumns(AST*)) */
	bool cached;

	/**
	 * \brief AST constructor - sets default values
	 */
	AST(): parts(1), left(NULL), right(NULL), cached(false) {}

	/**
	 * \brief AST destructor
	 */
	~AST()
	{
		delete left;
		delete right;
	}
};

} /* end of namespace ipfixdump */

#endif /* AST_H_ */
