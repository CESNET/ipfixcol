/**
 * \file ipfixconf.c
 * \author Michal Kozubik <kozubik@cesnet.cz>
 * \brief Tool for editing IPFIXcol internalcfg.xml
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

#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <libxml/xpathInternals.h>

#include "ipfixconf.h"
#include "lister.h"
#include "adder.h"
#include "remover.h"

#define DEFAULT_INTERNAL "/etc/ipfixcol/internalcfg.xml"

/** Acceptable command-line parameters (normal) */
#define OPTSTRING "hfc:p:n:s:t:"

/** Acceptable command-line parameters (long) */
struct option long_opts[] = {
	{ "help", no_argument, NULL, 'h' },
	{ 0, 0, 0, 0 }
};

#define CMD_ADD_STR    "add"
#define CMD_REMOVE_STR "remove"
#define CMD_LIST_STR   "list"

enum command_type {
	CMD_NONE,
	CMD_ADD,
	CMD_REMOVE,
	CMD_LIST
};

void usage(char *binary)
{
	printf("\n");
	printf("Tool for editing IPFIXcol internal configuration\n");
	printf("\n");
	printf("Usage: %s command [options]\n\n", binary);
	printf("  -h               show this text\n");
	printf("  -c path          configuration file, default %s\n", DEFAULT_INTERNAL);
	printf("  -p type          plugin type: i (input), m (intermediate), o (output)\n");
	printf("  -n name          plugin name\n");
	printf("  -s path          path to plugin .so file\n");
	printf("  -t thread_name   plugin thread name\n");
	printf("  -f               force add (rewrite plugin in case it already exists)\n");
	printf("\n");
	printf("Available commands:\n");
	printf("  add              add new plugin to configuration; all parameters are required\n");
	printf("  remove           remove plugin from configuration; plugin type and name are required\n");
	printf("  list             list configured plugins; type (-p) can be set\n");
	printf("\n");
}

/**
 * \brief Evaluate xpath
 */
xmlXPathObjectPtr eval_xpath(conf_info_t *info, char *tag)
{
	xmlXPathObjectPtr xpath_obj_file = NULL;
	
	char *path;
	if (asprintf(&path, "/cesnet-ipfixcol-int:ipfixcol/cesnet-ipfixcol-int:%s", tag) == -1) {
		fprintf(stderr, "Unable to allocate memory for asprintf!\n");
		return NULL;
	}
	
	xpath_obj_file = xmlXPathEvalExpression (BAD_CAST path, info->ctxt);
	free(path);
	
	if (xpath_obj_file != NULL) {
		if (xmlXPathNodeSetIsEmpty (xpath_obj_file->nodesetval)) {
			printf("No %s definition found in internal configuration.\n", tag);
			return NULL;
		}
	}
	
	return xpath_obj_file;
}

/**
 * \brief Check element existence
 */
xmlNodePtr get_node(xmlXPathObjectPtr xpath_obj_file, char *nameval, char *nametag)
{
	int i;
	xmlNodePtr children1;
	
	for (i = 0; i < xpath_obj_file->nodesetval->nodeNr; i++) {
		children1 = xpath_obj_file->nodesetval->nodeTab[i]->children;
		while (children1) {
			if ((!strncmp ((char*) children1->name, nametag, strlen (nametag) + 1))
					&& (!xmlStrncmp (children1->children->content, (xmlChar *) nameval, xmlStrlen ((xmlChar *)nameval) + 1))) {
				/* element found*/
				return xpath_obj_file->nodesetval->nodeTab[i];
			}
			children1 = children1->next;
		}
	}
	return NULL;
}

/**
 * \brief Get plugin node by its type and name
 */
xmlNodePtr get_plugin(conf_info_t *info, char *tag, char *nametag, char *nameval)
{
	xmlChar *xpath = NULL;
	xmlXPathObjectPtr result;
	xmlNodePtr plug = NULL;
	
	/* Check if plugin already exists */
	if (asprintf((char **) &xpath, "/cesnet-ipfixcol-int:ipfixcol/cesnet-ipfixcol-int:%s", tag) == -1) {
		fprintf(stderr, "Unable to allocate memory for asprintf!\n");
		return NULL;
	}
	
	result = xmlXPathEvalExpression(xpath, info->ctxt);
	free(xpath);
	
	if (!result) {
		fprintf(stderr, "xmlXPathEvalExpression failed\n");
		return NULL;
	}
	
	if (!xmlXPathNodeSetIsEmpty(result->nodesetval)) {
		plug = get_node(result, nameval, nametag);
	}
	xmlXPathFreeObject(result);
	
	return plug;
}

/**
 * \brief Get document root element
 */
