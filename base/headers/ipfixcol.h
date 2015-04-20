/*
 * Author: Radek Krejci <rkrejci@cesnet.cz>
 * The main devel header for IPFIX Collector.
 *
 * Copyright (C) 2015 CESNET, z.s.p.o.
 *
 * LICENSE TERMS
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
 * This software is provided ``as is'', and any express or implied
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

#ifndef IPFIXCOL_H_
#define IPFIXCOL_H_

/**
 * \mainpage IPFIX Collector Developer's Documentation
 *
 * This documents provides documentation of IPFIX Collector (ipfixcol). We
 * provides public API of the collector's input plugins as well as its storage
 * (output) plugins.
 */

/**
 * \defgroup publicAPIs Public ipfixcol's APIs
 * \brief APIs for connecting plugins into the ipfixcol.
 */

/**
 * \defgroup inputPlugins ipficol's Input Plugins
 * \brief Input plugins for the ipfixcol.
 */

#include <ipfixcol/input.h>
#include <ipfixcol/storage.h>
#include <ipfixcol/ipfix.h>
#include <ipfixcol/templates.h>
#include <ipfixcol/verbose.h>
#include <ipfixcol/centos5.h>
#include <ipfixcol/utils.h>
#include <ipfixcol/intermediate.h>
#include <ipfixcol/api.h>
#include <ipfixcol/ipfix_message.h>

#endif /* IPFIXCOL_H_ */
