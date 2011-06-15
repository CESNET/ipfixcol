/**
 * \file ipfixcol.c
 * \author Radek Krejci <rkrejci@cesnet.cz>
 * \brief Main body of the ipfixcol
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

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <commlbr.h>
#include <signal.h>
#include <syslog.h>

#include <pthread.h>

#include <libxml/parser.h>
#include <libxml/xpath.h>

#include "../ipfixcol.h"

#include "config.h"
#include "data_mngmt.h"

/**
 * \defgroup internalAPIs ipfixcol's Internal APIs
 *
 * Internal ipfixcol's functions not supposed to be used outside the collector
 * core.
 *
 */


/** Version number */
#define VERSION "0.1"

/** Acceptable command-line parameters */
#define OPTSTRING "c:dhv:V"

/* verbose from libcommlbr */
extern int verbose;

/* main loop indicator */
volatile int done = 0;

/**
 * \brief Print program version information
 * @param progname Name of the program
 */
void version (char* progname)
{
	printf ("%s: IPFIX Collector capture daemon.\n", progname);
	printf ("Version: %s, Copyright (C) 2011 CESNET z.s.p.o.\n", VERSION);
	printf ("See http://www.liberouter.org/ipfixcol/ for more information.\n\n");
}

/**
 *
 * @param progname
 */
void help (char* progname)
{
	printf ("Usage: %s [-dhV] [-v level]\n", progname);
	printf ("  -c file   Path to configuration file (%s by default)\n", DEFAULT_CONFIG_FILE);
	printf ("  -d        Daemonize\n");
	printf ("  -h        Print this help\n");
	printf ("  -v level  Print verbose messages up to specified level\n");
	printf ("  -V        Print version information\n\n");
}

void term_signal_handler(int sig)
{
	if (done) {
		VERBOSE(CL_VERBOSE_OFF, "Another termination signal (%i) detected - quiting without cleanup.", sig);
		exit (EXIT_FAILURE);
	} else {
		VERBOSE(CL_VERBOSE_OFF, "Signal: %i detected, will exit as soon as possible", sig);
		done = 1;
	}
}

