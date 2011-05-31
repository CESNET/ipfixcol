/**
 * \file verbose.h
 * \author Petr Kramolis <kramolis@cesnet.cz>
 * \brief Debug, warning and verbose MACROS for ipfixcol and its pulgins
 *
 * Copyright (C) 2009-2011 CESNET, z.s.p.o.
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

#include <syslog.h>

#ifndef VERBOSE_H_
#define VERBOSE_H_

/*! controls messages level */
int msg_level = 2;

/* controls usage of syslog */
int syslog_on = 0;

/*! buffer for messages */
char msg_buffer[4096];


/**
 * \brief open syslog for messages
 *
 * set syslog for print_msg() function
 *
 *\param progname name of program
 */
void use_syslog(char *progname){
	openlog(progname, 0, 0);
	syslog_on = 1;
}

/**
 * \brief set messages level
 *
 * All messages with lower or equal level
 * are printed.
 *
 *\param level level of messages
 */
void set_msg_level(int level){
	msg_level = level;
}

/*!
\brief MSG levels
*/
typedef enum msg_level{
	MSG_ERROR = 0, /*!< Print msg as error msg (printed with default msg_level)*/
	MSG_WARNING = 1, /*!< Print msg as warning msg (printed with default msg_level)*/
	MSG_NOTICE = 2, /*!< Print msg as verbose msg (printed with default msg_level)*/
	MSG_VERBOSE = 3, /*!< Print msg as verbose msg (print only if msg_level is increased )*/
	MSG_VERBOSE_ADVANCED = 4, /*!< Print msg as verbose msg (print only if msg_level is increased even more )*/
	MSG_DEBUG = 5 /*!< Print msg as debug msg (only if DEBUG macro is defined) */
}msg_level_t;


/**
 * \brief send message to stderr or syslog without new line
 *
 * send message to stderr or syslog according to global syslog_on variable
 * messages with MSG_DEBUG level are printed only if DEBUG macro is defined
 *
 * \param level message level
 * \param string format string, like printf function
 */
void print_msg(int level, char *string)
{
        if (syslog_on == 0) {
		if(level == MSG_ERROR ){
                	fprintf(stderr, "ERROR: %s", string);
		}
		else if(level == MSG_WARNING ){
                	fprintf(stderr, "WARNING: %s", string);
		}
		else if(level == MSG_NOTICE ){
                	fprintf(stderr, "NOTICE: %s", string);
		}
		else if(level == MSG_VERBOSE ){
                	fprintf(stderr, "VERBOSE: %s", string);
		}
		else if(level == MSG_VERBOSE_ADVANCED ){
                	fprintf(stderr, "VERBOSE: %s", string);
		}
#ifdef DEBUG
		else if(level == MSG_DEBUG ){
                	fprintf(stderr, "DEBUG: %s", string);
		}
#endif
                fflush(stderr);
        } else {
		if(level == MSG_ERROR ){
                	syslog(LOG_CRIT, "%s", string);
		}
		else if(level == MSG_WARNING ){
	                syslog(LOG_WARNING, "%s", string);
		}
		else if(level == MSG_NOTICE ){
        	        syslog(LOG_NOTICE, "%s", string);
		}
		else if(level == MSG_VERBOSE ){
                	syslog(LOG_INFO, "%s", string);
		}
		else if(level == MSG_VERBOSE_ADVANCED ){
	                syslog(LOG_INFO, "%s", string);
		}
#ifdef DEBUG
		else if(level == MSG_DEBUG ){
	                syslog(LOG_DEBUG, "%s", string);
		}
#endif
        }
}



#	define MSG(level,format,args...) if(msg_level>=level){snprintf(msg_buffer,4095,format,##args); print_msg(level,msg_buffer);}

#endif /* VERBOSE_H_ */
