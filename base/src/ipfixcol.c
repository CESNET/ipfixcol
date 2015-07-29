/**
 * \file ipfixcol.c
 * \author Radek Krejci <rkrejci@cesnet.cz>
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

#if HAVE_CONFIG_H
#include <pkgconfig.h>
#endif

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <getopt.h>
#include <pthread.h>
#include <signal.h>
#include <syslog.h>
#include <sys/wait.h>
#include <sys/prctl.h>

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>

#include <ipfixcol.h>
#include <ipfixcol/profiles.h>
#include "intermediate_process.h"
#include "config.h"
#include "preprocessor.h"
#include "output_manager.h"
#include "configurator.h"

/**
 * \defgroup internalAPIs ipfixcol's Internal APIs
 *
 * Internal ipfixcol's functions not supposed to be used outside the collector
 * core.
 *
 */

/** Acceptable command-line parameters (normal) */
#define OPTSTRING "c:dhv:Vsr:i:S:e:"

/** Acceptable command-line parameters (long) */
struct option long_opts[] = {
   { "help",    no_argument, NULL, 'h' },
   { "version", no_argument, NULL, 'V' },
   { 0, 0, 0, 0 }
};

/* Path to ipfix-elements.xml file */
const char *ipfix_elements = DEFAULT_IPFIX_ELEMENTS;

/* Template Manager */
struct ipfix_template_mgr *template_mgr = NULL;

/* terminating indicator */
volatile int terminating = 0;

/* reconfiguration indicator */
volatile int reconf = 0;

/* plugins configuration */
configurator *config = NULL;

/** Identifier to MSG_* macros */
static char *msg_module = "main";

/** Ring buffer size */
int ring_buffer_size = 8192;

/**
 * \brief Print program version information
 */
void version ()
{
	printf ("%s: IPFIX Collector capture daemon\n", PACKAGE);
	printf ("Version: %s, Copyright (C) 2015 CESNET z.s.p.o.\n", VERSION);
	printf ("Check out http://www.liberouter.org/technologies/ipfixcol/ for more information.\n\n");
}

/**
 * \brief Print program help
 */
void help ()
{
	printf ("Usage: %s [-c file] [-i file] [-dhVs] [-v level]\n", PACKAGE);
	printf ("  -c file   Path to startup configuration file (default: %s)\n", DEFAULT_CONFIG_FILE);
	printf ("  -i file   Path to internal configuration file (default: %s)\n", INTERNAL_CONFIG_FILE);
	printf ("  -e file   Path to IPFIX IE specification file (default: %s)\n", DEFAULT_IPFIX_ELEMENTS);
	printf ("  -d        Run daemonized\n");
	printf ("  -h        Print this help\n");
	printf ("  -v level  Increase logging verbosity (level: 0-3)\n");
	printf ("  -V        Print version information\n");
	printf ("  -s        Skip invalid sequence number error (especially useful for NetFlow v9 PDUs)\n");
	printf ("  -r        Ring buffer size (default: 8192)\n");
	printf ("  -S num    Print statistics every \"num\" seconds\n");
	printf ("\n");
}

void term_signal_handler (int sig)
{
	/* Reconfiguration signal */
	if (sig == SIGUSR1) {
		MSG_COMMON(ICMSG_ERROR, "Signal detected (%i); reloading configuration...", sig);
		reconf = 1;
		return;
	}
	
	/* Terminating signal */
	if (terminating) {
		MSG_COMMON(ICMSG_ERROR, "Another termination signal detected (%i); quiting without cleanup...", sig);
		exit (EXIT_FAILURE);
	} else {
		MSG_COMMON(ICMSG_ERROR, "Signal detected (%i); exiting as soon as possible...", sig);
		terminating = 1;
	}
}

