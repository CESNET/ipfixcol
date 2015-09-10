/**
 * \file configurator.c
 * \author Michal Kozubik <kozubik@cesnet.cz>
 * \brief Configurator implementation.
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

#include <ipfixcol.h>
#include <ipfixcol/profiles.h>

#include "configurator.h"
#include "config.h"
#include "queues.h"
#include "preprocessor.h"
#include "intermediate_process.h"
#include "output_manager.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <libxml2/libxml/tree.h>
#include <string.h>
#include <sys/prctl.h>
#include <dlfcn.h>
#include <unistd.h>
#include <libxml/xpath.h>
#include <libxml/xmlversion.h>

/* ID for MSG_ macros */
static const char *msg_module = "configurator";

configurator *global_config = NULL;

/* TODO !! */
#define PACKAGE "ipfixcol"

#define CHECK_ALLOC(_ptr_, _ret_)\
do {\
	if (!(_ptr_)) {\
		MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)", __FILE__, __LINE__);\
		return (_ret_);\
	}\
} while (0)

/**
 * \addtogroup internalConfig
 * \ingroup internalAPIs
 *
 * These functions implements processing of configuration data of the
 * collector (mainly plugins (re)configuration etc.).
 *
 * @{
 */

void free_startup(startup_config *startup);

/* DEBUG */
void print(configurator *config)
{
	MSG_DEBUG("", "%10.10s:              | %p -> ", "preproc", get_preprocessor_output_queue());
	if (config->startup) {
		for (int i = 0; config->startup->inter[i]; ++i) {
			MSG_DEBUG("", "%10.10s: -> %p | %p ->", config->startup->inter[i]->inter->thread_name, config->startup->inter[i]->inter->in_queue, config->startup->inter[i]->inter->out_queue);
		}
	}
	MSG_DEBUG("", "%10.10s: -> %p", "Out. Mgr", output_manager_get_in_queue());
}

/**
 * \brief Open xml document
 * 
 * \param[in] filename Path to xml file
 * \return xml document
 */
xmlDoc *config_open_xml(const char *filename)
{
	/* Open file */
	int fd = open(filename, O_RDONLY);
	if (fd == -1) {
		MSG_ERROR(msg_module, "Unable to open configuration file '%s': %s", filename, strerror(errno));
		return NULL;
	}
	
	/* Parse it */
	xmlDoc *xml_file = xmlReadFd(fd, NULL, NULL, XML_PARSE_NOERROR | XML_PARSE_NOWARNING | XML_PARSE_NOBLANKS);
	if (!xml_file) {
		MSG_ERROR(msg_module, "Unable to parse configuration file '%s'", filename);
		xml_file = NULL;
	}
	close(fd);
	
	return xml_file;
}

/**
 * \brief Initialize configurator
 */
configurator *config_init(const char *internal, const char *startup)
{
	/* Allocate memory */
	configurator *config = calloc(1, sizeof(configurator));
	CHECK_ALLOC(config, NULL);
	
	/* Save file paths */
	config->internal_file = internal;
	config->startup_file = startup;
	config->ip_id = 1; /* 0 == ALL */
	config->sp_id = 1; /* 0 == ALL */

	/* Open startup.xml */
	config->act_doc = config_open_xml(startup);
	if (!config->act_doc) {
		free(config);
		return NULL;
	}

	global_config = config;
	
	return config;
}

/**
 * \brief Close plugin and free its resources
 * 
 * \param[in] plugin
 */
void config_free_plugin(struct plugin_config *plugin)
{
	/* Close plugin */
	switch (plugin->type) {
	case PLUGIN_INPUT:
		if (plugin->input) {
			if (plugin->input->dll_handler) {
				if (plugin->input->config) {
					plugin->input->close(&(plugin->input->config));
				}

				dlclose(plugin->input->dll_handler);
			}

			/* Input is pointer to configurator structure; don't free it */
			// free(plugin->input);
		}
		
		break;
	case PLUGIN_INTER:
		if (plugin->inter) {
			if (plugin->inter->in_queue) {
				rbuffer_free(plugin->inter->in_queue);
			}
			if (plugin->inter->dll_handler) {
				plugin->inter->intermediate_close(plugin->inter->plugin_config);
				dlclose(plugin->inter->dll_handler);
			}
			free(plugin->inter);
		}
		
		break;
	case PLUGIN_STORAGE:
		if (plugin->storage) {
			if (plugin->storage->dll_handler) {
				dlclose(plugin->storage->dll_handler);
			}

			if (plugin->conf.observation_domain_id) {
				free(plugin->conf.observation_domain_id);
			}

			free(plugin->storage);
		}
		
		break;
	default:
		break;
	}
	
	/* Free resources */
	if (plugin->conf.file) {
		free(plugin->conf.file);
	}
	
	if (plugin->conf.xmldata) {
		xmlFreeDoc(plugin->conf.xmldata);
	}
	
	free(plugin);
}

/**
 * \brief Remove input plugin from running config
 * 
 * \param[in] config configurator
 * \param[in] index plugin index in array
 * \return 0 on success
 */
int config_remove_input(configurator *config, int index)
{
	struct plugin_config *plugin = config->startup->input[index];
	
	MSG_NOTICE(msg_module, "[%d] Closing input plugin %d (%s)", config->proc_id, index, plugin->conf.name);
	
	/* Free plugin */
	config_free_plugin(plugin);
	
	/* Remove it from startup config */
	config->startup->input[index] = NULL;
	return 0;
}

/**
 * \brief Remove intermediate plugin from running config
 * 
 * \param[in] config configurator
 * \param[in] index plugin index in array
 * \return 0 on success
 */
int config_remove_inter(configurator *config, int index)
{
	struct plugin_config *plugin = config->startup->inter[index];
	
	MSG_NOTICE(msg_module, "[%d] Closing intermediate plugin %d (%s)", config->proc_id, index, plugin->conf.name);
	
	struct ring_buffer *in_queue = NULL, *out_queue = NULL;
	
	/* Remove it from startup config */
	config->startup->inter[index] = NULL;
	
	/* Stop plugin */
	ip_stop(plugin->inter);
	
	/* Get plugin's queues */
	in_queue  = plugin->inter->in_queue;
	out_queue = plugin->inter->out_queue;
	
	/* Wait until output queue is empty */
	rbuffer_wait_empty(out_queue);
	
	/* Redirect queue of next plugin */
	if (config->startup->inter[index + 1]) {
		ip_change_in_queue(config->startup->inter[index + 1]->inter, in_queue);	
	} else {
		/* No next plugin, redirect Output Manager's queue */
		output_manager_set_in_queue(in_queue);
	}
	
	/* Free plugin and it's queue */
	rbuffer_free(out_queue);
	config_free_plugin(plugin);
	
	return 0;
}

/**
 * \brief Remove storage plugin from running config
 * 
 * \param[in] config configurator
 * \param[in] index plugin index in array
 * \return 0 on success
 */
int config_remove_storage(configurator *config, int index)
{
	struct plugin_config *plugin = config->startup->storage[index];
	
	MSG_NOTICE(msg_module, "[%d] Closing storage plugin %d (%s)", config->proc_id, index, plugin->conf.name);
	
	/* Remove it from startup config */
	config->startup->storage[index] = NULL;
	
	/* Close plugin in Output Manager*/
	output_manager_remove_plugin(plugin->storage->id);
	
	/* Free plugin */
	config_free_plugin(plugin);
	
	return 0;
}

/**
 * \brief Add input plugin into running configuration
 * 
 * \param[in] config configurator
 * \param[in] plugin plugin configuration
 * \param[in] index plugin index in array
 * \return 0 on success
 */
int config_add_input(configurator *config, struct plugin_config *plugin, int index)
{	
	MSG_NOTICE(msg_module, "[%d] Opening input plugin: %s", config->proc_id, plugin->conf.file);
	
	/* Save configuration */
	config->input.xml_conf = &(plugin->conf);
	
	/* Open plugin */
	config->input.dll_handler = dlopen(plugin->conf.file, RTLD_LAZY);
	if (!config->input.dll_handler) {
		MSG_ERROR(msg_module, "[%d] Unable to load input xml_conf (%s)", config->proc_id, dlerror());
		goto err;
	}
	
	/* Check API version */
	unsigned int *plugin_api_version = (unsigned int *) dlsym(config->input.dll_handler, "ipfixcol_api_version");
	if (!plugin_api_version) { /* no api version symbol */
		MSG_ERROR(msg_module, "[%d] Unable to load plugin '%s'; API version number is missing...",
				config->proc_id, plugin->conf.name, plugin->conf.file);
		goto err;
	} else if (*plugin_api_version != IPFIXCOL_API_VERSION_NUMBER) { /* wrong api version */
		MSG_ERROR(msg_module, "[%d] Unable to load plugin '%s' with version %ui; at least version %ui is required...",
				config->proc_id, plugin->conf.name, plugin->conf.file, *plugin_api_version, IPFIXCOL_API_VERSION_NUMBER);
		goto err;
	}

	/* Prepare Input API routines */
	config->input.init = dlsym(config->input.dll_handler, "input_init");
	if (!config->input.init) {
		MSG_ERROR(msg_module, "[%d] Unable to load input xml_conf (%s)", config->proc_id, dlerror());
		goto err;
	}
	
	config->input.get = dlsym(config->input.dll_handler, "get_packet");
	if (!config->input.get) {
		MSG_ERROR(msg_module, "[%d] Unable to load input xml_conf (%s)", config->proc_id, dlerror());
		goto err;
	}
	
	config->input.close = dlsym(config->input.dll_handler, "input_close");
	if (config->input.close == NULL) {
		MSG_ERROR(msg_module, "[%d] Unable to load input xml_conf (%s)", config->proc_id, dlerror());
		goto err;
	}

	/* Extend the process name variable by input name */
	snprintf(config->process_name, 16, "%s:%s", PACKAGE, plugin->conf.name);

	/* Set the process name to reflect the input name */
	prctl(PR_SET_NAME, config->process_name, 0, 0, 0);
	
	/* initialize plugin */
	xmlChar *plugin_params;
	xmlDocDumpMemory(plugin->conf.xmldata, &plugin_params, NULL);
	int retval = config->input.init((char *) plugin_params, &(config->input.config));
	xmlFree(plugin_params);
	
	if (retval != 0) {
		MSG_ERROR(msg_module, "[%d] Input xml_conf initialized failed", config->proc_id);
		goto err;
	}
	
	/* Save into an array */
	config->startup->input[index] = plugin;
	config->startup->input[index]->input = &(config->input);
	
	return 0;
	
err:
	if (config->input.dll_handler) {
		dlclose(config->input.dll_handler);
	}
	
	return 1;
}

/**
 * \brief Add intermediate plugin into running configuration
 * 
 * \param[in] config configurator
 * \param[in] plugin plugin configuration
 * \param[in] index plugin index in array
 * \return 0 on success
 */
int config_add_inter(configurator *config, struct plugin_config *plugin, int index)
{
	MSG_NOTICE(msg_module, "[%d] Opening intermediate xml_conf: %s", config->proc_id, plugin->conf.file);
	
	struct intermediate *im_plugin = calloc(1, sizeof(struct intermediate));
	CHECK_ALLOC(im_plugin, 1);
	
	/* Save xml config */
	im_plugin->xml_conf = &(plugin->conf);
	
	im_plugin->dll_handler = dlopen(plugin->conf.file, RTLD_LAZY);
	if (im_plugin->dll_handler == NULL) {
		MSG_ERROR(msg_module, "[%d] Unable to load intermediate xml_conf (%s)", config->proc_id, dlerror());
		goto err;
	}
	
	/* Set intermediate thread name */
	snprintf(im_plugin->thread_name, 16, "med:%s", plugin->conf.name);
	
	/* Check API version */
	unsigned int *plugin_api_version = (unsigned int *) dlsym(im_plugin->dll_handler, "ipfixcol_api_version");
	if (!plugin_api_version) { /* no api version symbol */
		MSG_ERROR(msg_module, "[%d] Unable to load plugin '%s'; API version number is missing...",
				config->proc_id, plugin->conf.name, plugin->conf.file);
		goto err;
	} else if (*plugin_api_version != IPFIXCOL_API_VERSION_NUMBER) { /* wrong api version */
		MSG_ERROR(msg_module, "[%d] Unable to load plugin '%s' with version %ui; at least version %ui is required...",
				config->proc_id, plugin->conf.name, plugin->conf.file, *plugin_api_version, IPFIXCOL_API_VERSION_NUMBER);
		goto err;
	}

	/* Prepare Input API routines */
	im_plugin->intermediate_process_message = dlsym(im_plugin->dll_handler, "intermediate_process_message");
	if (!im_plugin->intermediate_process_message) {
		MSG_ERROR(msg_module, "Unable to load intermediate xml_conf (%s)", dlerror());
		goto err;
	}
	
	im_plugin->intermediate_init = dlsym(im_plugin->dll_handler, "intermediate_init");
	if (im_plugin->intermediate_init == NULL) {
		MSG_ERROR(msg_module, "Unable to load intermediate xml_conf (%s)", dlerror());
		goto err;
	}

	im_plugin->intermediate_close = dlsym(im_plugin->dll_handler, "intermediate_close");
	if (im_plugin->intermediate_close == NULL) {
		MSG_ERROR(msg_module, "Unable to load intermediate xml_conf (%s)", dlerror());
		goto err;
	}

	/* Create new output buffer for plugin */
	im_plugin->out_queue = rbuffer_init(ring_buffer_size);
	
	/* Set input queue */
	/* Find previous plugin */
	for (int i = index - 1; i >= 0; --i) {
		/* Found some previous plugin */
		if (config->startup->inter[i]) {
			im_plugin->in_queue = config->startup->inter[i]->inter->out_queue;
			break;
		}
	}
	
	/* No plugin before this one, input == preprocessor's output */
	if (!im_plugin->in_queue) {
		im_plugin->in_queue = get_preprocessor_output_queue();
	}
	
	struct ring_buffer *backup_queue;
	
	/* Set input queue of next plugin */
	if (config->startup->inter[index + 1]) {
		backup_queue = config->startup->inter[index + 1]->inter->in_queue;
		ip_change_in_queue(config->startup->inter[index + 1]->inter, im_plugin->out_queue);
	} else {
		backup_queue = output_manager_get_in_queue();
		output_manager_set_in_queue(im_plugin->out_queue);
	}
	
	/* Start plugin */
	if (ip_init(im_plugin, config->ip_id) != 0) {
		/* Restore queues */
		if (config->startup->inter[index + 1]) {
			ip_change_in_queue(config->startup->inter[index + 1]->inter, backup_queue);
		} else {
			output_manager_set_in_queue(backup_queue);
		}
		goto err;
	}
	
	config->ip_id++;
	
	/* Save data into an array */
	config->startup->inter[index] = plugin;
	config->startup->inter[index]->inter = im_plugin;
	
	return 0;
	
err:
	if (im_plugin) {
		if (im_plugin->dll_handler) {
			dlclose(im_plugin->dll_handler);
		}
		free(im_plugin);
	}
	
	return 1;
}

/**
 * \brief Add storage plugin into running configuration
 * 
 * \param[in] config configurator
 * \param[in] plugin plugin configuration
 * \param[in] index plugin index in array
 * \return 0 on success
 */
int config_add_storage(configurator *config, struct plugin_config *plugin, int index)
{
	MSG_NOTICE(msg_module, "[%d] Opening storage xml_conf: %s", config->proc_id, plugin->conf.file);
	
	/* Create storage plugin structure */
	struct storage *st_plugin = calloc(1, sizeof(struct storage));
	CHECK_ALLOC(st_plugin, 1);
	
	/* Save xml config */
	st_plugin->xml_conf = &(plugin->conf);
	
	st_plugin->dll_handler = dlopen(plugin->conf.file, RTLD_LAZY);
	if (!st_plugin->dll_handler) {
		MSG_ERROR(msg_module, "[%d] Unable to load storage xml_conf (%s)", config->proc_id, dlerror());
		goto err;
	}
	
	/* Set storage thread name */
	snprintf(st_plugin->thread_name, 16, "out:%s", plugin->conf.name);
	
	/* Check API version */
	unsigned int *plugin_api_version = (unsigned int *) dlsym(st_plugin->dll_handler, "ipfixcol_api_version");
	if (!plugin_api_version) { /* no api version symbol */
		MSG_ERROR(msg_module, "[%d] Unable to load plugin '%s'; API version number is missing...",
				config->proc_id, plugin->conf.name, plugin->conf.file);
		goto err;
	} else if (*plugin_api_version != IPFIXCOL_API_VERSION_NUMBER) { /* wrong api version */
		MSG_ERROR(msg_module, "[%d] Unable to load plugin '%s' with version %ui; at least version %ui is required...",
				config->proc_id, plugin->conf.name, plugin->conf.file, *plugin_api_version, IPFIXCOL_API_VERSION_NUMBER);
		goto err;
	}

	/* Prepare Input API routines */
	st_plugin->init = dlsym(st_plugin->dll_handler, "storage_init");
	if (!st_plugin->init) {
		MSG_ERROR(msg_module, "[%d] Unable to load storage xml_conf (%s)", config->proc_id, dlerror());
		goto err;
	}
	
	st_plugin->store = dlsym(st_plugin->dll_handler, "store_packet");
	if (!st_plugin->store) {
		MSG_ERROR(msg_module, "[%d] Unable to load storage xml_conf (%s)", config->proc_id, dlerror());
		goto err;
	}
	
	st_plugin->store_now = dlsym(st_plugin->dll_handler, "store_now");
	if (!st_plugin->store_now) {
		MSG_ERROR(msg_module, "[%d] Unable to load storage xml_conf (%s)", config->proc_id, dlerror());
		goto err;
	}
	
	st_plugin->close = dlsym(st_plugin->dll_handler, "storage_close");
	if (!st_plugin->close) {
		MSG_ERROR(msg_module, "[%d] Unable to load storage xml_conf (%s)", config->proc_id, dlerror());
		goto err;
	}
	
	/* Set plugin id */
	st_plugin->id = config->sp_id;
	
	/* Add plugin to Output Manager */
	if (output_manager_add_plugin(st_plugin) != 0) {
		MSG_ERROR(msg_module, "[%d] Unable to add plugin to Output Manager", config->proc_id);
		goto err;
	}
	
	config->sp_id++;
	
	/* Save data into an array */
	config->startup->storage[index] = plugin;
	config->startup->storage[index]->storage = st_plugin;
	
	return 0;
	
err:	
	if (st_plugin) {
		if (st_plugin->dll_handler) {
			/* Close dll handler */
			dlclose(st_plugin->dll_handler);
		}
		/* Free plugin structure */
		free(st_plugin);
	}
	
	return 1;
}

/**
 * \brief Compare two plugin configurations
 * 
 * \param[in] first first config
 * \param[in] second second config
 * \return 0 if configurations are the same
 */
int config_compare_xml(struct plugin_xml_conf *first, struct plugin_xml_conf *second)
{
	/* Compare plugin name, file path and ODID */
	if (   strcmp(first->file, second->file)
		|| strcmp(first->name, second->name)) {
		return 1;
	}
	
	/* TODO: memory management!! */
	
	/* Compare XML content */
	xmlNode *fnode, *snode;
	fnode = xmlDocGetRootElement(first->xmldata);
	snode = xmlDocGetRootElement(second->xmldata);
	
	const char *scontent, *fcontent;
	
#if LIBXML_VERSION < 20900
	/* Old API without xmlBuf */
	xmlBufferPtr fbuf, sbuf;
#else
	/* New API with xmlBuf */
	xmlBufPtr fbuf, sbuf;
#endif
	
	/* Allocate space for buffers */
	fbuf = calloc(1, 2000);
	CHECK_ALLOC(fbuf, 1);
	
	sbuf = calloc(1, 2000);
	if (!sbuf) {
		MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)", __FILE__, __LINE__);
		free(fbuf);
		return 1;
	}
	
#if LIBXML_VERSION < 20900
	/* Old API */
	/* Get first subtree */
	xmlNodeBufGetContent(fbuf, fnode);
	fcontent = (const char *) xmlBufferContent(fbuf);
	
	/* Get second subtree */
	xmlNodeBufGetContent(sbuf, snode);
	scontent = (const char *) xmlBufferContent(sbuf);
#else
	/* New API */
	/* Get first subtree */
	xmlBufGetNodeContent(fbuf, fnode);
	fcontent = (const char *) xmlBufContent(fbuf);
	
	/* Get second subtree */
	xmlBufGetNodeContent(sbuf, snode);
	scontent = (const char *) xmlBufContent(sbuf);
#endif
	
	/* Compare contents of subtrees */
	int ret = 0;
	if (fcontent && scontent) {
		/* Both have contents */
		ret = strcmp(fcontent, scontent);
	} else if (!fcontent && !scontent) {
		/* Both without contents */
		ret = 0;
	} else {
		/* One with content and second without contents */
		ret = 1;
	}
	
	xmlBufferFree((xmlBufferPtr) fbuf);
	xmlBufferFree((xmlBufferPtr) sbuf);
	
	return ret;
}

