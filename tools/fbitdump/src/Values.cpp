/**
 * \file Values.cpp
 * \author Petr Velan <petr.velan@cesnet.cz>
 * \brief Struct for managing values of different types
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

#include "Values.h"

namespace fbitdump {

int64_t Values::toLong(int part) const
{
	switch (type) {
	case ibis::BYTE:
		return (int64_t) this->value[part].int8;
		break;
	case ibis::UBYTE:
		return (int64_t) this->value[part].uint8;
		break;
	case ibis::SHORT:
		return (int64_t) this->value[part].int16;
		break;
	case ibis::USHORT:
		return (int64_t) this->value[part].uint16;
		break;
	case ibis::INT:
		return (int64_t) this->value[part].int32;
		break;
	case ibis::UINT:
		return (int64_t) this->value[part].uint32;
		break;
	case ibis::LONG:
		return (int64_t) this->value[part].int64;
		break;
	case ibis::ULONG:
		return (int64_t) this->value[part].uint64;
		break;
	default: return 0;
	}
}

double Values::toDouble(int part) const
{
	switch (type) {
	case ibis::FLOAT:
		return (double) this->value[part].flt;
		break;
	case ibis::DOUBLE:
		return this->value[part].dbl;
		break;
	case ibis::ULONG:
		return (double) this->value[part].uint64;
		break;
	default:
		return (double) this->toLong(part);
	}
}

std::string Values::toString(bool plainNumbers) const
{
	std::string valStr;
	/* this is static for performance reason */
	static std::ostringstream ss;

	/* print by type */
	switch (this->type) {
	/* formatNumber is a macro */
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
		formatNumber(this->value[0].int32, ss, plainNumbers);
		valStr = ss.str();
		break;
	case ibis::UINT:
		formatNumber(this->value[0].uint32, ss, plainNumbers);
		valStr = ss.str();
		break;
	case ibis::LONG:
		formatNumber(this->value[0].int64, ss, plainNumbers);
		valStr = ss.str();
		break;
	case ibis::ULONG:
		formatNumber(this->value[0].uint64, ss, plainNumbers);
		valStr = ss.str();
		break;
	case ibis::FLOAT:
		formatNumber(this->value[0].flt, ss, plainNumbers);
		valStr = ss.str();
		break;
	case ibis::DOUBLE:
		formatNumber(this->value[0].dbl, ss, plainNumbers);
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

template <class T>
inline void Values::formatNumber(T num, std::ostringstream &ss, bool plainNumbers) const
{
	ss << std::fixed;
	if (num <= 1000000 || plainNumbers) {
		ss.precision(0);
		ss << num;
	} else if (num > 1000000000) {
		ss.precision(1);
		ss << (float) num/1000000000 << " G";
	} else if (num > 1000000) {
		ss.precision(1);
		ss << (float) num/1000000 << " M";
	}
	ss.precision(0); /* set zero precision for other numbers */
}

inline bool operator== (const Values &lhs, const Values &rhs)
{
	/* if necessary, this could decide upon type. it could also take parts into consideration */
	return lhs.toDouble(0) == rhs.toDouble(0);
}

inline bool operator< (const Values &lhs, const Values &rhs)
{
	return lhs.toDouble(0) < rhs.toDouble(0);
}

inline bool operator!=(const Values &lhs, const Values &rhs)
{
	return !operator==(lhs,rhs);
}

inline bool operator> (const Values &lhs, const Values &rhs)
{
	return  operator< (rhs,lhs);
}

inline bool operator>= (const Values &lhs, const Values &rhs)
{
	return !operator< (lhs,rhs);
}

inline bool operator<= (const Values &lhs, const Values &rhs)
{
	return !operator> (lhs,rhs);
}

} /* end of namespace fbitdump */