int main (int argc, char* argv[])
{
	int c, i, retval = 0, get_retval, proc_count = 0;
	int source_status = SOURCE_STATUS_OPENED, stat_interval = 0;
	pid_t pid = 0;
	bool daemonize = false;
	char *config_file = NULL, *internal_file = NULL;
	struct sigaction action;
	char *packet = NULL;
	struct input_info* input_info;
	void *output_manager_config = NULL;
	xmlXPathObjectPtr collectors = NULL;
	int ring_buffer_size = 8192;

	/* parse command line parameters */
	while ((c = getopt_long(argc, argv, OPTSTRING, long_opts, NULL)) != -1) {
		switch (c) {
		case 'c':
			config_file = optarg;
			break;
		case 'i':
			internal_file = optarg;
			break;
		case 'e':
			ipfix_elements = optarg;
			break;
		case 'd':
			daemonize = true;
			break;
		case 'h':
			help();
			exit(EXIT_SUCCESS);
			break;
		case 'v':
			verbose = strtoi(optarg, 10);
			if (verbose == INT_MAX) {
				MSG_ERROR(msg_module, "No valid verbosity level provided (%s)", optarg);
				help();
				exit(EXIT_FAILURE);
			}

			break;
		case 'V':
			version();
			exit(EXIT_SUCCESS);
			break;
		case 's':
			skip_seq_err = 1;
			break;
		case 'r':
			ring_buffer_size = strtoi(optarg, 10);
			if (ring_buffer_size == INT_MAX) {
				MSG_ERROR(msg_module, "No valid ring buffer size provided (%s)", optarg);
				help();
				exit(EXIT_FAILURE);
			}

			break;
		case 'S':
			stat_interval = strtoi(optarg, 10);
			if (stat_interval == INT_MAX) {
				MSG_ERROR(msg_module, "No valid statistics interval provided (%s)", optarg);
				help();
				exit(EXIT_FAILURE);
			}

			break;
		default:
			help();
			exit(EXIT_FAILURE);
			break;
		}
	}

	/* prepare signal handling */
	/* establish the signal handler */
	sigemptyset(&action.sa_mask);
	action.sa_flags = 0;
	action.sa_handler = term_signal_handler;
	sigaction(SIGINT, &action, NULL);
	sigaction(SIGQUIT, &action, NULL);
	sigaction(SIGTERM, &action, NULL);
	sigaction(SIGUSR1, &action, NULL);

	/*
	 * this initialize the library and check potential ABI mismatches
	 * between the version it was compiled for and the actual shared
	 * library used.
	 */LIBXML_TEST_VERSION
	xmlIndentTreeOutput = 1;

	/* we daemonize early to get syslog redirection for all messages */
	if (daemonize) {
		closelog();
		MSG_SYSLOG_INIT(PACKAGE);
		
		/* and send all following messages to the syslog */
		if (daemon (1, 0)) {
			MSG_ERROR(msg_module, "%s", strerror(errno));
		}
	}

	/* check config file */
	if (config_file == NULL) {
		/* and use default if not specified */
		config_file = DEFAULT_CONFIG_FILE;
		MSG_NOTICE(msg_module, "Using default configuration file: %s", config_file);
	}

	/* check internal config file */
	if (internal_file == NULL) {
		/* and use default if not specified */
		internal_file = INTERNAL_CONFIG_FILE;
		MSG_NOTICE(msg_module, "Using default internal configuration file: %s", internal_file);
	}
	  
	/* Initialize configurator */
	config = config_init(internal_file, config_file);
	if (!config) {
		MSG_ERROR(msg_module, "Configurator initialization failed");
		goto cleanup_err;
	}
	
	/* Get all collectors */
	collectors = get_collectors(config->act_doc);
	if (collectors == NULL) {
		/* no collectingProcess configured */
		MSG_ERROR(msg_module, "No collector process configured");
		goto cleanup_err;
	}
	
	/* create separate process for each <collectingProcess> */
	for (i = (collectors->nodesetval->nodeNr - 1); i >= 0; i--) {
		/*
		 * fork for multiple collectors - original parent process handle only
		 * collector 0
		 */
		if (i > 0) {
			pid = fork();
			if (pid > 0) { /* parent process waits for collector 0 */
				proc_count++;
				continue;
			} else if (pid < 0) { /* error occured, fork failed */
				MSG_ERROR(msg_module, "Forking collector process failed (%s); skipping collector '%d'", strerror(errno), i);
				continue;
			}
			/* else child - just continue to handle plugins */
			config->proc_id = i;
			
			MSG_NOTICE(msg_module, "[%d] New collector process started", config->proc_id);
		}
		
		/* DEBUG - remove this */
		config->proc_id = getpid();
		
		/* Set collectors node */
		config->collector_node = collectors->nodesetval->nodeTab[i];
		
		break;
	}

	/* XML cleanup */
	xmlXPathFreeObject(collectors);
	
	/*
	 * create Template Manager
	 */
	template_mgr = tm_create();
	if (template_mgr == NULL) {
		MSG_ERROR(msg_module, "[%d] Unable to create Template Manager", config->proc_id);
		goto cleanup_err;
	}
	
	/* Create output queue for preprocessor */
	preprocessor_set_output_queue(rbuffer_init(ring_buffer_size));
	
	/* Create Output Manager */
	retval = output_manager_create(config, stat_interval, &output_manager_config);
	if (retval != 0) {
		MSG_ERROR(msg_module, "[%d] Unable to create Output Manager", config->proc_id);
		goto cleanup_err;
	}
	
	/* Parse plugins configuration */
	if (config_reconf(config) != 0) {
		MSG_ERROR(msg_module, "[%d] Unable to parse plugin configuration", config->proc_id);
		goto cleanup_err;
	}
	
	/* Pass configurator to the preprocessor */
	preprocessor_set_configurator(config);

	/* configure output subsystem */
	retval = output_manager_start();
	if (retval != 0) {
		MSG_ERROR(msg_module, "[%d] Storage Manager initialization failed", config->proc_id);
		goto cleanup;
	}
	
	/* main loop */
	while (!terminating) {
		/* get data to process */
		if ((get_retval = config->input.get(config->input.config, &input_info, &packet, &source_status)) < 0) {
			if ((!reconf && !terminating) || get_retval != INPUT_INTR) { /* if interrupted and closing, it's ok */
				MSG_WARNING(msg_module, "[%d] Could not get IPFIX data", config->proc_id);
			}
			
			if (reconf) {
				config_reconf(config);
				reconf = 0;
			}
			
			if (packet) {
				free(packet);
				packet = NULL;
			}

			continue;
		} else if (get_retval == INPUT_CLOSED) {
			/* ensure that parser gets NULL packet => closed connection */
			if (packet != NULL) {
				/* free the memory allocated by xml_conf (if any) right away */
				free(packet);
				packet = NULL;
			}

			/* if input plugin is file reader, end collector */
			if (input_info->type == SOURCE_TYPE_IPFIX_FILE) {
				terminating = 1;
			}
		}

		/* distribute data to the particular Data Manager for further processing */
		preprocessor_parse_msg(packet, get_retval, input_info, source_status);
		source_status = SOURCE_STATUS_OPENED;
		packet = NULL;
		input_info = NULL;
	}
	
	goto cleanup;
	
cleanup_err:
	retval = EXIT_FAILURE;

cleanup:
	/* Close preprocessor */
	preprocessor_close();
	
	/* Flush buffers in intermediate plugins */
	if (config) {
		config_stop_inter(config);
	}

	/* Close whole Output Manager (including Data Managers) */
	if (output_manager_config) {
		output_manager_close(output_manager_config);
	}

	/* Close all plugins */
	if (config) {
		config_destroy(config);
	}
	
	/* wait for child processes */
	if (pid > 0) {
		for (i = 0; i < proc_count; i++) {
			pid = wait(NULL);
			MSG_NOTICE(msg_module, "[%d] Collector child process '%d' terminated", config->proc_id, pid);
		}
		MSG_NOTICE(msg_module, "[%d] Closing collector", config->proc_id);
	}

	/* destroy template manager */
	if (template_mgr) {
		tm_destroy(template_mgr);
	}

	xmlCleanupThreads();
	xmlCleanupParser();

	return (retval);
}