/**
 * \brief Remove plugin from running config
 * 
 * \param[in] config configurator
 * \param[in] index plugin index in array
 * \param[in] type plugin type
 * \return 0 on success
 */
int config_remove(configurator *config, int index, int type)
{
	switch (type) {
	case PLUGIN_INPUT:   return config_remove_input(config, index);
	case PLUGIN_INTER:   return config_remove_inter(config, index);
	case PLUGIN_STORAGE: return config_remove_storage(config, index);
	default: break;
	}
	
	return 0;
}

/**
 * \brief Add plugin into running config
 * 
 * \param[in] config configurator
 * \param[in] plugin plugin configuration
 * \param[in] index plugin index in array
 * \param[in] type plugin type
 * \return 0 on success
 */
int config_add(configurator *config, struct plugin_config *plugin, int index, int type)
{
	plugin->type = type;
	
	switch (type) {
	case PLUGIN_INPUT:   return config_add_input(config, plugin, index);
	case PLUGIN_INTER:   return config_add_inter(config, plugin, index);
	case PLUGIN_STORAGE: return config_add_storage(config, plugin, index);
	default: break;
	}
	
	return 0;
}

/**
 * \brief Process changes in plugins
 * 
 * \param[in] config configurator
 * \param[in] old_plugins array of actually used plugins
 * \param[in] new_plugins array of new configurations
 * \param[in] type plugins type
 * \return 0 on success
 */
