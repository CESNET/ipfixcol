/**
 * \file dummy_ip.c
 * \author Michal Srb <michal.srb@cesnet.cz>
 * \brief Simple Intermediate Process which does literally nothing
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


/**
 * \defgroup dummyInter Dummy Intermediate Process
 * \ingroup intermediatePlugins
 *
 * This plugin does nothing. It't the example of the most basic intermediate plugin.
 *
 * @{
 */

#include <stdio.h>
#include <stdlib.h>

#include <ipfixcol.h>

/* API version constant */
IPFIXCOL_API_VERSION;

static char *msg_module = "dummy Intermediate Process";

/* plugin's configuration structure */
struct dummy_ip_config {
	char *params;
	void *ip_config;
	uint32_t ip_id;
	struct ipfix_template_mgr *tm;
};

int intermediate_init(char *params, void *ip_config, uint32_t ip_id, struct ipfix_template_mgr *template_mgr, void **config)
{
	struct dummy_ip_config *conf;

	conf = (struct dummy_ip_config *) malloc(sizeof(*conf));
	if (!conf) {
		MSG_ERROR(msg_module, "Unable to allocate memory (%s:%d)", __FILE__, __LINE__);
		return -1;
	}

	conf->params = params;
	conf->ip_config = ip_config;
	conf->ip_id = ip_id;
	conf->tm = template_mgr;

	*config = conf;

	MSG_INFO(msg_module, "Successfully initialized");

	/* plugin successfully initialized */
	return 0;
}


/**
 * \brief Do nothing, just pass the message to the output queue 
 */
int intermediate_process_message(void *config, void *message)
{
	struct dummy_ip_config * conf;
	conf = (struct dummy_ip_config *) config;

	MSG_DEBUG(msg_module, "got IPFIX message!");

	pass_message(conf->ip_config, message);

	return 0;
}


int intermediate_close(void *config)
{
	struct dummy_ip_config *conf;

	conf = (struct dummy_ip_config *) config;

	free(conf);

	return 0;
}

/**@}*/
