/**
 * \file profiler.cpp
 * \author Michal Kozubik <kozubik@cesnet.cz>
 * \brief intermediate plugin for profiling data
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

extern "C" {
#include <ipfixcol.h>
#include <ipfixcol/profiles.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <siso.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>

/* API version constant */
IPFIXCOL_API_VERSION;
}

#include <stdexcept>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <thread>

#define LEN_BYTES 4
#define BUFFER_SIZE 2048

static const char *suffixes[] {
	"B", "KB", "MB", "GB", nullptr
};

/* Identifier for verbose macros */
static const char *msg_module = "profiler";

struct daemon_config {
	fd_set read_flags;
	bool done;
	const char *ip{};
	const char *port{};
	std::string xml_path;
	sisoconf *receiver{};
	char buffer[BUFFER_SIZE];
	std::string message;
	int sockfd;
};

/**
 * \struct plugin_conf
 * 
 * Plugin configuration with list of organizations
 */
struct plugin_conf {
	void *ip_config;	/**< intermediate process config */
	std::thread thread;
	daemon_config *daemon{};
};

std::string formatSize(double size)
{
	int i = 0;

	while (size > 1024.0 && suffixes[i + 1] != nullptr) {
		size /= 1024.0;
		++i;
	}

	std::ostringstream oss;
	oss << std::setprecision(2) << size << " " << suffixes[i];
	return oss.str();
}

void reconfigure()
{
	MSG_DEBUG(msg_module, "Reconfiguring process %d", getpid());
	kill(getpid(), SIGUSR1);
}

void save_config(daemon_config *daemon, bool reconf = true)
{
	if (!daemon->message.empty()) {
		const char *path = profiles_get_xml_path();

		MSG_DEBUG(msg_module, "Saving configuration to %s", path);

		std::ofstream file(path);
		file << daemon->message;

		if (reconf) {
			reconfigure();
		}
	}
}

void receive_config(daemon_config *daemon)
{
	MSG_DEBUG(msg_module, "Waiting for data from daemon");
	daemon->message.clear();
	uint32_t remaining;

	int readed = read(daemon->sockfd, &remaining, LEN_BYTES);
	if (readed != LEN_BYTES) {
		if (errno != EINTR) {
			MSG_ERROR(msg_module, "Cannot read data from daemon: %s", strerror(errno));
		}
		return;
	}

	remaining = ntohl(remaining);

	MSG_DEBUG(msg_module, "Reading %s", formatSize(remaining).c_str());

	while (remaining > 0) {
		readed = read(daemon->sockfd, daemon->buffer, std::min(remaining, (uint32_t) BUFFER_SIZE));
		if (readed < 1) {
			MSG_ERROR(msg_module, "Error while reading message: %s", strerror(errno));
			daemon->message.clear();
			return;
		}

		daemon->buffer[readed] = '\0';
		daemon->message += daemon->buffer;
		remaining -= readed;
	}
}

void daemon_loop(daemon_config *config)
{
	while (!config->done) {
		receive_config(config);
		save_config(config);
	}

	siso_close_connection(config->receiver);
	siso_destroy(config->receiver);
}

void start_daemon(plugin_conf *config)
{
	MSG_DEBUG(msg_module, "Creating siso connection");
	if (siso_create_connection(config->daemon->receiver, config->daemon->ip, config->daemon->port, "TCP") != SISO_OK) {
		throw std::runtime_error(siso_get_last_err(config->daemon->receiver));
	}

	config->daemon->sockfd = siso_get_socket(config->daemon->receiver);

	receive_config(config->daemon);
	save_config(config->daemon, false);

	config->daemon->done = false;
	config->thread = std::move(std::thread(daemon_loop, config->daemon));
}

