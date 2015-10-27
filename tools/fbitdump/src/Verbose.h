/**
 * \file Verbose.h
 * \author Petr Velan <petr.velan@cesnet.cz>
 * \brief Public function for debug output
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

extern int verbose;

typedef enum {
	ICMSG_ERROR,
	ICMSG_WARNING,
	ICMSG_INFO,
	ICMSG_DEBUG,
} ICMSG_LEVEL;

/**
 * \brief Macros for printing error messages.
 * \param module Identification of program component that generated this message
 * \param format
 */
#define MSG_FILTER(module, format, ...) if (verbose >= ICMSG_WARNING) icmsg_print(module, NULL, format, ##__VA_ARGS__)
#define MSG_ERROR(module, format, ...) if (verbose >= ICMSG_ERROR) icmsg_print("ERROR", module, format, ##__VA_ARGS__)
#define MSG_WARNING(module, format, ...) if (verbose >= ICMSG_WARNING) icmsg_print("WARNING", module, format, ## __VA_ARGS__)
#define MSG_INFO(module, format, ...) if (verbose >= ICMSG_INFO) icmsg_print("INFO", module, format, ## __VA_ARGS__)
#define MSG_DEBUG(module, format, ...) if (verbose >= ICMSG_DEBUG) icmsg_print("DEBUG", module, format, ## __VA_ARGS__)

/**
 * \brief Macro for printing common messages, without severity prefix.
 *
 * In syslog, all of these messages will have LOG_INFO severity.
 *
 * \param level The verbosity level at which this message should be printed
 * \param format
 */
#define MSG_COMMON(level, format, ...) if (verbose < level); else icmsg_print(-1, format"\n", ## __VA_ARGS__)

/**
 * \brief Macro for initialising syslog.
 *
 * \param ident Identification for syslog
 */
#define MSG_SYSLOG_INIT(ident) openlog(ident, LOG_PID, LOG_DAEMON);

/**
 * \brief Set verbosity level to the specified level.
 *
 * \param level
 */
#define MSG_SET_VERBOSE(level) verbose = level;

/**
 * \brief Printing function.
 *
 * \param level Verbosity level of the message (for syslog severity)
 * \param module
 * \param format
 */
void icmsg_print(const char *type, const char *module, const char *format, ...);

#endif /* VERBOSE_H_ */
