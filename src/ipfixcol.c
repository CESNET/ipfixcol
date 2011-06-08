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

#include <libxml/parser.h>
#include <libxml/xpath.h>

#include "config.h"
#include "../ipfixcol.h"

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
#define OPTSTRING "dhv:V"

#define CONFIG_FILE "/etc/ipfixcol/full_example.xml"

/* verbose from libcommlbr */
extern int verbose;

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
	printf ("\t-d        Daemonize\n");
	printf ("\t-h        Print this help\n");
	printf ("\t-v level  Print verbose messages up to specified level\n");
	printf ("\t-V        Print version information\n\n");
}

struct input {
	int (*init) (char*, void**);
	int (*get) (void*, struct input_info**, char**);
	int (*close) (void**);
	void *dll_handler;
};

struct storage {
	int (*init) (char*, void**);
	int (*store) (void*, const struct ipfix_message*, const struct ipfix_template_t*);
	int (*store_now) (const void*);
	int (*close) (void**);
	void *dll_handler;
	struct storage *next;
};

int main (int argc, char* argv[])
{
	int c, i, fd;
	bool daemonize = false;
	char *progname;
	struct plugin_list* input_plugins = NULL, *storage_plugins = NULL,
	        *aux_plugins = NULL;
	struct input input;
	struct storage *storage = NULL, *aux_storage = NULL;
	void *input_plugin_handler = NULL, *storage_plugin_handler = NULL;

	xmlXPathObjectPtr collectors;
	xmlDocPtr xml_config;

	/* some initialization */
	input.dll_handler = NULL;

	/* get program name withou execute path */
	progname = ((progname = strrchr (argv[0], '/')) != NULL) ? (progname + 1) : argv[0];

	/* parse command line parameters */
	while ((c = getopt (argc, argv, OPTSTRING)) != -1) {
		switch (c) {
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

	/* daemonize */
	if (daemonize) {
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

	/* open and prepare XML configuration file */
	/* TODO: this part should be in the future replaced by NETCONF configuration */
	if ((fd = open (CONFIG_FILE, O_RDONLY)) == -1) {
		VERBOSE(CL_VERBOSE_OFF, "Unable to open configuration file %s (%s)", CONFIG_FILE, strerror(errno));
		exit (EXIT_FAILURE);
	}
	if ((xml_config = xmlReadFd (fd, NULL, NULL, XML_PARSE_NOERROR
	        | XML_PARSE_NOWARNING | XML_PARSE_NOBLANKS)) == NULL) {
		VERBOSE(CL_VERBOSE_OFF, "Unable to parse configuration file %s", CONFIG_FILE);
		exit (EXIT_FAILURE);
	}
	close (fd);

	/* get collectors' specification from the configuration file */
	/* TODO: now we handle only one collector, but we'll have to support
	 * launching multiple collectors specified in a single configuration file
	 */
	if ((collectors = get_collectors (xml_config)) == NULL) {
		/* no collectingProcess configured */
		VERBOSE(CL_VERBOSE_OFF, "No collectingProcess configured - nothing to do.");
		exit (EXIT_FAILURE);
	}

	/* initialize plugins for the collector */
	for (i = 0; i < collectors->nodesetval->nodeNr; i++) {
		/* get input plugin - one */
		if ((input_plugins
		        = get_input_plugins (collectors->nodesetval->nodeTab[i]))
		        == NULL) {
			continue;
		}

		/* get storage plugins - at least one */
		if ((storage_plugins
		        = get_storage_plugins (collectors->nodesetval->nodeTab[i], xml_config))
		        == NULL) {
			free (input_plugins->file);
			free (input_plugins);
			continue;
		}

		/* prepare input plugin */
		VERBOSE(CL_VERBOSE_ADVANCED, "Opening input plugin: %s", input_plugins->file);

		if ((input_plugin_handler = dlopen (input_plugins->file, RTLD_LAZY))
		        == NULL) {
			VERBOSE(CL_VERBOSE_OFF, "Unable to load input plugin (%s)", dlerror());
			continue;
		}
		input.dll_handler = input_plugin_handler;

		/* prepare Input API routines */
		if ((input.init = dlsym (input_plugin_handler, "input_init")) == NULL) {
			VERBOSE(CL_VERBOSE_OFF, "Unable to load input plugin (%s)", dlerror());
			dlclose (input_plugin_handler);
			input_plugin_handler = NULL;
			continue;
		}
		if ((input.get = dlsym (input_plugin_handler, "get_packet")) == NULL) {
			VERBOSE(CL_VERBOSE_OFF, "Unable to load input plugin (%s)", dlerror());
			dlclose (input_plugin_handler);
			input_plugin_handler = NULL;
			continue;
		}
		if ((input.close = dlsym (input_plugin_handler, "input_close")) == NULL) {
			VERBOSE(CL_VERBOSE_OFF, "Unable to load input plugin (%s)", dlerror());
			dlclose (input_plugin_handler);
			input_plugin_handler = NULL;
			continue;
		}

		/* prepare storage plugins */
		aux_plugins = storage_plugins;
		while (aux_plugins) {
			VERBOSE(CL_VERBOSE_ADVANCED, "Opening storage plugin: %s", aux_plugins->file);

			if ((storage_plugin_handler = dlopen (aux_plugins->file, RTLD_LAZY))
			        == NULL) {
				VERBOSE(CL_VERBOSE_OFF, "Unable to load storage plugin (%s)", dlerror());
				continue;
			}

			aux_storage = storage;
			storage = (struct storage*) malloc (sizeof(struct storage));

			storage->dll_handler = storage_plugin_handler;

			/* prepare Input API routines */
			if ((storage->init = dlsym (storage_plugin_handler, "storage_init"))
			        == NULL) {
				VERBOSE(CL_VERBOSE_OFF, "Unable to load storage plugin (%s)", dlerror());
				dlclose (storage_plugin_handler);
				storage_plugin_handler = NULL;
				free (storage);
				storage = aux_storage;
				continue;
			}
			if ((storage->store
			        = dlsym (storage_plugin_handler, "store_packet")) == NULL) {
				VERBOSE(CL_VERBOSE_OFF, "Unable to load storage plugin (%s)", dlerror());
				dlclose (storage_plugin_handler);
				storage_plugin_handler = NULL;
				free (storage);
				storage = aux_storage;
				continue;
			}
			if ((storage->store_now
			        = dlsym (storage_plugin_handler, "store_now")) == NULL) {
				VERBOSE(CL_VERBOSE_OFF, "Unable to load storage plugin (%s)", dlerror());
				dlclose (storage_plugin_handler);
				storage_plugin_handler = NULL;
				free (storage);
				storage = aux_storage;
				continue;
			}
			if ((storage->close
			        = dlsym (storage_plugin_handler, "storage_close")) == NULL) {
				VERBOSE(CL_VERBOSE_OFF, "Unable to load storage plugin (%s)", dlerror());
				dlclose (storage_plugin_handler);
				storage_plugin_handler = NULL;
				free (storage);
				storage = aux_storage;
				continue;
			}
			storage->next = aux_storage;
			aux_plugins = aux_plugins->next;
		}

		/* finnish looking for input plugin */
		break;
	}

	/* check if we have found any input plugin */
	if (!input.dll_handler) {
		VERBOSE(CL_VERBOSE_OFF, "Input plugin initialization failed.");
		exit (EXIT_FAILURE);
	}
	/* check if we have found at least one storage plugin */
	if (!storage) {
		VERBOSE(CL_VERBOSE_OFF, "Input plugin initialization failed.");
		exit (EXIT_FAILURE);
	}

	/* start capturing data */
	/* TODO */

	/* xml cleanup */
	if (collectors) {
		xmlXPathFreeObject (collectors);
	}
	if (input_plugins) {
		if (input_plugins->file) {
			free (input_plugins->file);
		}
		if (input_plugins->xmldata) {
			xmlFreeNode (input_plugins->xmldata);
		}
		free (input_plugins);
	}
	while (storage_plugins) {
		if (storage_plugins->file) {
			free (input_plugins->file);
		}
		if (storage_plugins->xmldata) {
			xmlFreeNode (storage_plugins->xmldata);
		}
		aux_plugins = storage_plugins->next;
		free (storage_plugins);
		storage_plugins = aux_plugins;
	}
	if (xml_config) {
		xmlFreeDoc (xml_config);
	}
	xmlCleanupParser ();

	/* DLLs cleanup */
	dlclose (input_plugin_handler);

	VERBOSE(CL_VERBOSE_BASIC, "done :)");

	return (0);
}