void process_xml_config(struct plugin_conf *config, char *xml_config)
{
	xmlDoc *doc = xmlParseDoc(BAD_CAST xml_config);
	if (doc == nullptr) {
		throw std::invalid_argument("Unable to parse startup configuration!");
	}

	xmlNode *root = xmlDocGetRootElement(doc);
	if (root == nullptr) {
		xmlFreeDoc(doc);
		throw std::invalid_argument("Cannot get document root element!");
	}

	xmlNode *node{nullptr};
	for (node = root->children; node != nullptr; node = node->next) {
		if (node->type != XML_ELEMENT_NODE) {
			continue;
		}

		std::string name = (const char *) node->name;
		if (name != "configurator") {
			continue;
		}

		for (xmlNode *daemon = node->children; daemon != nullptr; daemon = daemon->next) {
			name = (const char *) daemon->name;
			xmlChar *aux_char = xmlNodeListGetString(doc, daemon->children, 1);

			if (config->daemon == nullptr) {
				config->daemon = new daemon_config;
			}

			if (name == "ip") {
				config->daemon->ip = (const char *) aux_char;
			} else if (name == "port") {
				config->daemon->port = (const char *) aux_char;
			}
		}
	}

	if (config->daemon != nullptr) {
		if (config->daemon->ip == nullptr) {
			throw std::invalid_argument("Missing configurator IP address");
		}

		if (config->daemon->port == nullptr) {
			throw std::invalid_argument("Missing configurator port number");
		}

		config->daemon->xml_path = profiles_get_xml_path();
		if (config->daemon->xml_path.empty()) {
			throw std::runtime_error("Missing path to the profiles.xml");
		}

		config->daemon->receiver = siso_create();
		if (config->daemon->receiver == nullptr) {
			throw std::runtime_error("Cannot create sisoconf structure");
		}
	}

	xmlFreeDoc(doc);
}

/**
 * \brief Plugin initialization
 */
int intermediate_init(char* params, void* ip_config, uint32_t ip_id, ipfix_template_mgr* template_mgr, void** config)
{	
	/* Suppress compiler warning */
	(void) ip_id; (void) template_mgr;
	
	if (!params) {
		MSG_ERROR(msg_module, "Missing plugin configuration");
		return 1;
	}
	
	try {
		/* Create configuration */
		plugin_conf *conf = new struct plugin_conf;
		process_xml_config(conf, params);

		if (conf->daemon != nullptr) {
			start_daemon(conf);
		}

		/* Save configuration */
		conf->ip_config = ip_config;
		*config = conf;
		
	} catch (std::exception &e) {
		*config = NULL;
		if (!std::string(e.what()).empty()) {
			MSG_ERROR(msg_module, "%s", e.what());
		}
		
		return 1;
	}
	
	MSG_DEBUG(msg_module, "initialized");
	return 0;
}

/**
 * \brief Process IPFIX message
 */
int intermediate_process_message(void* config, void* message)
{
	plugin_conf *conf = reinterpret_cast<plugin_conf *>(config);
	struct ipfix_message *msg = reinterpret_cast<struct ipfix_message *>(message);
	
	if (msg->data_records_count == 0) {
		pass_message(conf->ip_config, msg);
		return 0;
	}

	if (!msg->live_profile) {
		MSG_WARNING(msg_module, "Missing profiles configuration");
		pass_message(conf->ip_config, msg);
		return 0;
	}

	/* Match channels for each data record */
	for (uint16_t i = 0; i < msg->data_records_count; ++i) {
		msg->metadata[i].channels = profile_match_data(msg->live_profile, msg, &(msg->metadata[i]));
	}

	pass_message(conf->ip_config, msg);
	return 0;
}

void handler(int signum)
{
	(void) signum;
}

/**
 * \brief Close intermediate plugin
 */
int intermediate_close(void *config)
{
	MSG_DEBUG(msg_module, "CLOSING");
	plugin_conf *conf = static_cast<plugin_conf*>(config);
	
	/* Destroy configuration */
	if (conf->daemon) {
		conf->daemon->done = true;

		/*
		 * Daemon thread is blocked on "read" waiting for data from configurator
		 *
		 * To stop him:
		 * 1) replace signal handler with empty one
		 * 2) send interrupt signal so "read" is released with errno = EINTR
		 * 3) restore old action handler
		 * 4) join thread
		 */

		struct sigaction newAction, oldAction;
		sigemptyset(&newAction.sa_mask);
		newAction.sa_handler = handler;
		newAction.sa_flags = 0;

		sigaction(SIGINT, &newAction, &oldAction);
		pthread_kill(conf->thread.native_handle(), SIGINT);
		sigaction(SIGINT, &oldAction, NULL);


		if (conf->thread.joinable()) {
			conf->thread.join();
		}

		free((void*) conf->daemon->ip);
		free((void*) conf->daemon->port);

		delete conf->daemon;
	}

	delete conf;

	return 0;
}