int config_process_changes(configurator *config, struct plugin_config *old_plugins[], struct plugin_config *new_plugins[], int type)
{
	int plugs = 0, old_plugs = 0, found, i, j;
	
	/* Get number of plugins in configurations */
	while (old_plugins[old_plugs]) old_plugs++;
	while (new_plugins[plugs]) plugs++;
	
	/* Check plugins */
	for (i = 0; i < old_plugs; ++i) {
		/* Array may have holes (removed plugins) */
		if (!old_plugins[i]) {
			continue;
		}
		
		found = 0;
		
		for (j = 0; j < plugs; ++j) {
			/* Array may have holes (processed plugins) */
			if (!new_plugins[j]) {
				continue;
			}
			
			/* Find plugins with same names */
			if (!strcmp(old_plugins[i]->conf.name, new_plugins[j]->conf.name)) {
				/* Compare configurations */
				if (config_compare_xml(&(old_plugins[i]->conf), &(new_plugins[j]->conf)) == 0) {
					/* Same configurations - nothing changed, only position (needed only for intermediate plugins) */
					if (type == PLUGIN_INTER && i != j) {
						/* move */
						config_remove(config, i, type);
					} else {
						new_plugins[j] = NULL;
					}
				} else {
					/* Different configurations - remove old, add new plugin */
					config_remove(config, i, type);
				}
				
				found = 1;
				break;
			}
		}
		
		/* Was plugin found in new configuration? */
		if (!found) {
			/* If not, remove it from running config */
			config_remove(config, i, type);
		}
	}
	
	/* Add new plugins */
	for (j = 0; j < plugs; ++j) {
		if (new_plugins[j]) {
			/* New plugin */
			if (config_add(config, new_plugins[j], j, type) != 0) {
				return 1;
			}
			
			new_plugins[j] = NULL;
		}
	}
	
	return 0;
}

