
#include <ipfixcol.h>

#include "configurator.h"
#include "config.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <libxml2/libxml/tree.h>
#include <string.h>
#include <sys/prctl.h>
#include <dlfcn.h>
#include <unistd.h>

/* ID for MSG_ macros */
static const char *msg_module = "configurator";

/* TODO !! */
#define PACKAGE "ipfixcol"

#define CHECK_ALLOC(_ptr_, _ret_)\
do {\
	if (!(_ptr_)) {\
		MSG_ERROR(msg_module, "Unable to allocate memory (%s:%d)", __FILE__, __LINE__);\
		return (_ret_);\
	}\
} while (0)

/**
 * \brief Open xml document
 * 
 * \param filename Path to xml file
 * \return xml document
 */
xmlDoc *config_open_xml(const char *filename)
{
	/* Open file */
	int fd = open(filename, O_RDONLY);
	if (fd == -1) {
		MSG_ERROR(msg_module, "Unable to open configuration file %s (%s)", filename, strerror(errno));
		return NULL;
	}
	
	/* Parse it */
	xmlDoc *xml_file = xmlReadFd (fd, NULL, NULL, XML_PARSE_NOERROR | XML_PARSE_NOWARNING | XML_PARSE_NOBLANKS);
	if (!xml_file) {
		MSG_ERROR(msg_module, "Unable to parse configuration file %s", filename);
		xml_file = NULL;
	}
	close (fd);
	
	return xml_file;
}

/**
 * \breif Initialize configurator
 */
configurator *config_init(const char *internal, const char *startup)
{
	/* Allocate memory */
	configurator *config = calloc(1, sizeof(configurator));
	CHECK_ALLOC(config, NULL);
	
	/* Save file paths */
	config->internal_file = internal;
	config->startup_file = startup;

	/* Open startup.xml */
	config->act_doc = config_open_xml(startup);
	if (!config->act_doc) {
		free(config);
		return NULL;
	}
	
	return config;
}

/**
 * \brief Remove input plugin from running config
 * 
 * \param config configurator
 * \param index plugin index in array
 * \return 0 on success
 */
int config_remove_input(configurator *config, int index)
{
	MSG_ERROR(msg_module, "[%d] Input remove %d", config->proc_id, index);
	
	config->startup->input[index] = NULL;
	return 0;
}

/**
 * \brief Remove intermediate plugin from running config
 * 
 * \param config configurator
 * \param index plugin index in array
 * \return 0 on success
 */
int config_remove_inter(configurator *config, int index)
{
	MSG_ERROR(msg_module, "[%d] Inter remove %d", config->proc_id, index);
	config->startup->inter[index] = NULL;
	return 0;
}

/**
 * \brief Remove storage plugin from running config
 * 
 * \param config configurator
 * \param index plugin index in array
 * \return 0 on success
 */
int config_remove_storage(configurator *config, int index)
{
	MSG_ERROR(msg_module, "[%d] Storage remove %d", config->proc_id, index);
	config->startup->storage[index] = NULL;
	return 0;
}

/**
 * \brief Add input plugin into running configuration
 * 
 * \param config configurator
 * \param plugin plugin configuration
 * \param index plugin index in array
 * \return 0 on success
 */
int config_add_input(configurator *config, struct plugin_config *plugin, int index)
{
	MSG_ERROR(msg_module, "[%d] Input add %s", config->proc_id, plugin->conf.name);
	config->startup->input[index] = plugin;
	config->startup->input[index]->input = &(config->input);
	
	MSG_NOTICE(msg_module, "[%d] Opening input plugin: %s", config->proc_id, plugin->conf.file);
	
	/* Save configuration */
	config->input.xml_conf = &(plugin->conf);
	
	/* Open plugin */
	config->input.dll_handler = dlopen(plugin->conf.file, RTLD_LAZY);
	if (!config->input.dll_handler) {
		MSG_ERROR(msg_module, "[%d] Unable to load input xml_conf (%s)", config->proc_id, dlerror());
		return 1;
	}
	
	/* prepare Input API routines */
	config->input.init = dlsym(config->input.dll_handler, "input_init");
	if (!config->input.init) {
		MSG_ERROR(msg_module, "[%d] Unable to load input xml_conf (%s)", config->proc_id, dlerror());
		dlclose(config->input.dll_handler);
		return 1;
	}
	
	config->input.get = dlsym(config->input.dll_handler, "get_packet");
	if (!config->input.get) {
		MSG_ERROR(msg_module, "[%d] Unable to load input xml_conf (%s)", config->proc_id, dlerror());
		dlclose(config->input.dll_handler);
		return 1;
	}
	
	config->input.close = dlsym(config->input.dll_handler, "input_close");
	if (config->input.close == NULL) {
		MSG_ERROR(msg_module, "[%d] Unable to load input xml_conf (%s)", config->proc_id, dlerror());
		dlclose(config->input.dll_handler);
		return 1;
	}

	/* extend the process name variable by input name */
	snprintf(config->process_name, 16, "%s:%s", PACKAGE, plugin->conf.name);

	/* set the process name to reflect the input name */
    prctl(PR_SET_NAME, config->process_name, 0, 0, 0);
	
	/* initialize plugin */
	xmlChar *plugin_params;
	xmlDocDumpMemory(plugin->conf.xmldata, &plugin_params, NULL);
	int retval = config->input.init((char*) plugin_params, &(config->input.config));
	xmlFree(plugin_params);
	
	if (retval != 0) {
		MSG_ERROR(msg_module, "[%d] Initiating input xml_conf failed.", config->proc_id);
		dlclose(config->input.dll_handler);
		return 1;
	}
	
	return 0;
}

