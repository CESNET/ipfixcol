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

#if HAVE_CONFIG_H
#include <pkgconfig.h>
#endif
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
#include <signal.h>
#include <syslog.h>
#include <sys/wait.h>
#include <pthread.h>
#include <sys/prctl.h>

#include <libxml/parser.h>
#include <libxml/xpath.h>

#include <ipfixcol.h>
#include "intermediate_process.h"
#include "config.h"
#include "preprocessor.h"
#include "output_manager.h"

/**
 * \defgroup internalAPIs ipfixcol's Internal APIs
 *
 * Internal ipfixcol's functions not supposed to be used outside the collector
 * core.
 *
 */


/** Acceptable command-line parameters */
#define OPTSTRING "c:dhv:Vsr:i:S:e:"

/* Path to ipfix-elements.xml file */
const char *ipfix_elements = DEFAULT_IPFIX_ELEMENTS;

/* main loop indicator */
volatile int done = 0;

/** Identifier to MSG_* macros */
static char *msg_module = "main";

/** Ring buffer size */
int ring_buffer_size = 8192;

/**
 * \brief Print program version information
 */
void version ()
{
	printf ("%s: IPFIX Collector capture daemon.\n", PACKAGE);
	printf ("Version: %s, Copyright (C) 2011 CESNET z.s.p.o.\n", VERSION);
	printf ("See http://www.liberouter.org/technologies/ipfixcol/ for more information.\n\n");
}

/**
 * \brief Print program help
 */
void help ()
{
	printf ("Usage: %s [-c file] [-i file] [-dhVs] [-v level]\n", PACKAGE);
	printf ("  -c file   Path to configuration file (%s by default)\n", DEFAULT_CONFIG_FILE);
	printf ("  -i file   Path to internal configuration file (%s by default)\n", INTERNAL_CONFIG_FILE);
	printf ("  -e file   Path to ipfix-elements.xml file (%s by default)\n", DEFAULT_IPFIX_ELEMENTS);
	printf ("  -d        Daemonize\n");
	printf ("  -h        Print this help\n");
	printf ("  -v level  Print verbose messages up to specified level\n");
	printf ("  -V        Print version information\n");
	printf ("  -s        Skip invalid sequence number error (especially when receiving Netflow v9 data format)\n");
	printf ("  -r        Ring buffer size\n");
	printf ("  -S num    Print proccessing statistics every \"num\" seconds\n");
	printf ("\n");
}

void term_signal_handler(int sig)
{
	if (done) {
		MSG_COMMON(ICMSG_ERROR, "Another termination signal (%i) detected - quiting without cleanup.", sig);
		exit (EXIT_FAILURE);
	} else {
		MSG_COMMON(ICMSG_ERROR, "Signal: %i detected, will exit as soon as possible", sig);
		done = 1;
	}
}