/**
 * \brief Free list of plugin configurations
 * 
 * \param[in] list
 */
void free_conf_list(struct plugin_xml_conf_list *list) 
{
	struct plugin_xml_conf_list *aux_list;
	while (list) {
		aux_list = list->next;
		free(list);
		list = aux_list;
	}
}

char *config_get_new_profiles_file(configurator *config)
{
	xmlNode *aux_node;
	for (aux_node = config->collector_node->children; aux_node; aux_node = aux_node->next) {
		if (!xmlStrcmp(aux_node->name, (xmlChar *) "profiles")) {
			return (char *) xmlNodeGetContent(aux_node);
		}
	}

	return NULL;
}

/**
 * \brief Create startup configuration
 * 
 * \param[in] config configurator
 */
startup_config *config_create_startup(configurator *config)
{
	/* Allocate memory */
	startup_config *startup = calloc(1, sizeof(startup_config));
	CHECK_ALLOC(startup, NULL);
	
	struct plugin_xml_conf_list *aux_list, *aux_conf;
	int i = 0, found = 0;
	
	/* Get new collector's node */
	xmlNode *collector_node = NULL, *aux_node, *aux_collector;
	xmlChar *collector_name = NULL, *aux_name;
	bool single_mgr_warning = false;
	
	/* Get actual collector name */
	for (aux_node = config->collector_node->children; aux_node; aux_node = aux_node->next) {
		if (!xmlStrcmp(aux_node->name, (xmlChar *) "name")) {
			collector_name = xmlNodeGetContent(aux_node);
			break;
		}
	}
	
	/* Get all collectors */
	xmlXPathObject *collectors = get_collectors(config->new_doc);
	if (collectors == NULL) {
		/* no collectingProcess configured */
		MSG_ERROR(msg_module, "No collector process found");
		goto err;
	}
	
	/* Find node of this collector */
	for (i = (collectors->nodesetval->nodeNr - 1); i >= 0; i--) {
		found = 0;
		
		/* Find new collector name */
		aux_collector = collectors->nodesetval->nodeTab[i];
		for (aux_node = aux_collector->children; aux_node; aux_node = aux_node->next) {
			if (!xmlStrcmp(aux_node->name, (xmlChar *) "name")) {
				aux_name = xmlNodeGetContent(aux_node);
				
				if (!xmlStrcmp(aux_name, collector_name)) {
					/* Found new collector node */
					collector_node = aux_collector;
					free(aux_name);
					found = 1;
					break;
				}
				
				free(aux_name);
			}
		}
		
		/* Collector node found */
		if (found) {
			break;
		}
	}
	
	xmlXPathFreeObject (collectors);
	xmlFree(collector_name);
	
	if (!collector_node) {
		MSG_ERROR(msg_module, "No collector process found");
		goto err;
	}
	
	config->collector_node = collector_node;
	
	/* Get input plugins */
	aux_list = get_input_plugins(config->collector_node, (char *) config->internal_file);
	if (!aux_list) {
		goto err;
	}
	
	/* Store them into array */
	i = 0;
	for (aux_conf = aux_list; aux_conf; aux_conf = aux_conf->next, ++i) {
		startup->input[i] = calloc(1, sizeof(struct plugin_config));
		
		if (!startup->input[i]) {
			MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)", __FILE__, __LINE__);
			free_conf_list(aux_list);
			free_startup(startup);
			return NULL;
		}

		startup->input[i]->conf = aux_conf->config;
	}
	startup->input[i] = NULL;
	free_conf_list(aux_list);
	
	/* Get storage plugins */
	aux_list = get_storage_plugins(config->collector_node, config->new_doc, (char *) config->internal_file);
	if (!aux_list) {
		goto err;
	}
	
	/* Store them into array */
	i = 0;
	startup->single_data_manager = false;
	for (aux_conf = aux_list; aux_conf; aux_conf = aux_conf->next, ++i) {
		startup->storage[i] = calloc(1, sizeof(struct plugin_config));

		if (!startup->storage[i]) {
			MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)", __FILE__, __LINE__);
			free_conf_list(aux_list);
			free_startup(startup);
			return NULL;
		}
		
		startup->storage[i]->conf = aux_conf->config;

		// Check if configuration will run with single data manager
		if (startup->single_data_manager == aux_conf->config.require_single_manager ||
				single_mgr_warning) {
			continue;
		}

		if (i != 0) {
			// There is at least one plugin without required single data mngr
			MSG_WARNING(msg_module, "All storage plugins will run with single data manager");
			single_mgr_warning = true;
		}

		if (aux_conf->config.require_single_manager) {
			startup->single_data_manager = true;
		}
	}
	startup->storage[i] = NULL;
	free_conf_list(aux_list);
	
	/* Get (optional) intermediate plugins */
	aux_list = get_intermediate_plugins(config->new_doc, (char *) config->internal_file);
	
	/* Store them into array */
	i = 0;
	for (aux_conf = aux_list; aux_conf; aux_conf = aux_conf->next, ++i) {
		startup->inter[i] = calloc(1, sizeof(struct plugin_config));

		if (!startup->inter[i]) {
			MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)", __FILE__, __LINE__);
			free_conf_list(aux_list);
			free_startup(startup);
			return NULL;
		}

		startup->inter[i]->conf = aux_conf->config;
	}
	startup->inter[i] = NULL;
	free_conf_list(aux_list);
	
	return startup;
	
