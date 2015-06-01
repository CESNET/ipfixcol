/**
 * \file verbose.h
 * \author Michal Kozubik <kozubik@cesnet.cz>
 * \brief Verbose macros
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

#ifndef VERBOSE_H_
#define VERBOSE_H_

extern int  verbose;
extern bool use_syslog;

typedef enum {
	ICMSG_ERROR,
	ICMSG_WARNING,
	ICMSG_NOTICE,
	ICMSG_DEBUG
} ICMSG_LEVEL;

/**
 * \brief Macros for printing error messages
 * 
 * \param module Identification of program part that generated this message
 * \param format
 */
#define MSG_ERROR(format, ...) if(verbose < ICMSG_ERROR); else icmsg_print(ICMSG_ERROR, "ERROR", format, ## __VA_ARGS__)
#define MSG_WARNING(format, ...) if(verbose < ICMSG_WARNING); else icmsg_print(ICMSG_WARNING, "WARNING", format, ## __VA_ARGS__)
#define MSG_NOTICE(format, ...) if(verbose < ICMSG_NOTICE); else icmsg_print(ICMSG_NOTICE, "NOTICE", format, ## __VA_ARGS__)
#define MSG_DEBUG(format, ...) if(verbose < ICMSG_DEBUG); else icmsg_print(ICMSG_DEBUG, "DEBUG", format, ## __VA_ARGS__)

/**
 * \brief Macrot for initialising syslog
 *
 * \param ident Identification for syslog
 */
#define MSG_SYSLOG_INIT(ident) openlog(ident, LOG_PID, LOG_DAEMON); use_syslog = true


#define MSG_COMMON(format, ...) icmsg_print_common(format, ## __VA_ARGS__)

/**
 * \brief Set verbose level to level
 *
 * \param level
 */
#define MSG_SET_VERBOSE(level) verbose = level;

/**
 * \brief Printing function
 *
 * \param lvl Level of the message (for syslog severity)
 * \param prefix Message prefix (ERROR, NOTICE...)
 * \param module Module name
 * \param format message format
 */
void icmsg_print(ICMSG_LEVEL lvl, const char *prefix, const char *format, ...);
void icmsg_print_common(const char *format, ...);

#endif /* VERBOSE_H_ */