xmlNodePtr get_root(conf_info_t *info)
{
	xmlChar *xpath = NULL;
	xmlXPathObjectPtr result;
	xmlNodePtr root;
	
	if (asprintf((char **) &xpath, "/cesnet-ipfixcol-int:ipfixcol") == -1) {
		fprintf(stderr, "Unable to allocate memory for asprintf!\n");
			return NULL;
	}
	
	result = xmlXPathEvalExpression(xpath, info->ctxt);
	free(xpath);
	
	if (!result) {
		fprintf(stderr, "xmlXPathEvalExpression failed\n");
		return NULL;
	}
	
	root = result->nodesetval->nodeTab[0];
	xmlXPathFreeObject(result);
	
	return root;
}

/**
 * \brief Open and parse xml file
 * 
 * \param info info structure
 * \param internal_cfg file path
 * \return 0 on success
 */
int open_xml(conf_info_t *info, char *internal_cfg)
{	
	info->doc = xmlParseFile(internal_cfg);

	/* create xpath evaluation context of internal configuration file */
	if ((info->ctxt = xmlXPathNewContext (info->doc)) == NULL) {
		fprintf(stderr, "Unable to create XPath context for internal configuration (%s:%d).", __FILE__, __LINE__);
		xmlFreeDoc(info->doc);
		return -1;
	}
	
	/* register namespace for the context of internal configuration file */
	if (xmlXPathRegisterNs(info->ctxt, BAD_CAST "cesnet-ipfixcol-int", BAD_CAST "urn:cesnet:params:xml:ns:yang:ipfixcol-internals") != 0) {
		fprintf(stderr, "Unable to register namespace for internal configuration file (%s:%d).", __FILE__, __LINE__);
		xmlXPathFreeContext(info->ctxt);
		xmlFreeDoc(info->doc);
		return -1;
	}
	
	return 0;
}

/**
 * \brief Save xml file
 * 
 * \param info tool configuration
 * \param path file path
 * \return 0 on success
 */
int save_xml(conf_info_t *info, char *path)
{
	xmlSaveFormatFile(path, info->doc, 1);
	return 0;
}

/**
 * \brief Close xml file
 * 
 * \param info tool informations
 * \return 0 on success
 */
int close_xml(conf_info_t *info)
{
	xmlXPathFreeContext(info->ctxt);
	xmlFreeDoc(info->doc);
	return 0;
}

/**
 * \brief Decode command string
 * 
 * \param cmd command
 * \return numeric value
 */
int command_decode(char *cmd)
{
	int ret;

	if (!strncmp(cmd, CMD_ADD_STR, sizeof(CMD_ADD_STR))) {
		ret = CMD_ADD;
	} else if (!strncmp(cmd, CMD_REMOVE_STR, sizeof(CMD_REMOVE_STR))) {
		ret = CMD_REMOVE;
	} else if (!strncmp(cmd, CMD_LIST_STR, sizeof(CMD_LIST_STR))) {
		ret = CMD_LIST;
	} else {
		ret = CMD_NONE;
	}

	return ret;
}

int main(int argc, char *argv[])
{
	int c, cmd, ret;
	conf_info_t info = {0};
	char *config = DEFAULT_INTERNAL;
	xmlKeepBlanksDefault(0);
	xmlTreeIndentString = "\t";
	
	if (argc < 2) {
		usage(argv[0]);
		return 1;
	}
	
	cmd = command_decode(argv[1]);

	/* parse params */
	while ((c = getopt_long(argc, argv, OPTSTRING, long_opts, NULL)) != -1) {
		switch (c) {
		case 'h':
			usage(argv[0]);
			return 0;
		case 'c':
			config = optarg;
			break;
		case 'p':
			switch (optarg[0]) {
			case 'i': case 'I':
				info.type = PL_INPUT;
				break;
			case 'm': case 'M':
				info.type = PL_INTERMEDIATE;
				break;
			case 'o': case 'O':
				info.type = PL_OUTPUT;
				break;
			default:
				fprintf(stderr, "Unknown plugin type '%c'\n", optarg[0]);
				return 1;
			}
			break;
		case 'n':
			info.name = optarg;
			break;
		case 's':
			info.sofile = optarg;
			break;
		case 't':
			info.thread = optarg;
			break;
		case 'f':
			info.force = 1;
			break;
		default:
			return 1;
		}
	}
	
	ret = open_xml(&info, config);
	if (ret != 0) {
		return 1;
	}
	
	switch (cmd) {
		case CMD_ADD:
			ret = add_plugin(&info);
			break;
		case CMD_REMOVE:
			ret = remove_plugin(&info);
			break;
		case CMD_LIST:
			ret = list_plugins(&info);
			break;
		default:
			fprintf(stderr, "Unknown command '%s'\n", argv[1]);
			ret = 1;
			break;
	}
	
	if (ret == 0 && cmd != CMD_LIST) {
		save_xml(&info, config);
	}
	
	close_xml(&info);
	
	return ret;
}