err:
	if (startup) {
		free_startup(startup);
	}
	
	return NULL;
}

/**
 * \brief Apply new startup configuration
 * 
 * \param[in] config configurator
 * \param[in] new_startup new startup configuration
 * \return 0 on success
 */
int config_process_new_startup(configurator *config, startup_config *new_startup)
{
	int i;
	
	if (!config->startup) {
		/* First configuration - allocate memory */
		config->startup = calloc(1, sizeof(startup_config));
		CHECK_ALLOC(config->startup, 1);
		
		/* Add all input plugins */
		for (i = 0; new_startup->input[i]; ++i) {
			if (config_add(config, new_startup->input[i], i, PLUGIN_INPUT) != 0) {
				goto process_err;
			}
			new_startup->input[i] = NULL;
		}
		
		/* Add all intermediate plugins */
		for (i = 0; new_startup->inter[i]; ++i) {
			if (config_add(config, new_startup->inter[i], i, PLUGIN_INTER) != 0) {
				goto process_err;
			}
			new_startup->inter[i] = NULL;
		}
		
		/* Add all storage plugins */
		for (i = 0; new_startup->storage[i]; ++i) {
			if (config_add(config, new_startup->storage[i], i, PLUGIN_STORAGE) != 0) {
				goto process_err;
			}
			new_startup->storage[i] = NULL;
		}

		/* Manager mode */
		if (new_startup->single_data_manager) {
			output_manager_set_mode(OM_SINGLE);
		}
		
		return 0;
	}
	
	// Process input plugins changes
	if (config_process_changes(config, config->startup->input,   new_startup->input,   PLUGIN_INPUT) != 0) {
		return 1;
	}
	
	// Process intermediate plugins changes
	if (config_process_changes(config, config->startup->inter,   new_startup->inter,   PLUGIN_INTER) != 0) {
		return 1;
	}

	// Check if output manager mode is still same
	if (config->startup->single_data_manager != new_startup->single_data_manager) {
		bool single_mode = new_startup->single_data_manager;

		// Single data manager vs. multiple data managers
		MSG_WARNING(msg_module, "Output data manager mode will be set to %s mode",
			single_mode ? "single" : "multiple");

		// Delete all data managers (and plugins)
		if (output_manager_set_mode(single_mode ? OM_SINGLE : OM_MULTIPLE) != 0) {
			return 1;
		}

		// Done, new plugins will be dynamically created by output manager
		config->startup->single_data_manager = new_startup->single_data_manager;
	}

	// Process storage plugins changes
	if (config_process_changes(config, config->startup->storage, new_startup->storage, PLUGIN_STORAGE) != 0) {
		return 1;
	}
	
	return 0;
	
process_err:
	if (config->startup) {
		free_startup(config->startup);
		config->startup = NULL;
	}
	
	return 1;
}