/**
 * \brief Add intermediate plugin into running configuration
 * 
 * \param config configurator
 * \param plugin plugin configuration
 * \param index plugin index in array
 * \return 0 on success
 */
int config_add_inter(configurator *config, struct plugin_config *plugin, int index)
{
	MSG_ERROR(msg_module, "[%d] Inter add %s", config->proc_id, plugin->conf.name);
	config->startup->inter[index] = plugin;
	return 0;
}

/**
 * \brief Add storage plugin into running configuration
 * 
 * \param config configurator
 * \param plugin plugin configuration
 * \param index plugin index in array
 * \return 0 on success
 */
int config_add_storage(configurator *config, struct plugin_config *plugin, int index)
{
	MSG_ERROR(msg_module, "[%d] Storage add %s", config->proc_id, plugin->conf.name);
	config->startup->storage[index] = plugin;
	return 0;
}

/**
 * \brief Compare two plugin configurations
 * 
 * \param first first config
 * \param second second config
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
	
	/* Get first subtree */
	xmlBufPtr fbuf = calloc(1, 20000);
	CHECK_ALLOC(fbuf, 1);
	xmlBufGetNodeContent(fbuf, fnode);
	const char *fcontent = (const char *) xmlBufContent(fbuf);
	
	/* Get second subtree */
	xmlBufPtr sbuf = calloc(1, 20000);
	CHECK_ALLOC(sbuf, 1);
	xmlBufGetNodeContent(sbuf, snode);
	const char *scontent = (const char *) xmlBufContent(sbuf);
	
	/* Compare contents of subtrees */
	int ret = strcmp(fcontent, scontent);
	
	free(fbuf);
	free(sbuf);
	
	return ret;
}

/**
 * \brief Remove plugin from running config
 * 
 * \param config configurator
 * \param index plugin index in array
 * \param type plugin type
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
 * \param config configurator
 * \param plugin plugin configuration
 * \param index plugin index in array
 * \param type plugin type
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
 * \param config configurator
 * \param old_plugins array of actually used plugins
 * \param new_plugins array of new configurations
 * \param type plugins type
 * \return 0 on success
 */
