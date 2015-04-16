/**
 * \file Cursor.cpp
 * \author Petr Velan <petr.velan@cesnet.cz>
 * \brief Class wrapping ibis::table::cursor
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

#include "Cursor.h"

namespace fbitdump {

Cursor::Cursor(Table &table): table(table), cursor(NULL) {}

bool Cursor::next()
{
	/* on first run create the real cursor */
	if (this->cursor == NULL) {
		/* if table was filtered out, there is no next row */
		if (this->table.getFastbitTable() == NULL) {
			return false;
		}

		this->cursor = this->table.getFastbitTable()->createCursor();
		/* save column types array (this is a lot more effective than calling the menthod on cursor) */
		this->columnTypes = this->cursor->columnTypes();
	}

	int ret = 0;
	/* skip filtered rows */
	do {
		ret = this->cursor->fetch();
		/* no more rows */
		if (ret != 0) return false;

	} while (!this->table.getFilter()->isValid(*this));

	/* we got valid row */
	return true;
}

bool Cursor::getColumn(std::string name, Values &value, int part) const
{
	if (this->cursor == NULL) {
		std::cerr << "Call next() on Cursor before reading!" << std::endl;
		return false;
	}

	int ret = 0;
	uint32_t colNum = 0;
	ibis::TYPE_T type;

	/* get location of the column */
	for (colNum = 0; colNum < this->cursor->columnNames().size(); ++colNum) {
		if (this->cursor->columnNames()[colNum] == name) {
			break;
		}
	}
	
	if (colNum >= cursor->columnNames().size()) {
		return false;
	}
	
	/* get column type */
	type = this->columnTypes[colNum];

	switch (type) {
	case ibis::BYTE:
		ret = this->cursor->getColumnAsByte(name.c_str(), value.value[part].int8);
		value.type = ibis::BYTE;
		break;
	case ibis::UBYTE:
		ret = this->cursor->getColumnAsUByte(name.c_str(), value.value[part].uint8);
		value.type = ibis::UBYTE;
		break;
	case ibis::SHORT:
		ret = this->cursor->getColumnAsShort(name.c_str(), value.value[part].int16);
		value.type = ibis::SHORT;
		break;
	case ibis::USHORT:
		ret = this->cursor->getColumnAsUShort(name.c_str(), value.value[part].uint16);
		value.type = ibis::USHORT;
		break;
	case ibis::INT:
		ret = this->cursor->getColumnAsInt(name.c_str(), value.value[part].int32);
		value.type = ibis::INT;
		break;
	case ibis::UINT:
		ret = this->cursor->getColumnAsUInt(name.c_str(), value.value[part].uint32);
		value.type = ibis::UINT;
		break;
	case ibis::LONG:
		ret = this->cursor->getColumnAsLong(name.c_str(), value.value[part].int64);
		value.type = ibis::LONG;
		break;
	case ibis::ULONG:
		ret = this->cursor->getColumnAsULong(name.c_str(), value.value[part].uint64);
		value.type = ibis::ULONG;
		break;
	case ibis::FLOAT:
		ret = this->cursor->getColumnAsFloat(name.c_str(), value.value[part].flt);
		value.type = ibis::FLOAT;
		break;
	case ibis::DOUBLE:
		ret = this->cursor->getColumnAsDouble(name.c_str(), value.value[part].dbl);
		value.type = ibis::DOUBLE;
		break;
	case ibis::TEXT:
	case ibis::CATEGORY: {
		ret = this->cursor->getColumnAsString(name.c_str(), value.string);
		value.type = ibis::TEXT;
		break; }
	case ibis::OID:
	case ibis::BLOB:
		value.type = ibis::BLOB;
		ret = this->cursor->getColumnAsOpaque(name.c_str(), value.opaque);
		if (ret >= 0) {
			value.value[part].blob.ptr = value.opaque.address();
			value.value[part].blob.length = value.opaque.size();
		}
		break;
	case ibis::UNKNOWN_TYPE:
		/* column not found in DB */
		ret = 1;
		break;
	default:
		break;
	}

	if (ret < 0) {
		return false;
	}

	return true;
}

Cursor::~Cursor()
{
	delete this->cursor;
}

} /* end of namespace fbitdump */