/**
 * \brief Free startup structure
 * 
 * \param[in] startup startup structure
 */
void free_startup(startup_config *startup)
{
	int i;

	/* Close and free input plugins */
	for (i = 0; startup->input[i]; ++i) {
		config_free_plugin(startup->input[i]);
	}

	/* Close and free storage plugins */
	for (i = 0; startup->storage[i]; ++i) {
		config_free_plugin(startup->storage[i]);
	}

	/* Close and free intermediate plugins */
	for (i = 0; startup->inter[i]; ++i) {
		config_free_plugin(startup->inter[i]);
	}

	/* Free startup configuration */
	free(startup);	
}

/**
 * \brief Replace current profiles with the new configuration
 *
 * \param[in] config configurator
 * \param[in] profiles new profiles
 */
void config_replace_profiles(configurator *config, void *profiles)
{
	if (config->profiles[config->current_profiles]) {
		config->current_profiles++;
	}

	if (config->current_profiles == MAX_PROFILES_CONFIGS) {
		config->current_profiles = 0;
	}

	/* Free old profiles */
	if (config->profiles[config->current_profiles] != NULL) {
		profiles_free(config->profiles[config->current_profiles]);
	}

	config->profiles[config->current_profiles] = profiles;
}

/**
 * Get current profiles
 */