int main (int argc, char* argv[])
{
	int c, i, fd, retval = 0, get_retval, proc_count = 0, proc_id = 0, collector_mode = 0;
	int source_status = SOURCE_STATUS_OPENED, stat_interval = 0;
	pid_t pid = 0;
	bool daemonize = false;
	char *config_file = NULL, *internal_file = NULL;
	char process_name[16]; /* name of the process for ps auxc */
	struct plugin_xml_conf_list* input_plugins = NULL, *storage_plugins = NULL,
	        *aux_plugins = NULL, *intermediate_plugins = NULL;;
	struct input input;
	struct storage_list *storage_list = NULL, *aux_storage_list = NULL;
	void *input_plugin_handler = NULL, *storage_plugin_handler = NULL, *intermediate_plugin_handler = NULL;
	struct intermediate_list *intermediate_list = NULL, *aux_intermediate_list = NULL;
	struct sigaction action;
	char *packet = NULL;
	struct input_info* input_info;
	struct ring_buffer *in_queue = NULL, *out_queue = NULL, *aux_queue = NULL, *preprocessor_output_queue = NULL;
	struct ipfix_template_mgr *template_mgr = NULL;
	void *output_manager_config = NULL;
	uint8_t core_initialized = 0;
	uint32_t ip_id = 0;

	xmlXPathObjectPtr collectors;
	xmlNodePtr collector_node = NULL;
	xmlDocPtr xml_config;
	xmlChar *plugin_params;
	xmlChar *ip_params;

	/* some initialization */
	input.dll_handler = NULL;
	input.config = NULL;

	int ring_buffer_size = 8192;

	/* parse command line parameters */
	while ((c = getopt (argc, argv, OPTSTRING)) != -1) {
		switch (c) {
		case 'c':
			config_file = optarg;
			break;
		case 'i':
			internal_file = optarg;
			break;
		case 'd':
			daemonize = true;
			break;
		case 'h':
			version ();
			help ();
			exit (EXIT_SUCCESS);
			break;
		case 'v':
			verbose = atoi (optarg);
			break;
		case 'V':
			version ();
			exit (EXIT_SUCCESS);
			break;
		case 's':
			skip_seq_err = 1;
			break;
		case 'r':
			ring_buffer_size = atoi (optarg);
			break;
		case 'S':
			stat_interval = atoi(optarg);
			break;
		case 'e':
			ipfix_elements = optarg;
			break;
		default:
			MSG_ERROR(msg_module, "Unknown parameter %c", c);
			help ();
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
		MSG_NOTICE(msg_module, "Using default configuration file %s.", config_file);
	}

	/* check internal config file */
	if (internal_file == NULL) {
		/* and use default if not specified */
		internal_file = INTERNAL_CONFIG_FILE;
		MSG_NOTICE(msg_module, "Using default internal configuration file %s.", internal_file);
	}

	/* TODO: this part should be in the future replaced by NETCONF configuration */
	fd = open (config_file, O_RDONLY);
	if (fd == -1) {
		MSG_ERROR(msg_module, "Unable to open configuration file %s (%s)", config_file, strerror(errno));
		exit (EXIT_FAILURE);
	}
	xml_config = xmlReadFd (fd, NULL, NULL, XML_PARSE_NOERROR | XML_PARSE_NOWARNING | XML_PARSE_NOBLANKS);
	if (xml_config == NULL) {
		MSG_ERROR(msg_module, "Unable to parse configuration file %s", config_file);
		close (fd);
		exit (EXIT_FAILURE);
	}
	close (fd);

	/* get collectors' specification from the configuration file */
	collectors = get_collectors (xml_config);
	if (collectors == NULL) {
		/* no collectingProcess configured */
		MSG_ERROR(msg_module, "No collectingProcess configured - nothing to do.");
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
                proc_count++;
				continue;
			} else if (pid < 0) { /* error occured, fork failed */
				MSG_ERROR(msg_module, "Forking collector process failed (%s), skipping collector %d.", strerror(errno), i);
				continue;
			}
			/* else child - just continue to handle plugins */
            proc_id = i;
            MSG_NOTICE(msg_module, "[%d] New collector process started.", proc_id);
		}
		collector_node = collectors->nodesetval->nodeTab[i];
		break;
	}

	/*
	 * create Template Manager
	 */

	template_mgr = tm_create();
	if (template_mgr == NULL) {
		MSG_ERROR(msg_module, "[%d] Unable to create Template Manager", proc_id);
		retval = EXIT_FAILURE;
		goto cleanup;
	}

	/*
	 * initialize plugins for the collector
	 */
	/* get input plugin - one */
	input_plugins = get_input_plugins (collector_node, internal_file);
	if (input_plugins == NULL) {
		retval = EXIT_FAILURE;
		goto cleanup;
	}

	/* get storage plugins - at least one */
	storage_plugins = get_storage_plugins (collector_node, xml_config, internal_file);
	if (storage_plugins == NULL) {
		retval = EXIT_FAILURE;
		goto cleanup;
	}

	intermediate_plugins = get_intermediate_plugins(xml_config, internal_file);
	if (intermediate_plugins == NULL) {
		MSG_NOTICE(msg_module, "[%d] No intermediate plugin specified. ipfixmed will act as IPFIX collector.", proc_id);
	}

	/* prepare input plugin */
	for (aux_plugins = input_plugins; aux_plugins != NULL; aux_plugins = aux_plugins->next) {
		input.xml_conf = &aux_plugins->config;
		MSG_NOTICE(msg_module, "[%d] Opening input plugin: %s", proc_id, aux_plugins->config.file);
		input_plugin_handler = dlopen (input_plugins->config.file, RTLD_LAZY);
		if (input_plugin_handler == NULL) {
			MSG_ERROR(msg_module, "[%d] Unable to load input xml_conf (%s)", proc_id, dlerror());
			continue;
		}
		input.dll_handler = input_plugin_handler;

		/* prepare Input API routines */
		input.init = dlsym (input_plugin_handler, "input_init");
		if (input.init == NULL) {
			MSG_ERROR(msg_module, "[%d] Unable to load input xml_conf (%s)", proc_id, dlerror());
			dlclose(input.dll_handler);
			continue;
		}
		input.get = dlsym (input_plugin_handler, "get_packet");
		if (input.get == NULL) {
			MSG_ERROR(msg_module, "[%d] Unable to load input xml_conf (%s)", proc_id, dlerror());
			dlclose(input.dll_handler);
			continue;
		}
		input.close = dlsym (input_plugin_handler, "input_close");
		if (input.close == NULL) {
			MSG_ERROR(msg_module, "[%d] Unable to load input xml_conf (%s)", proc_id, dlerror());
			dlclose(input.dll_handler);
			continue;
		}

		/* extend the process name variable by input name */
		snprintf(process_name, 16, "%s:%s", PACKAGE, aux_plugins->config.name);

        /* get the first one we can */
        break;
	}
	/* check if we have found any input xml_conf */
	if (!input.dll_handler || !input.init || !input.get || !input.close) {
		MSG_ERROR(msg_module, "[%d] Loading input xml_conf failed.", proc_id);
		retval = EXIT_FAILURE;
		goto cleanup;
	}

	/* set the process name to reflect the input name */
    prctl(PR_SET_NAME, process_name, 0, 0, 0);

	/* prepare storage xml_conf(s) */
	for (aux_plugins = storage_plugins; aux_plugins != NULL; aux_plugins = aux_plugins->next) {
		MSG_NOTICE(msg_module, "[%d] Opening storage xml_conf: %s", proc_id, aux_plugins->config.file);

		storage_plugin_handler = dlopen (aux_plugins->config.file, RTLD_LAZY);
		if (storage_plugin_handler == NULL) {
			MSG_ERROR(msg_module, "[%d] Unable to load storage xml_conf (%s)", proc_id, dlerror());
			continue;
		}

		aux_storage_list = storage_list;
		storage_list = (struct storage_list*) malloc (sizeof(struct storage_list));
		if (storage_list == NULL) {
			MSG_ERROR(msg_module, "[%d] Memory allocation failed (%s:%d)", proc_id, __FILE__, __LINE__);
			storage_list = aux_storage_list;
			dlclose(storage_plugin_handler);
			continue;
		}
		memset(storage_list, 0, sizeof(struct storage_list));

		storage_list->storage.dll_handler = storage_plugin_handler;
		/* set storage thread name */
		snprintf(storage_list->storage.thread_name, 16, "out:%s", aux_plugins->config.name);

		/* prepare Input API routines */
		storage_list->storage.init = dlsym (storage_plugin_handler, "storage_init");
		if (storage_list->storage.init == NULL) {
			MSG_ERROR(msg_module, "[%d] Unable to load storage xml_conf (%s)", proc_id, dlerror());
			dlclose (storage_plugin_handler);
			storage_plugin_handler = NULL;
			free (storage_list);
			storage_list = aux_storage_list;
			continue;
		}
		storage_list->storage.store = dlsym (storage_plugin_handler, "store_packet");
		if (storage_list->storage.store == NULL) {
			MSG_ERROR(msg_module, "[%d] Unable to load storage xml_conf (%s)", proc_id, dlerror());
			dlclose (storage_plugin_handler);
			storage_plugin_handler = NULL;
			free (storage_list);
			storage_list = aux_storage_list;
			continue;
		}
		storage_list->storage.store_now = dlsym (storage_plugin_handler, "store_now");
		if (storage_list->storage.store_now == NULL) {
			MSG_ERROR(msg_module, "[%d] Unable to load storage xml_conf (%s)", proc_id, dlerror());
			dlclose (storage_plugin_handler);
			storage_plugin_handler = NULL;
			free (storage_list);
			storage_list = aux_storage_list;
			continue;
		}
		storage_list->storage.close = dlsym (storage_plugin_handler, "storage_close");
		if (storage_list->storage.close == NULL) {
			MSG_ERROR(msg_module, "[%d] Unable to load storage xml_conf (%s)", proc_id, dlerror());
			dlclose (storage_plugin_handler);
			storage_plugin_handler = NULL;
			free (storage_list);
			storage_list = aux_storage_list;
			continue;
		}

		storage_list->storage.xml_conf = &aux_plugins->config;
		storage_list->next = aux_storage_list;
		continue;
	}
	/* check if we have found at least one storage plugin */
	if (!storage_list) {
		MSG_ERROR(msg_module, "[%d] Loading storage xml_conf(s) failed.", proc_id);
		retval = EXIT_FAILURE;
		goto cleanup;
	}

	/* prepare intermediate plugins */
	for (aux_plugins = intermediate_plugins; aux_plugins != NULL; aux_plugins = aux_plugins->next) {
		MSG_NOTICE(msg_module, "[%d] Opening intermediate xml_conf: %s", proc_id, aux_plugins->config.file);

		intermediate_plugin_handler = dlopen(aux_plugins->config.file, RTLD_LAZY);
		if (intermediate_plugin_handler == NULL) {
			MSG_ERROR(msg_module, "[%d] Unable to load intermediate xml_conf (%s)", proc_id, dlerror());
			continue;
		}

		aux_intermediate_list = intermediate_list;
		intermediate_list = (struct intermediate_list*) malloc (sizeof(struct intermediate_list));
		if (intermediate_list == NULL) {
			MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)", __FILE__, __LINE__);
			intermediate_list = aux_intermediate_list;
			dlclose(intermediate_plugin_handler);
			continue;
		}
		memset(intermediate_list, 0, sizeof(struct intermediate_list));

		intermediate_list->intermediate.dll_handler = intermediate_plugin_handler;
		/* set intermediate thread name */
		snprintf(intermediate_list->intermediate.thread_name, 16, "med:%s", aux_plugins->config.name);

		/* prepare Input API routines */
		intermediate_list->intermediate.intermediate_process_message = dlsym(intermediate_plugin_handler, "intermediate_process_message");
		if (intermediate_list->intermediate.intermediate_process_message == NULL) {
			MSG_ERROR(msg_module, "Unable to load intermediate xml_conf (%s)", dlerror());
			dlclose(intermediate_plugin_handler);
			intermediate_plugin_handler = NULL;
			free(intermediate_list);
			intermediate_list = aux_intermediate_list;
			continue;
		}

		intermediate_list->intermediate.intermediate_init = dlsym(intermediate_plugin_handler, "intermediate_init");
		if (intermediate_list->intermediate.intermediate_init == NULL) {
			MSG_ERROR(msg_module, "Unable to load intermediate xml_conf (%s)", dlerror());
			dlclose(intermediate_plugin_handler);
			intermediate_plugin_handler = NULL;
			free(intermediate_list);
			intermediate_list = aux_intermediate_list;
			continue;
		}

		intermediate_list->intermediate.intermediate_close = dlsym(intermediate_plugin_handler, "intermediate_close");
		if (intermediate_list->intermediate.intermediate_close == NULL) {
			MSG_ERROR(msg_module, "Unable to load intermediate xml_conf (%s)", dlerror());
			dlclose(intermediate_plugin_handler);
			intermediate_plugin_handler = NULL;
			free(intermediate_list);
			intermediate_list = aux_intermediate_list;

			continue;
		}

		intermediate_list->intermediate.xml_conf = &aux_plugins->config;
		intermediate_list->next = aux_intermediate_list;

		if (aux_intermediate_list) {
			aux_intermediate_list->prev = intermediate_list;
		}

		continue;
	}
	/* check if we have found at least one intermediate plugin */
	if (!intermediate_list) {
		MSG_NOTICE(msg_module, "[%d] Running in collector mode.", proc_id);
		collector_mode = 1;
	}

	/* daemonize */
	if (daemonize) {
		closelog();
		MSG_SYSLOG_INIT(PACKAGE);
		/* and send all following messages to the syslog */
		if (daemon (1, 0)) {
			MSG_ERROR(msg_module, "%s", strerror(errno));
		}
	}

	/*
	 * CAPTURE DATA
	 */

	/* init input xml_conf */
	xmlDocDumpMemory (input.xml_conf->xmldata, &plugin_params, NULL);
	retval = input.init ((char*) plugin_params, &(input.config));
	xmlFree (plugin_params);
	if (retval != 0) {
		MSG_ERROR(msg_module, "[%d] Initiating input xml_conf failed.", proc_id);
		goto cleanup;
	}


	if (collector_mode) {
		/* no intermediate plugins */
		in_queue = rbuffer_init(ring_buffer_size);
		preprocessor_output_queue = in_queue;

		/* preprocessor's output queue will be input queue of output manager(s) */
		aux_queue = in_queue;
	} else {
		/* there is at least one intermediate plugin */
		aux_intermediate_list = intermediate_list;
		while (aux_intermediate_list->next) {
			aux_intermediate_list = aux_intermediate_list->next;
		}


		while (aux_intermediate_list) {

			/* create input and output queue for Intermediate Process */
			out_queue = rbuffer_init(ring_buffer_size);

			if (!in_queue) {
				in_queue = rbuffer_init(ring_buffer_size);
				preprocessor_output_queue = in_queue;
			} else {
				in_queue = aux_queue;
			}

			xmlDocDumpMemory(aux_intermediate_list->intermediate.xml_conf->xmldata, &ip_params, NULL);

			/* initialize Intermediate Process */
			ip_init(in_queue, out_queue,
					&(aux_intermediate_list->intermediate),
					(char *) ip_params,
					ip_id,
					template_mgr,
					&(aux_intermediate_list->intermediate.ip_config));
			
			/* Increment source ID for next Intermediate Process */
			ip_id++;

			/* output queue of one Intermediate Process is an input queue of another */
			aux_queue = out_queue;

			aux_intermediate_list = aux_intermediate_list->prev;
		}
	}
	core_initialized = 1;

	/* configure output subsystem */
	retval = output_manager_create(storage_list, aux_queue, stat_interval, &output_manager_config);
	if (retval != 0) {
		MSG_ERROR(msg_module, "[%d] Initiating Storage Manager failed.", proc_id);
		goto cleanup;
	}

	retval = preprocessor_init(preprocessor_output_queue, template_mgr);
	if (retval != 0) {
		MSG_ERROR(msg_module, "[%d] initiating Preprocessor failed.", proc_id);
		goto cleanup;
	}

	/* main loop */
	while (!done) {
		/* get data to process */
		if ((get_retval = input.get (input.config, &input_info, &packet, &source_status)) < 0) {
			if (!done || get_retval != INPUT_INTR) { /* if interrupted and closing, it's ok */
				MSG_WARNING(msg_module, "[%d] Getting IPFIX data failed!", proc_id);
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
            	done = 1;
            }
        }
		/* distribute data to the particular Data Manager for further processing */
		preprocessor_parse_msg (packet, get_retval, input_info, storage_list, source_status);
		source_status = SOURCE_STATUS_OPENED;
		packet = NULL;
		input_info = NULL;
	}

