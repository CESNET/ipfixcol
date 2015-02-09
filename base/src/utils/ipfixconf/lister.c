/**
 * \file lister.c
 * \author Michal Kozubik <kozubik@cesnet.cz>
 * \brief Tool for editing IPFIXcol internalcfg.xml
 *
 * Copyright (C) 2014 CESNET, z.s.p.o.
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

#include "lister.h"
#include "ipfixconf.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <libxml/xpathInternals.h>

#define COLS 4

static int COL_WIDTH[] = {
	20, 20, 16, 20
};

static char *COL_HEADER[] = {
	"Plugin type", "Name/Format", "Process/Thread", "File"
};

/**
 * \brief Center string on given width
 * 
 * \param s Text
 * \param width Column width
 */
void centrePrint(char *s, int width)
{
    int len = strlen(s);
    if (len >= width) {
        printf("%s", s);
	} else {
        int remaining = width - len;
        int spacesRight = remaining / 2;
        int spacesLeft  = remaining - spacesRight;
        printf("%*s%s%*s", spacesLeft, "", s, spacesRight, "");
    }
}

/**
 * \brief List plugins
 * 
 * \param info tool configuration
 * \param tag Plugin class tag
 * \param type Plugin type (to print)
 * \param to_print Strings for printing
 * \param to_search String for searching
 */
void list(conf_info_t *info, char *tag, char *type, char **to_print, char **to_search)
{
	int i;
	xmlNodePtr children1 = NULL, children2 = NULL, children3 = NULL;
	xmlXPathObjectPtr xpath_obj_file = eval_xpath(info, tag);
	if (!xpath_obj_file) {
		return;
	}
	
	for (i = 0; i < xpath_obj_file->nodesetval->nodeNr; i++) {
		children1 = children2 = children3 = xpath_obj_file->nodesetval->nodeTab[i]->children;
		while (children1) {
			if (!strncmp ((char*) children1->name, to_search[0],strlen (to_search[0]) + 1)) {
				/* find the processName of specified inputPlugin in internalcfg.xml */
				while (children3) {
					if (!xmlStrncmp (children3->name, BAD_CAST to_search[1], strlen (to_search[1]) + 1)) {
						break;
					}
					children3 = children3->next;
				}
				/* find the file of specified inputPLugin in internalcfg.xml */
				while (children2) {
					if (!xmlStrncmp (children2->name, BAD_CAST to_search[2], strlen (to_search[2]) + 1)) {
						break;
					}
					children2 = children2->next;
				}
				
				/* Print informations */
				centrePrint(type, COL_WIDTH[0]);
				centrePrint((char *) children1->children->content, COL_WIDTH[1]);
				
				if (children3) {
					centrePrint((char *) children3->children->content, COL_WIDTH[2]);
				}
				if (children2) {
					printf("    %s\n", (char *) children2->children->content);
				}
			}
			children1 = children1->next;
		}
	}
	
	xmlXPathFreeObject(xpath_obj_file);
}

/**
 * \brief List input plugins
 * 
 * \param info tool configuration
 */
void list_input_plugins(conf_info_t *info)
{
	char *to_print[ITEMS_CNT]  = {"Name", "Process", "File"};
	char *to_search[ITEMS_CNT] = {"name", "processName", "file"}; 
	list(info, TAG_INPUT, "input", to_print, to_search);
}

/**
 * \brief List intermediate plugins
 * 
 * \param info tool configuration
 */
void list_intermediate_plugins(conf_info_t *info)
{
	char *to_print[ITEMS_CNT]  = {"Name", "Thread", "File"};
	char *to_search[ITEMS_CNT] = {"name", "threadName", "file"}; 
	list(info, TAG_INTER, "intermediate", to_print, to_search);
}

/**
 * \brief List output plugins
 * 
 * \param info tool configuration
 */
void list_output_plugins(conf_info_t *info)
{
	char *to_print[ITEMS_CNT]  = {"Format", "Thread", "File"};
	char *to_search[ITEMS_CNT] = {"fileFormat", "threadName", "file"}; 
	list(info, TAG_OUTPUT, "storage", to_print, to_search);
}

/**
 * \brief Print line with given length
 * 
 * \param len line length
 */
static inline void print_line(int len)
{
	int i;
	printf(" ");
	for (i = 0; i < len; ++i) {
		printf("-");
	}
	printf("\n");
}

/**
 * \brief List plugins
 * 
 * \param info tool configuration
 * \return 0 on success
 */
int list_plugins(conf_info_t *info)
{
	int i, len = 0;
	
	/* Print column headers */
	printf("\n");
	for (i = 0; i < COLS; i++) {
		centrePrint(COL_HEADER[i], COL_WIDTH[i]);
		len += COL_WIDTH[i];
	}
	printf("\n");
	
	print_line(len);
	
	/* Print plugins */
	switch (info->type) {
	case PL_INPUT:
		list_input_plugins(info);
		break;
	case PL_INTERMEDIATE:
		list_intermediate_plugins(info);
		break;
	case PL_OUTPUT:
		list_output_plugins(info);
		break;
	default:
		list_input_plugins(info);
		print_line(len);
		list_intermediate_plugins(info);
		print_line(len);
		list_output_plugins(info);
		break;
	}
	
	printf("\n");
	return 0;
}