void *config_get_current_profiles(configurator *config)
{
	return config->profiles[config->current_profiles];
}

/**
 * \brief Process profiles configuration
 *
 * \param[in] config configurator
 */
void config_process_profiles(configurator *config)
{
	if (!config->profiles_file) {
		MSG_NOTICE(msg_module, "No profile configuration");
		config_replace_profiles(config, NULL);
		return;
	}

	/* Compare paths */
	if (config->profiles_file_old && !strcmp(config->profiles_file_old, config->profiles_file)) {
		/* Path are the same, compare timestamps */
		struct stat st;
		if (stat(config->profiles_file, &st) != 0) {
			MSG_ERROR(msg_module, "Cannot process profiles file %s: %s", config->profiles_file, strerror(errno));
			free(config->profiles_file);
			config->profiles_file = config->profiles_file_old;
			return;
		}

		if (config->profiles_file_tstamp == st.st_mtim.tv_sec) {
			/* Files are the same */
			free(config->profiles_file);
			config->profiles_file = config->profiles_file_old;
			return;
		}

		/* Save modification time */
		config->profiles_file_tstamp = st.st_mtim.tv_sec;
	}

	/* Process XML file */
	void *profiles = profiles_process_xml(config->profiles_file);
	if (!profiles) {
		free(config->profiles_file);
		MSG_ERROR(msg_module, "Cannot parse new profiles configuration; keeping old configuration...");
		config->profiles_file = config->profiles_file_old;
		return;
	}

	/* Replace profiles */
	config_replace_profiles(config, profiles);
	if (config->profiles_file_old) {
		free(config->profiles_file_old);
	}

	/* Save filename */
	config->profiles_file_old = config->profiles_file;
}