int main (int argc, char* argv[])
{
	int c, i, fd, retval = 0;
	pid_t pid;
	bool daemonize = false;
	char *progname, *config_file = NULL;
	struct plugin_list* input_plugins = NULL, *storage_plugins = NULL,
	        *aux_plugins = NULL;
	struct input input;
	struct storage *storage = NULL, *aux_storage = NULL;
	void *input_plugin_handler = NULL, *storage_plugin_handler = NULL;
	struct sigaction action;

	xmlXPathObjectPtr collectors;
	xmlNodePtr collector_node;
	xmlDocPtr xml_config;
	xmlChar *plugin_params;

	/* some initialization */
	input.dll_handler = NULL;
	input.config = NULL;

	/* get program name withou execute path */
	progname = ((progname = strrchr (argv[0], '/')) != NULL) ? (progname + 1) : argv[0];

	/* parse command line parameters */
	while ((c = getopt (argc, argv, OPTSTRING)) != -1) {
		switch (c) {
		case 'c':
			config_file = optarg;
			break;
		case 'd':
			daemonize = true;
			break;
		case 'h':
			version (progname);
			help (progname);
			exit (EXIT_SUCCESS);
			break;
		case 'v':
			verbose = atoi (optarg);
			break;
		case 'V':
			version (progname);
			exit (EXIT_SUCCESS);
			break;
		default:
			VERBOSE(CL_VERBOSE_OFF, "Unknown parameter %c", c);
			help (progname);
			exit (EXIT_FAILURE);
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

	/* daemonize */
	if (daemonize) {
		debug_init(progname, 1);
		closelog();
		openlog(progname, LOG_PID, 0);
		/* and send all following messages to the syslog */
		if (daemon (1, 0)) {
			VERBOSE(CL_VERBOSE_OFF, "%s", strerror(errno));
		}
	}

	/*
	 * this initialize the library and check potential ABI mismatches
	 * between the version it was compiled for and the actual shared
	 * library used.
	 */LIBXML_TEST_VERSION
	xmlIndentTreeOutput = 1;

	/*
	 * open and prepare XML configuration file
	 */
	/* check config file */
	if (config_file == NULL) {
		/* and use default if not specified */
		config_file = DEFAULT_CONFIG_FILE;
		VERBOSE(CL_VERBOSE_BASIC, "Using default configuration file %s.", config_file);
	}

	/* TODO: this part should be in the future replaced by NETCONF configuration */
	fd = open (config_file, O_RDONLY);
	if (fd == -1) {
		VERBOSE(CL_VERBOSE_OFF, "Unable to open configuration file %s (%s)", config_file, strerror(errno));
		exit (EXIT_FAILURE);
	}
	xml_config = xmlReadFd (fd, NULL, NULL, XML_PARSE_NOERROR | XML_PARSE_NOWARNING | XML_PARSE_NOBLANKS);
	if (xml_config == NULL) {
		VERBOSE(CL_VERBOSE_OFF, "Unable to parse configuration file %s", config_file);
		close (fd);
		exit (EXIT_FAILURE);
	}
	close (fd);

	/* get collectors' specification from the configuration file */
	collectors = get_collectors (xml_config);
	if (collectors == NULL) {
		/* no collectingProcess configured */
		VERBOSE(CL_VERBOSE_OFF, "No collectingProcess configured - nothing to do.");
		retval = EXIT_FAILURE;
		goto cleanup;
	}

	/* create separate process for each <collectingProcess */
	for (i = (collectors->nodesetval->nodeNr - 1); i >= 0; i--) {

		/*
		 * fork for multiple collectors - original parent process handle only
		 * collector 0
		 */
		if (i > 0) {
			pid = fork();
			if (pid > 0) { /* parent process waits for collector 0 */
				continue;
			} else if (pid < 0) { /* error occured, fork failed */
				VERBOSE(CL_VERBOSE_OFF, "Forking collector process failed (%s), skipping collector %d.", strerror(errno), i);
				continue;
			}
			/* else child - just continue to handle plugins */
			VERBOSE(CL_VERBOSE_BASIC, "New collector process started.");
		}
		collector_node = collectors->nodesetval->nodeTab[i];
		break;
	}

	/*
	 * initialize plugins for the collector
	 */
	/* get input plugin - one */
	input_plugins = get_input_plugins (collector_node);
	if (input_plugins == NULL) {
		retval = EXIT_FAILURE;
		goto cleanup;
	}

	/* get storage plugins - at least one */
	storage_plugins = get_storage_plugins (collector_node, xml_config);
	if (storage_plugins == NULL) {
		retval = EXIT_FAILURE;
		goto cleanup;
	}

	/* prepare input plugin */
	for (i = 0; i == 0; i++) {
		input.plugin = input_plugins;
		VERBOSE(CL_VERBOSE_ADVANCED, "Opening input plugin: %s", input_plugins->file);
		input_plugin_handler = dlopen (input_plugins->file, RTLD_LAZY);
		if (input_plugin_handler == NULL) {
			VERBOSE(CL_VERBOSE_OFF, "Unable to load input plugin (%s)", dlerror());
			continue;
		}
		input.dll_handler = input_plugin_handler;

		/* prepare Input API routines */
		input.init = dlsym (input_plugin_handler, "input_init");
		if (input.init == NULL) {
			VERBOSE(CL_VERBOSE_OFF, "Unable to load input plugin (%s)", dlerror());
			continue;
		}
		input.get = dlsym (input_plugin_handler, "get_packet");
		if (input.get == NULL) {
			VERBOSE(CL_VERBOSE_OFF, "Unable to load input plugin (%s)", dlerror());
			continue;
		}
		input.close = dlsym (input_plugin_handler, "input_close");
		if (input.close == NULL) {
			VERBOSE(CL_VERBOSE_OFF, "Unable to load input plugin (%s)", dlerror());
			continue;
		}
	}
	/* check if we have found any input plugin */
	if (!input.dll_handler || !input.init || !input.get || !input.close) {
		VERBOSE(CL_VERBOSE_OFF, "Loading input plugin failed.");
		retval = EXIT_FAILURE;
		goto cleanup;
	}

	/* prepare storage plugin(s) */
	aux_plugins = storage_plugins;
	while (storage_plugins) {
		VERBOSE(CL_VERBOSE_ADVANCED, "Opening storage plugin: %s", storage_plugins->file);

		storage_plugin_handler = dlopen (storage_plugins->file, RTLD_LAZY);
		if (storage_plugin_handler == NULL) {
			VERBOSE(CL_VERBOSE_OFF, "Unable to load storage plugin (%s)", dlerror());
			goto storage_plugin_remove;
		}

		aux_storage = storage;
		storage = (struct storage*) malloc (sizeof(struct storage));
		storage->dll_handler = storage_plugin_handler;

		/* prepare Input API routines */
		storage->init = dlsym (storage_plugin_handler, "storage_init");
		if (storage->init == NULL) {
			VERBOSE(CL_VERBOSE_OFF, "Unable to load storage plugin (%s)", dlerror());
			dlclose (storage_plugin_handler);
			storage_plugin_handler = NULL;
			free (storage);
			storage = aux_storage;
			goto storage_plugin_remove;
		}
		storage->store = dlsym (storage_plugin_handler, "store_packet");
		if (storage->store == NULL) {
			VERBOSE(CL_VERBOSE_OFF, "Unable to load storage plugin (%s)", dlerror());
			dlclose (storage_plugin_handler);
			storage_plugin_handler = NULL;
			free (storage);
			storage = aux_storage;
			goto storage_plugin_remove;
		}
		storage->store_now = dlsym (storage_plugin_handler, "store_now");
		if (storage->store_now == NULL) {
			VERBOSE(CL_VERBOSE_OFF, "Unable to load storage plugin (%s)", dlerror());
			dlclose (storage_plugin_handler);
			storage_plugin_handler = NULL;
			free (storage);
			storage = aux_storage;
			goto storage_plugin_remove;
		}
		storage->close = dlsym (storage_plugin_handler, "storage_close");
		if (storage->close == NULL) {
			VERBOSE(CL_VERBOSE_OFF, "Unable to load storage plugin (%s)", dlerror());
			dlclose (storage_plugin_handler);
			storage_plugin_handler = NULL;
			free (storage);
			storage = aux_storage;
			goto storage_plugin_remove;
		}
		storage->plugin = storage_plugins;
		storage_plugins = storage_plugins->next;
		storage->plugin->next = NULL;
		storage->next = aux_storage;
		aux_plugins = storage_plugins;
		continue;

storage_plugin_remove:
		/* if something went wrong, remove the storage_plugin structure */
		storage_plugins = storage_plugins->next;
			if (aux_plugins) {
			if (aux_plugins->file) {
				free (aux_plugins->file);
			}
			if (aux_plugins->xmldata) {
				xmlFreeDoc (aux_plugins->xmldata);
			}
			free (aux_plugins);
		}
		aux_plugins = storage_plugins;
	}
	/* check if we have found at least one storage plugin */
	if (!storage) {
		VERBOSE(CL_VERBOSE_OFF, "Loading storage plugin(s) failed.");
		retval = EXIT_FAILURE;
		goto cleanup;
	}

	/*
	 * CAPTURE DATA
	 */

	/* init input plugin */
	xmlDocDumpMemory (input.plugin->xmldata, &plugin_params, NULL);
	retval = input.init ((char*) plugin_params, &(input.config));
	xmlFree (plugin_params);
	if (retval != 0) {
		VERBOSE(CL_VERBOSE_OFF, "Initiating input plugin failed.");
		goto cleanup;
	}

	/* init storage plugins */
	/* if at least one storage plugin will not be initialized, we will stop the collector */
	done = 1;
	aux_storage = storage;
	while (aux_storage) {
		xmlDocDumpMemory (aux_storage->plugin->xmldata, &plugin_params, NULL);
		retval = storage->init ((char*) plugin_params, &(aux_storage->config));
		xmlFree (plugin_params);
		aux_storage = aux_storage->next;
		if (retval == 0) {
			done = 0;
		} else {
			VERBOSE(CL_VERBOSE_OFF, "Initiating storage plugin failed.");
		}
	}

	/* main loop */
	while (!done) {
		/* get data to process */
		/* TODO */
		VERBOSE(CL_VERBOSE_BASIC, "loop");
		sleep (5);
	}

cleanup:
	/* xml cleanup */
	if (collectors) {
		xmlXPathFreeObject (collectors);
	}
	if (input_plugins) {
		if (input_plugins->file) {
			free (input_plugins->file);
		}
		if (input_plugins->xmldata) {
			xmlFreeDoc (input_plugins->xmldata);
		}
		free (input_plugins);
	}
	if (xml_config) {
		xmlFreeDoc (xml_config);
	}
	xmlCleanupParser ();
	while (storage_plugins) { /* while is just for sure, it should be always one */
		if (storage_plugins->file) {
			free (storage_plugins->file);
		}
		if (storage_plugins->xmldata) {
			xmlFreeDoc (storage_plugins->xmldata);
		}
		aux_plugins = storage_plugins->next;
		free (storage_plugins);
		storage_plugins = aux_plugins;
	}

	/* DLLs cleanup */
	if (input_plugin_handler) {
		if (input.config != NULL) {
			input.close (&(input.config));
		}
		dlclose (input_plugin_handler);
	}
	while (storage) {
		aux_storage = storage->next;
		if (storage->dll_handler) {
			if (storage->config != NULL) {
				storage->close (&(storage->config));
			}
			dlclose (storage->dll_handler);
		}
		while (storage->plugin) { /* while is just for sure, it should be always one */
			if (storage->plugin->file) {
				free (storage->plugin->file);
			}
			if (storage->plugin->xmldata) {
				xmlFreeDoc (storage->plugin->xmldata);
			}
			aux_plugins = storage->plugin->next;
			free (storage->plugin);
			storage->plugin = aux_plugins;
		}
		free (storage);
		storage = aux_storage;
	}
	VERBOSE(CL_VERBOSE_BASIC, "Closing collector.");

	return (retval);
}