cleanup:
	/* xml cleanup */
	if (collectors) {
		xmlXPathFreeObject (collectors);
	}

	if (input_plugins) {
		if (input_plugins->config.file) {
			free (input_plugins->config.file);
		}
		if (input_plugins->config.xmldata) {
			xmlFreeDoc (input_plugins->config.xmldata);
		}
		free (input_plugins);
	}
	if (xml_config) {
		xmlFreeDoc (xml_config);
	}

	/* wait for all intermediate processes to close */
	aux_intermediate_list = intermediate_list;
	while (aux_intermediate_list && aux_intermediate_list->next) {
		aux_intermediate_list = aux_intermediate_list->next;
	}

	/*
	 * Stop all intermediate plugins and flush buffers
	 * Plugins will be closed later - they can have data needed by storage plugins
	 * (templates etc.)
	 */
	while (aux_intermediate_list && core_initialized) {
		/* this function will wait for intermediate thread to terminate */
		ip_stop(aux_intermediate_list->intermediate.ip_config);
		aux_intermediate_list = aux_intermediate_list->prev;
	}

	/* Close whole Output Manager (including Data Managers) */
	if (output_manager_config) {
		output_manager_close(output_manager_config);
	}

	/* Close preprocessor */
	preprocessor_close();

	while (storage_plugins) { /* while is just for sure, it should be always one */
		if (storage_plugins->config.file) {
			free (storage_plugins->config.file);
		}
		if (storage_plugins->config.observation_domain_id) {
			free (storage_plugins->config.observation_domain_id);
		}
		if (storage_plugins->config.xmldata) {
			xmlFreeDoc (storage_plugins->config.xmldata);
		}
		aux_plugins = storage_plugins->next;
		free (storage_plugins);
		storage_plugins = aux_plugins;
	}

	/* Now close all intermediate plugins */
	aux_intermediate_list = intermediate_list;
	while (aux_intermediate_list) {
		ip_destroy(aux_intermediate_list->intermediate.ip_config);
		aux_intermediate_list = aux_intermediate_list->next;
	}

	while (intermediate_plugins) {
		if (intermediate_plugins->config.file) {
			free(intermediate_plugins->config.file);
		}
		if (intermediate_plugins->config.xmldata) {
			xmlFreeDoc(intermediate_plugins->config.xmldata);
		}
		if (intermediate_plugins->config.observation_domain_id) {
			free(intermediate_plugins->config.observation_domain_id);
		}
		aux_plugins = intermediate_plugins->next;
		free(intermediate_plugins);
		intermediate_plugins = aux_plugins;
	}

	/* DLLs cleanup */
	if (input_plugin_handler) {
		if (input.config != NULL) {
			input.close (&(input.config));
		}
		dlclose (input_plugin_handler);
	}

    /* all storage plugins should be closed now -> free packet */
    if (packet != NULL) {
    	free(packet);
    }

    /* free allocated resources in storages ( xml configuration closed above ) */
	while (storage_list) {
		aux_storage_list = storage_list->next;
		if (storage_list->storage.dll_handler) {
			dlclose(storage_list->storage.dll_handler);
		}
		free (storage_list);
		storage_list = aux_storage_list;
	}

    /* free allocated resources in intermediate processes */
   	while (intermediate_list) {
   		aux_intermediate_list = intermediate_list->next;
   		if (intermediate_list->intermediate.dll_handler) {
   			dlclose(intermediate_list->intermediate.dll_handler);
   		}
   		free(intermediate_list);
   		intermediate_list = aux_intermediate_list;
   	}


    /* wait for child processes */
    if (pid > 0) {
        for (i=0; i<proc_count; i++) {
            pid = wait(NULL);
            MSG_NOTICE(msg_module, "[%d] Collector child process %d terminated", proc_id, pid);
        }
        MSG_NOTICE(msg_module, "[%d] Closing collector.", proc_id);
    }

    /* destroy template manager */
	if (template_mgr) {
		tm_destroy(template_mgr);
	}

	xmlCleanupThreads();
	xmlCleanupParser ();

	return (retval);
}