/**
 * \brief Reload IPFIXcol startup configuration
 */
int config_reconf(configurator *config)
{	
	/* Create startup configuration from updated xml file */
	config->new_doc = config_open_xml(config->startup_file);
	if (!config->new_doc) {
		return 1;
	}
	
	startup_config *new_startup = config_create_startup(config);
	if (!new_startup) {
		xmlFreeDoc(config->new_doc);
		return 1;
	}
	
	config->profiles_file = config_get_new_profiles_file(config);

	/* Process changes */
	int ret = config_process_new_startup(config, new_startup);

	/* Process profiles configuration */
	config_process_profiles(config);
	
	if (ret == 0) {
		/* Set output manager's input queue */
		int i;

		/* Get last intermediate plugin */
		for (i = 0; config->startup->inter[i]; ++i) {}

		if (i == 0) {
			/* None? Set preprocessor's output */
			output_manager_set_in_queue(get_preprocessor_output_queue());
		} else {
			output_manager_set_in_queue(config->startup->inter[i - 1]->inter->out_queue);
		}
	}
	
	/* Free resources */
	free_startup(new_startup);
	
	/* Replace startup xml */
	xmlFreeDoc(config->act_doc);
	config->act_doc = config->new_doc;
	
	return ret;
}

/**
 * Stop all intermediate plugins and flush buffers
 * Output Manager MUST be closed AFTER calling this function
 * otherwise data from intermediate plugins buffers will be lost
 */
void config_stop_inter(configurator *config)
{
	if (config->startup) {
		int i;
		for (i = 0; config->startup->inter[i]; ++i) {
			ip_stop(config->startup->inter[i]->inter);
		}
	}
}

/**
 * \brief Destroy configurator structure
 */
void config_destroy(configurator *config)
{
	/* Close startup xml */
	if (config->act_doc) {
		xmlFreeDoc(config->act_doc);
		config->act_doc = NULL;
	}
	
	if (config->startup) {
		free_startup(config->startup);
	}
	
	/* Free all profile trees */
	for (int i = 0; i < MAX_PROFILES_CONFIGS; ++i) {
		if (config->profiles[i]) {
			profiles_free(config->profiles[i]);
		}
	}

	/* Free path to the profiles configuration */
	if (config->profiles_file) {
		free(config->profiles_file);
	}

	/* Free configurator */
	free(config);
}

const char *profiles_get_xml_path()
{
	return (global_config != NULL) ? global_config->profiles_file : NULL;
}

/**@}*/
