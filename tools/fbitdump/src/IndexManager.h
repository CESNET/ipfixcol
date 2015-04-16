/**
 * \file IndexManager.h
 * \author Petr Velan <petr.velan@cesnet.cz>
 * \brief Header of class that works with indexes
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

#ifndef INDEXMANAGER_H_
#define INDEXMANAGER_H_

#include "Configuration.h"
#include "TableManager.h"

namespace fbitdump {

class IndexManager
{
public:
	/**
	 * \brief Delete indexes from parts specified in configuration
	 *
	 * Uses system() function to call rm (not very portable)
	 *
	 * @param conf Configuration class that specifies indexes to remove
	 * @param tm TableManager with created parts to process
	 */
	static void deleteIndexes(Configuration &conf, TableManager &tm);

	/**
	 * \brief Create indexes specified in configuration on parts in TableManager
	 *
	 * Parts in table manager are already loaded, just use them
	 *
	 * @param conf Configuration class that specifies indexes to create
	 * @param tm TableManager with created parts to process
	 */
	static void createIndexes(Configuration &conf, TableManager &tm);
};

} /* end of fbitdump namespace */

#endif /* INDEXMANAGER_H_ */