int config_process_changes(configurator *config, struct plugin_config *old_plugins[], struct plugin_config *new_plugins[], int type)
{
	int plugs = 0, found, i, j;
	
	/* Get number of plugins in new configuration */
	while (new_plugins[plugs]) plugs++;
	
	/* Check plugins */
	for (i = 0; old_plugins[i]; ++i) {
		found = 0;
		
		for (j = 0; j < plugs; ++j) {
			/* Find plugins with same names */
			if (!strcmp(old_plugins[i]->conf.name, new_plugins[j]->conf.name)) {
				/* Compare configurations */
				if (config_compare_xml(&(old_plugins[i]->conf), &(new_plugins[j]->conf)) == 0) {
					/* Same configurations - nothing changed, only position (?) */
					/* move */
				} else {
					/* Different configurations - remove old, add new plugin */
					config_remove(config, i, type);
					config_add(config, new_plugins[j], j, type);
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
	
	return 0;
}

/**
 * \brief Free list of plugin configurations
 * 
 * \param list
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

/**
 * \brief Create startup configuration
 * 
 * \param config configurator
 */
startup_config *config_create_startup(configurator *config)
{
	/* Allocate memory */
	startup_config *startup = calloc(1, sizeof(startup_config));
	CHECK_ALLOC(startup, NULL);
	
	struct plugin_xml_conf_list *aux_list, *aux_conf;
	int i = 0;
	
	/* Get input plugins */
	aux_list = get_input_plugins(config->collector_node, (char *) config->internal_file);
	if (!aux_list) {
		free(startup);
		return NULL;
	}
	
	/* Store them into array */
	for (aux_conf = aux_list; aux_conf; aux_conf = aux_conf->next, ++i) {
		startup->input[i] = calloc(1, sizeof(struct plugin_config));
		CHECK_ALLOC(startup->input[i], NULL);
		startup->input[i]->conf = aux_conf->config;
	}
	startup->input[i] = NULL;
	free_conf_list(aux_list);
	
	/* Get storage plugins */
	aux_list = get_storage_plugins(config->collector_node, config->act_doc, (char *) config->internal_file);
	if (!aux_list) {
		free(startup);
		return NULL;
	}
	
	/* Store them into array */
	i = 0;
	for (aux_conf = aux_list; aux_conf; aux_conf = aux_conf->next, ++i) {
		startup->storage[i] = calloc(1, sizeof(struct plugin_config));
		CHECK_ALLOC(startup->storage[i], NULL);
		startup->storage[i]->conf = aux_conf->config;
	}
	startup->storage[i] = NULL;
	free_conf_list(aux_list);
	
	/* Get (optional) intermediate plugins */
	aux_list = get_intermediate_plugins(config->act_doc, (char *) config->internal_file);
	
	/* Store them into array */
	i = 0;
	for (aux_conf = aux_list; aux_conf; aux_conf = aux_conf->next, ++i) {
		startup->inter[i] = calloc(1, sizeof(struct plugin_config));
		CHECK_ALLOC(startup->inter[i], NULL);
		startup->inter[i]->conf = aux_conf->config;
	}
	startup->inter[i] = NULL;
	free_conf_list(aux_list);
	
	return startup;
}

/**
 * \brief Apply new startup configuration
 * 
 * \param config configurator
 * \param new_startup new startup configuration
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
			config_add(config, new_startup->input[i], i, PLUGIN_INPUT);
		}
		
		/* Add all intermediate plugins */
		for (i = 0; new_startup->inter[i]; ++i) {
			config_add(config, new_startup->inter[i], i, PLUGIN_INTER);
		}
		
		/* Add all storage plugins */
		for (i = 0; new_startup->storage[i]; ++i) {
			config_add(config, new_startup->storage[i], i, PLUGIN_STORAGE);
		}
		
		return 0;
	}
	
	/* Process changes in all plugins */
	config_process_changes(config, config->startup->input,   new_startup->input,   PLUGIN_INPUT);
	config_process_changes(config, config->startup->inter,   new_startup->inter,   PLUGIN_INTER);
	config_process_changes(config, config->startup->storage, new_startup->storage, PLUGIN_STORAGE);
	
	return 0;
}

/**
 * \brief Reload IPFIXcol startup configuration
 */
int config_reconf(configurator *config)
{
	/* Create startup configuration from updated xml file */
	startup_config *new_startup = config_create_startup(config);
	if (!new_startup) {
		return 1;
	}
	
	/* Process changes */
	int ret = config_process_new_startup(config, new_startup);
	
	/* Free resources */
	free(new_startup);
	
	return ret;
}

/**
 * \brief Close plugin and free its resources
 * 
 * \param plugin
 */
void config_free_plugin(struct plugin_config *plugin)
{
	/* Close plugin */
	switch (plugin->type) {
	case PLUGIN_INPUT:
		if (plugin->input->dll_handler) {
			if (plugin->input->config) {
				plugin->input->close(&(plugin->input->config));
			}
			
			dlclose(plugin->input->dll_handler);
		}
		break;
	case PLUGIN_INTER:
		break;
	case PLUGIN_STORAGE:
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
 * \brief Destroy configurator structure
 */
void config_destroy(configurator *config)
{
	/* Close startup xml */
	if (config->act_doc) {
		xmlFreeDoc(config->act_doc);
		config->act_doc = NULL;
	}
	
	int i = 0;
	if (config->startup) {
		/* Close and free input plugins */
		for (i = 0; config->startup->input[i]; ++i) {
			config_free_plugin(config->startup->input[i]);
		}
		
		/* Close and freeintermediate plugins */
		for (i = 0; config->startup->inter[i]; ++i) {
			config_free_plugin(config->startup->inter[i]);
		}
		
		/* Close and free storage plugins */
		for (i = 0; config->startup->storage[i]; ++i) {
			config_free_plugin(config->startup->storage[i]);
		}
		
		/* Free startup configuration */
		free(config->startup);
	}
	
	/* Free configurator */
	free(config);
}