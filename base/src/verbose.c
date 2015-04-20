/**
 * \file verbose.c
 * \author Petr Velan <petr.velan@cesnet.cz>
 * \brief Main body of the ipfixcol
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

#include <ipfixcol/verbose.h>
#include <stdio.h>
#include <stdarg.h>
#include <syslog.h>

/* Default is to print only errors */
int verbose = ICMSG_ERROR;

/* Do not use syslog unless specified otherwise */
int use_syslog = 0;
int skip_seq_err = 0;

void icmsg_print(ICMSG_LEVEL lvl, const char *format, ...)
{
	va_list ap;
	int priority;

	va_start(ap, format);
	vprintf(format, ap);
	va_end(ap);

	if (use_syslog) {
		va_start(ap, format);
		switch (lvl) {
		case ICMSG_ERROR: priority = LOG_ERR; break;
		case ICMSG_WARNING: priority = LOG_WARNING; break;
		case ICMSG_NOTICE: priority = LOG_NOTICE; break;
		case ICMSG_DEBUG: priority = LOG_DEBUG; break;
		default: priority = LOG_INFO; break;
		}
		vsyslog(priority, format, ap);
		va_end(ap);
	}
}
