/**
 * \file tcp_input.c
 * \author Petr Velan <petr.velan@cesnet.cz>
 * \brief IPFIX Collector TCP Input Plugin
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
 * \defgroup tcpInput TCP input plugin for ipfixcol
 * \ingroup inputPLugins
 *
 * This is implementation of the input plugin API for TCP network input.
 * Input parameters are passed in xml format.
 *
 * @{
 */

#include <stdint.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <time.h>
#include <stdbool.h>

#include <ipfixcol.h>
#include "convert.h"

#ifdef TLS_SUPPORT
#	include <openssl/ssl.h>
#	include <openssl/err.h>

/* use these if not specified otherwise */
#	define DEFAULT_SERVER_CERT_FILE "/etc/ssl/certs/collector.crt"
#	define DEFAULT_SERVER_PKEY_FILE "/etc/ssl/private/collector.key"
#	define DEFAULT_CA_FILE          "/etc/ssl/private/ca.crt"
#	define DEFAULT_SIZE_SSL_LIST 100
#endif

/* API version constant */
IPFIXCOL_API_VERSION;

/* input buffer length */
#define BUFF_LEN 10000
/* default port for tcp collector */
#define DEFAULT_PORT "4739"
/* backlog for tcp connections */
#define BACKLOG SOMAXCONN
/* initial size of array of socket addresses */
#define ADDR_ARRAY_INITIAL_SIZE 50

/** Identifier to MSG_* macros */
static char *msg_module = "TCP input";

/**
 * \struct input_info_list
 * \brief  List structure for input info
 */
struct input_info_list {
	struct input_info_network info;
	struct input_info_list *next;
#ifdef TLS_SUPPORT
	char *collector_cert;
	X509 *exporter_cert;
#endif
};

/**
 * \struct plugin_conf
 * \brief  Plugin configuration structure passed by the collector
 */
struct plugin_conf {
	int socket; /**< listening socket */
	struct input_info_network info; /**< basic information structure */
	fd_set master; /**< set of all active sockets */
	int fd_max; /**< max file descriptor number */
	struct sockaddr_in6 **sock_addresses; /*< array of addresses indexed by sockets */
	uint16_t sock_addresses_max;
	struct input_info_list *info_list; /**< list of infromation structures
										* passed to collector */
#ifdef TLS_SUPPORT
	uint8_t tls;                  /**< TLS enabled? 0 = no, 1 = yes */
	SSL_CTX *ctx;                 /**< CTX structure */
	SSL **ssl_list;               /**< list of all SSL structures */
	uint16_t ssl_list_size;       /**< number of SSL structures in the list */
	char *ca_cert_file;           /**< CA certificate in PEM format */
	char *server_cert_file;       /**< server's certifikate in PEM format */
	char *server_pkey_file;       /**< server's private key */
#endif
};

#ifdef TLS_SUPPORT
/* auxiliary structure for error handling */
struct cleanup {
	struct sockaddr_in6 *address;
	SSL *ssl;
	X509 *peer_cert;
} maid;
#endif

pthread_mutex_t mutex;
pthread_t listen_thread;

/**
 * \brief Free address variable on input_listen exit
 *
 * \param[in] address Address to free on exit
 * \return void
 */
void input_listen_cleanup(void *address)
{
	if (address != NULL) {
		free(address);
	}
}

#ifdef TLS_SUPPORT
/**
 * \brief Free TLS related things when TLS connection fails from some reason
 *
 * \param[in] config  plugin configuration structure
 * \param[in] maid  structure containing all pointers to free
 * \return  nothing
 */
void input_listen_tls_cleanup(void *config, struct cleanup *maid)
{
	struct plugin_conf *conf;
	int fd;
	int ret;
	
	conf = (struct plugin_conf *) config;

	/* TLS enabled? */
	if (conf->tls) {
		if (maid->address != NULL) {
			free(maid->address);
		}

		if (maid->ssl != NULL) {
			fd = SSL_get_fd(maid->ssl);
			if (fd >=0) {
				/* TLS shutdown */
				ret = SSL_shutdown(maid->ssl);
				if (ret == -1) {
					MSG_WARNING(msg_module, "Error during TLS connection teardown");
				}
			}
			SSL_free(maid->ssl);
		}

		if (maid->peer_cert != NULL) {
			X509_free(maid->peer_cert);
		}
	}
}
#endif

/**
 * \brief Resizes socket addresses storage
 * \param conf plugin configuration
 * \return true on success
 */
bool enlarge_sock_addresses(struct plugin_conf *conf)
{
	uint16_t old_max = conf->sock_addresses_max;
	conf->sock_addresses_max += ADDR_ARRAY_INITIAL_SIZE;

	struct sockaddr_in6 **new_addresses = realloc(conf->sock_addresses, conf->sock_addresses_max * sizeof(void*));
	if (new_addresses == NULL) {
		MSG_ERROR(msg_module, "Cannot allocate memory: %s", strerror(errno));
		conf->sock_addresses_max = old_max;
		return false;
	}

	// Initializes newly allocated memory
	conf->sock_addresses = new_addresses;
	memset(&(conf->sock_addresses[old_max]), 0, ADDR_ARRAY_INITIAL_SIZE * sizeof(void*));

	return true;
}

/**
 * \brief Stores socket address and resizes array if needed
 * \param conf plugin configuration
 * \param address socket address
 * \param position socket address position (socket file descriptor)
 * \return true on success
 */
bool add_sock_address(struct plugin_conf *conf, struct sockaddr_in6 *address, uint16_t position)
{
	if (position >= conf->sock_addresses_max) {
		if (!enlarge_sock_addresses(conf)) {
			return false;
		}
	}

	conf->sock_addresses[position] = address;
	return true;
}

/**
 * \brief Removes socket address and frees memory
 * \param conf plugin configuration
 * \param position address position (socket file descriptor)
 */
void remove_sock_address(struct plugin_conf *conf, uint16_t position)
{
	if (conf->sock_addresses[position] != NULL) {
		free(conf->sock_addresses[position]);
		conf->sock_addresses[position] = NULL;
	}
}

/**
 * \brief Removes all addresses and destroys storage
 * \param conf plugin configuration
 */
void destroy_sock_addresses(struct plugin_conf *conf)
{
	if (conf->sock_addresses == NULL) {
		return;
	}

	for (int i = 0; i < conf->sock_addresses_max; ++i) {
		if (conf->sock_addresses[i] != NULL) {
			free(conf->sock_addresses[i]);
		}
	}

	free(conf->sock_addresses);
	conf->sock_addresses = NULL;
}

/**
 * \brief Funtion that listens for new connetions
 *
 * Runs in a thread and adds new connections to plugin_conf->master set
 *
 * \param[in, out] config Plugin configuration structure
 * \return NULL always
 */
void *input_listen(void *config)
{
	struct plugin_conf *conf = (struct plugin_conf *) config;
	int new_sock;
	/* use IPv6 sockaddr structure to store address information (IPv4 fits easily) */
	struct sockaddr_in6 *address = NULL;
	socklen_t addr_length;
	char src_addr[INET6_ADDRSTRLEN];
	struct input_info_list *input_info;
#ifdef TLS_SUPPORT
	int ret;
	int i;
	SSL *ssl = NULL;           /* structure for TLS connection */
	X509 *peer_cert = NULL;    /* peer's certificate */
	struct cleanup maid;       /* auxiliary struct for TLS error handling */
#endif

	/* loop ends when thread is cancelled by pthread_cancel() function */
	while (1) {
		/* allocate space for the address */
		addr_length = sizeof(struct sockaddr_in6);
		address = malloc(addr_length);
		if (!address) {
			MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)", __FILE__, __LINE__);
			break;
		}

		/* ensure that address will be freed when thread is canceled */ 
		pthread_cleanup_push(input_listen_cleanup, (void *) address);

		if ((new_sock = accept(conf->socket, (struct sockaddr*) address, &addr_length)) == -1) {
			MSG_ERROR(msg_module, "Cannot accept new socket: %s", strerror(errno));
			/* exit and call cleanup */
			pthread_exit(0);
		}
#ifdef TLS_SUPPORT
		/* preparation for TLS error handling */
		maid.address = address;
		maid.ssl = NULL;
		maid.peer_cert = NULL;

		if (conf->tls) {
			/* create a new SSL structure for the connection */
			ssl = SSL_new(conf->ctx);
			if (!ssl) {
				MSG_ERROR(msg_module, "Unable to create SSL structure");
				ERR_print_errors_fp(stderr);
				/* cleanup */
				input_listen_tls_cleanup(conf, &maid);
				continue;
			}
			maid.ssl = ssl;

			/* connect the SSL object with the socket */
			ret = SSL_set_fd(ssl, new_sock);
			if (ret != 1) {
				MSG_ERROR(msg_module, "Unable to connect the SSL object with the socket");
				ERR_print_errors_fp(stderr);
				/* cleanup */
				input_listen_tls_cleanup(conf, &maid);
				continue;
			}

			/* TLS handshake */
			ret = SSL_accept(ssl);
			if (ret != 1) {
				/* handshake wasn't successful */
				MSG_ERROR(msg_module, "TLS handshake was not successful");
				ERR_print_errors_fp(stderr);
				/* cleanup */
				input_listen_tls_cleanup(conf, &maid);
				continue;
			}

			/* obtain peer's certificate */
			peer_cert = SSL_get_peer_certificate(ssl);
			if (!peer_cert) {
				MSG_ERROR(msg_module, "No certificate was presented by the peer");
				/* cleanup */
				input_listen_tls_cleanup(conf, &maid);
				continue;
			}
			maid.peer_cert = peer_cert;

			/* verify peer's certificate */
			if (SSL_get_verify_result(ssl) != X509_V_OK) {
				MSG_ERROR(msg_module, "Client sent bad certificate; verification failed");
				/* cleanup */
				input_listen_tls_cleanup(conf, &maid);
				continue;
			}

			/* put new SSL structure into the conf->ssl_list */
			for (i = 0; i < conf->ssl_list_size; i++) {
				if (conf->ssl_list[i] == NULL) {
					conf->ssl_list[i] = ssl;
				}
			}

			if (conf->ssl_list[i] != ssl) {
				/* limit reached. no space for new SSL structure */
				MSG_WARNING(msg_module, "Limit on the number of TLS connections reached; tearing down this connection...");
				/* cleanup */
				input_listen_tls_cleanup(conf, &maid);
				continue;
			}
		}
#endif
		pthread_mutex_lock(&mutex);
		FD_SET(new_sock, &conf->master);

		if (conf->fd_max < new_sock) {
			conf->fd_max = new_sock;
		}

		/* copy socket address to config structure */
		if (!add_sock_address(conf, address, new_sock)) {
			MSG_ERROR(msg_module, "Cannot store another socket address!");
			break;
		}

		/* print info */
		if (conf->info.l3_proto == 4) {
			inet_ntop(AF_INET, (void *)&((struct sockaddr_in*) address)->sin_addr, src_addr, INET6_ADDRSTRLEN);
		} else {
			inet_ntop(AF_INET6, &address->sin6_addr, src_addr, INET6_ADDRSTRLEN);
		}
		MSG_NOTICE(msg_module, "Exporter connected from address %s", src_addr);

		pthread_mutex_unlock(&mutex);

		/* create new input_info for this connection */
		input_info = calloc(1, sizeof(struct input_info_list));
		if (input_info == NULL) {
			MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)", __FILE__, __LINE__);
			break;
		}

		memcpy(&input_info->info, &conf->info, sizeof(struct input_info_list));

		/* set status to new connection */
		input_info->info.status = SOURCE_STATUS_NEW;

		/* copy address and port */
		if (address->sin6_family == AF_INET) {
			/* copy src IPv4 address */
			input_info->info.src_addr.ipv4.s_addr =
					((struct sockaddr_in*) address)->sin_addr.s_addr;

			/* copy port */
			input_info->info.src_port = ntohs(((struct sockaddr_in*)  address)->sin_port);
		} else {
			/* copy src IPv6 address */
			int i;
			for (i=0; i<4; i++) {
				input_info->info.src_addr.ipv6.s6_addr32[i] = address->sin6_addr.s6_addr32[i];
			}

			/* copy port */
			input_info->info.src_port = ntohs(address->sin6_port);
		}

#ifdef TLS_SUPPORT
		/* fill in certificates */
		input_info->collector_cert = conf->server_cert_file;
		input_info->exporter_cert = peer_cert;
#endif

		/* add to list */
		input_info->next = conf->info_list;
		conf->info_list = input_info;

		/* unset the address so that we do not free it incidentally */
		address = NULL;

		pthread_cleanup_pop(0);
	}
	return NULL;
}

/**
 * \brief Input plugin initializtion function
 *
 * \param[in]  params  XML with input parameters
 * \param[out] config  Sets source and destination IP, destination port.
 * \return 0 on success, nonzero else.
 */
int input_init(char *params, void **config)
{
	/* necessary structures */
	struct addrinfo *addrinfo = NULL, hints;
	struct plugin_conf *conf = NULL;
	char *port = NULL, *address = NULL;
	int ai_family = AF_INET6; /* IPv6 is default */
	char dst_addr[INET6_ADDRSTRLEN];
	int ret, ipv6_only = 0, retval = 0, yes = 1; /* yes is for setsockopt */
	/* 1 when using default port - don't free memory */
	int def_port = 0;
#ifdef TLS_SUPPORT
	SSL_CTX *ctx = NULL;       /* SSL context structure */
	SSL **ssl_list = NULL;     /* list of SSL connection structures */
	xmlNode *cur_node_parent;
#endif

	/* parse params */
	xmlDoc *doc = NULL;
	xmlNode *root_element = NULL;
	xmlNode *cur_node = NULL;

	/* allocate plugin_conf structure */
	conf = calloc(1, sizeof(struct plugin_conf));
	if (conf == NULL) {
		MSG_ERROR(msg_module, "Cannot allocate memory for config structure: %s", strerror(errno));
		retval = 1;
		goto out;
	}

	if (!enlarge_sock_addresses(conf)) {
		MSG_ERROR(msg_module, "Cannot initialize array for socket address");
		retval = 1;
		goto out;
	}

	/* empty the master set */
	FD_ZERO(&conf->master);

	/* parse xml string */
	doc = xmlParseDoc(BAD_CAST params);
	if (doc == NULL) {
		MSG_ERROR(msg_module, "Cannot parse configuration file");
		retval = 1;
		goto out;
	}
	/* get the root element node */
	root_element = xmlDocGetRootElement(doc);
	if (root_element == NULL) {
		MSG_ERROR(msg_module, "Cannot get document root element");
		retval = 1;
		goto out;
	}

	/* check that we have the right config xml, BAD_CAST is (xmlChar *) cast defined by libxml */
	if (!xmlStrEqual(root_element->name, BAD_CAST "tcpCollector")) {
		MSG_ERROR(msg_module, "Expecting tcpCollector root element; got %s", root_element->name);
		retval = 1;
		goto out;
	}

	/* go over all elements */
	for (cur_node = root_element->children; cur_node; cur_node = cur_node->next) {
#ifdef TLS_SUPPORT
		/* check whether we want to enable transport layer security */
		if (xmlStrEqual(cur_node->name, BAD_CAST "transportLayerSecurity")) {
			MSG_NOTICE(msg_module, "TLS enabled");
			conf->tls = 1;   /* TLS enabled */
			cur_node_parent = cur_node;	

			/* TLS configuration options */
			for (cur_node = cur_node->xmlChildrenNode; cur_node; cur_node = cur_node->next) {
			if (cur_node->type == XML_ELEMENT_NODE
						&& cur_node->children != NULL
						&& cur_node->children->content != NULL) {
					int tmp_val_len = strlen((char *) cur_node->children->content) + 1;
					char *tmp_val = malloc(sizeof(char) * tmp_val_len);
					if (!tmp_val) {
						MSG_ERROR(msg_module, "Cannot allocate memory: %s", strerror(errno));
						retval = 1;
						goto out;
					}
					strncpy_safe(tmp_val, (char *)cur_node->children->content, tmp_val_len);
					
					if (xmlStrEqual(cur_node->name, BAD_CAST "localCAfile")) {
						/* location of the CA certificate */
						conf->ca_cert_file = tmp_val;
					} else if (xmlStrEqual(cur_node->name, BAD_CAST "localServerCert")) {
						/* server's certificate */
						conf->server_cert_file = tmp_val;
					} else if (xmlStrEqual(cur_node->name, BAD_CAST "localServerCertKey")) {
						/* server's private key */
						conf->server_pkey_file = tmp_val;
					} else {
						/* unknown option */
						MSG_WARNING(msg_module, "Unknown configuration option: %s", cur_node->name);
						free(tmp_val);
					}
				}
			}

			cur_node = cur_node_parent;

			continue;
		}
#else   /* user wants TLS, but collector was compiled without it */
		if (xmlStrEqual(cur_node->name, BAD_CAST "transportLayerSecurity")) {
			/* user wants to enable TLS, but collector was compiled without it */
			MSG_WARNING(msg_module, "Collector was compiled without TLS support");
			continue;
		}
#endif
		if (cur_node->type == XML_ELEMENT_NODE && cur_node->children != NULL) {
			/* copy value to memory - don't forget the terminating zero */
			int tmp_val_len = strlen((char *) cur_node->children->content) + 1;
			char *tmp_val = malloc(sizeof(char) * tmp_val_len);
			/* this is not a preferred cast, but we really want to use plain chars here */
			if (tmp_val == NULL) {
				MSG_ERROR(msg_module, "Cannot allocate memory: %s", strerror(errno));
				retval = 1;
				goto out;
			}
			strncpy_safe(tmp_val, (char *)cur_node->children->content, tmp_val_len);

			if (xmlStrEqual(cur_node->name, BAD_CAST "localPort")) { /* set local port */
				port = tmp_val;
			} else if (xmlStrEqual(cur_node->name, BAD_CAST "localIPAddress")) { /* set local address */
				address = tmp_val;
			/* save following configuration to input_info */
			} else if (xmlStrEqual(cur_node->name, BAD_CAST "templateLifeTime")) {
				conf->info.template_life_time = tmp_val;
			} else if (xmlStrEqual(cur_node->name, BAD_CAST "optionsTemplateLifeTime")) {
				conf->info.options_template_life_time = tmp_val;
			} else if (xmlStrEqual(cur_node->name, BAD_CAST "templateLifePacket")) {
				conf->info.template_life_packet = tmp_val;
			} else if (xmlStrEqual(cur_node->name, BAD_CAST "optionsTemplateLifePacket")) {
				conf->info.options_template_life_packet = tmp_val;
			} else { /* unknown parameter, ignore */
				/* Free the tmp_val for unknown elements */
				free(tmp_val);
			}
		}
	}

	/* set default port if none given */
	if (port == NULL) {
		port = DEFAULT_PORT;
		def_port = 1;
	}

#ifdef TLS_SUPPORT
	if (conf->tls) {
		/* use default options if not specified in configuration file */
		if (conf->ca_cert_file == NULL) {
			conf->ca_cert_file = strdup(DEFAULT_CA_FILE);
		}

		if (conf->server_cert_file == NULL) {
			conf->server_cert_file = strdup(DEFAULT_SERVER_CERT_FILE);
		}

		if (conf->server_pkey_file == NULL) {
			conf->server_pkey_file = strdup(DEFAULT_SERVER_PKEY_FILE);
		}
	}
#endif

	/* specify parameters of the connection */
	memset (&hints, 0, sizeof(struct addrinfo));
	hints.ai_socktype = SOCK_STREAM; /* TCP */
	hints.ai_family = ai_family; /* select IPv4 or IPv6*/
	hints.ai_flags = AI_V4MAPPED; /* we want to accept mapped addresses */
	if (address == NULL) {
		hints.ai_flags |= AI_PASSIVE; /* no address given, listen on all local addresses */
	}

	/* get server address */
	if ((ret = getaddrinfo(address, port, &hints, &addrinfo)) != 0) {
		MSG_ERROR(msg_module, "getaddrinfo failed: %s", gai_strerror(ret));
		retval = 1;
		goto out;
	}

	/* create socket */
	conf->socket = socket(addrinfo->ai_family, addrinfo->ai_socktype, addrinfo->ai_protocol);
	/* Retry with IPv4 when the implementation does not support the specified address family */
	if (conf->socket == -1 && errno == EAFNOSUPPORT && addrinfo->ai_family == AF_INET6) {
		addrinfo->ai_family = AF_INET;
		conf->socket = socket(addrinfo->ai_family, addrinfo->ai_socktype, addrinfo->ai_protocol);
	}
	if (conf->socket == -1) {
		/* End with error otherwise */
		MSG_ERROR(msg_module, "Cannot create socket: %s", strerror(errno));
		retval = 1;
		goto out;
	}

	/* allow IPv4 connections on IPv6 */
	if ((addrinfo->ai_family == AF_INET6) &&
		(setsockopt(conf->socket, IPPROTO_IPV6, IPV6_V6ONLY, &ipv6_only, sizeof(ipv6_only)) == -1)) {
		MSG_WARNING(msg_module, "Cannot turn off socket option IPV6_V6ONLY; plugin may not accept IPv4 connections...");
	}

	/* allow to reuse the address immediately */
	if (setsockopt(conf->socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1)
	{
		MSG_WARNING(msg_module, "Cannot turn on socket reuse option; it may take a while before collector can be restarted");
	}

	/* bind socket to address */
	if (bind(conf->socket, addrinfo->ai_addr, addrinfo->ai_addrlen) != 0) {
		MSG_ERROR(msg_module, "Cannot bind socket: %s", strerror(errno));
		retval = 1;
		goto out;
	}

	/* this is a listening socket */
	if (listen(conf->socket, BACKLOG) == -1) {
		MSG_ERROR(msg_module, "Cannot listen on socket: %s", strerror(errno));
		retval = 1;
		goto out;
	}

#ifdef TLS_SUPPORT
	if (conf->tls) {
		/* configure TLS */
	
		/* initialize library */
		SSL_load_error_strings();
		SSL_library_init();
	
		/* create CTX structure for TLS */
		ctx = SSL_CTX_new(TLSv1_server_method());
		if (!ctx) {
			MSG_ERROR(msg_module, "Cannot create CTX structure");
			ERR_print_errors_fp(stderr);
			retval = 1;
			goto out;
		}
	
		/* load server certificate into the CTX structure */
		ret = SSL_CTX_use_certificate_file(ctx, conf->server_cert_file, SSL_FILETYPE_PEM);
		if (ret != 1) {
			MSG_ERROR(msg_module, "Unable to load server's certificate from %s", conf->server_cert_file);
			ERR_print_errors_fp(stderr);
			retval = 1;
			goto out;
		}
	
		/* load private keys into the CTX structure */
		SSL_CTX_use_PrivateKey_file(ctx, conf->server_pkey_file, SSL_FILETYPE_PEM);
		if (ret <= 0) {
			MSG_ERROR(msg_module, "Unable to load server's private key from %s", conf->server_pkey_file);
			ERR_print_errors_fp(stderr);
			retval = 1;
			goto out;
		}

		/* set peer certificate verification parameters */
		SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER | SSL_VERIFY_CLIENT_ONCE, NULL);
		
		ssl_list = (SSL **) malloc(sizeof(SSL *) * DEFAULT_SIZE_SSL_LIST);
		if (ssl_list == NULL) {
			MSG_ERROR(msg_module, "Not enough memory (%s:%d)", __FILE__, __LINE__);
			retval = 1;
			goto out;
		}
		memset(ssl_list, 0, DEFAULT_SIZE_SSL_LIST * sizeof(SSL *));

		conf->ssl_list_size = DEFAULT_SIZE_SSL_LIST;
		conf->ctx = ctx;
		conf->ssl_list = ssl_list;
	}
#endif  /* TLS */

	/* fill in general information */
	conf->info.type = SOURCE_TYPE_TCP;
	conf->info.dst_port = atoi(port);
	if (addrinfo->ai_family == AF_INET) { /* IPv4 */
		conf->info.l3_proto = 4;

		/* copy dst IPv4 address */
		conf->info.dst_addr.ipv4.s_addr =
			((struct sockaddr_in*) addrinfo->ai_addr)->sin_addr.s_addr;

		inet_ntop(AF_INET, &conf->info.dst_addr.ipv4, dst_addr, INET6_ADDRSTRLEN);
	} else { /* IPv6 */
		conf->info.l3_proto = 6;

		/* copy dst IPv6 address */
		int i;
		for (i=0; i<4;i++) {
			conf->info.dst_addr.ipv6.s6_addr32[i] =
				((struct sockaddr_in6*) addrinfo->ai_addr)->sin6_addr.s6_addr32[i];
		}

		inet_ntop(AF_INET6, &conf->info.dst_addr.ipv6, dst_addr, INET6_ADDRSTRLEN);
	}

	/* allocate memory for templates */
	if (convert_init(TCP_PLUGIN, BUFF_LEN) != 0) {
		MSG_ERROR(msg_module, "malloc() for templates failed!");
		retval = 1;
		goto out;
	}

	/* print info */
	MSG_NOTICE(msg_module, "Input plugin listening on %s, port %s", dst_addr, port);

	/* start listening thread */
	if (pthread_create(&listen_thread, NULL, &input_listen, (void *) conf) != 0) {
		MSG_ERROR(msg_module, "Failed to create listening thread");
		retval = 1;
		goto out;
	}

	/* pass general information to the collector */
	*config = (void*) conf;

	/* normal exit, all OK */
	MSG_NOTICE(msg_module, "Plugin initialization completed successfully");

out:
	if (def_port == 0 && port != NULL) { /* free when memory was actually allocated*/
		free(port);
	}

	if (address != NULL) {
		free(address);
	}

	if (addrinfo != NULL) {
		freeaddrinfo(addrinfo);
	}

	/* free the xml document */
	if (doc != NULL) {
		xmlFreeDoc(doc);
	}

	/* free the global variables that may have been allocated by the xml parser */
	xmlCleanupParser();

	/* free input_info when error occured */
	if (retval != 0 && conf != NULL) {
		if (conf->info.template_life_time != NULL) {
			free (conf->info.template_life_time);
		}
		if (conf->info.options_template_life_time != NULL) {
			free (conf->info.options_template_life_time);
		}
		if (conf->info.template_life_packet != NULL) {
			free (conf->info.template_life_packet);
		}
		if (conf->info.options_template_life_packet != NULL) {
			free (conf->info.options_template_life_packet);
		}
		free(conf);

	}

#ifdef TLS_SUPPORT
	/* error occurs, clean up */
	if ((retval != 0) && (conf != NULL)) {
		if (ssl_list) {
			free(ssl_list);
		}

		if (ctx) {
			SSL_CTX_free(ctx);
		}
	}
#endif

	return retval;
}

/**
 * \brief Pass input data from the input plugin into the ipfixcol core.
 *
 * IP addresses are passed as returned by recvfrom and getsockname,
 * ports are in host byte order
 *
 * \param[in] config  plugin_conf structure
 * \param[out] info   Information structure describing the source of the data.
 * \param[out] packet Flow information data in the form of IPFIX packet.
 * \param[out] source_status Status of source (new, opened, closed)
 * \return the length of packet on success, INPUT_CLOSE when some connection 
 *  closed, INPUT_ERROR on error or INPUT_SIGINT when interrupted.
 */
int get_packet(void *config, struct input_info **info, char **packet, int *source_status)
{
	/* temporary socket set */
	fd_set tmp_set;
	ssize_t length = 0, packet_length;
	struct timeval tv;
	char src_addr[INET6_ADDRSTRLEN];
	struct sockaddr_in6 *address;
	struct plugin_conf *conf = config;
	int retval = 0, sock;
	struct input_info_list *info_list;
#ifdef TLS_SUPPORT
	int i;
	SSL *ssl = NULL;        /* SSL structure for active socket */
#endif

	/* allocate memory for packet, if needed */
	if (*packet == NULL) {
		*packet = malloc(BUFF_LEN * sizeof(char));
		if (*packet == NULL) {
			MSG_ERROR(msg_module, "Cannot allocate memory for the packet, malloc failed: %s", strerror(errno));
		}
	}
   
	/* wait until some socket is ready */
	while (retval <= 0) {
		/* copy all sockets from master to tmp_set */
		FD_ZERO(&tmp_set);
		pthread_mutex_lock(&mutex);
		tmp_set = conf->master;
		pthread_mutex_unlock(&mutex);

		/* wait at most one second - give time to check for new sockets */
		tv.tv_sec = 1;
		tv.tv_usec = 0;

		/* select active connections */
		retval = select(conf->fd_max + 1, &tmp_set, NULL, NULL, &tv);
		if (retval == -1) {
			if (errno == EINTR) {
				return INPUT_INTR;
			}
			MSG_WARNING(msg_module, "Failed to select active connection: %s", strerror(errno));
			return INPUT_ERROR;
		}
	}

	/* find first socket that is ready */
	for (sock=0; sock <= conf->fd_max; sock++) {
		/* fetch first active connection */
		if (FD_ISSET(sock, &tmp_set)) {
			break;
		}
	}

	/* receive ipfix packet header */
#ifdef TLS_SUPPORT
	if (conf->tls) {
		/* TLS enabled */
		/* TODO - ugly piece of code... use epoll() instead of select()
		 * find corresponding SSL structure for socket */
		for (i = 0; ; i++) {
			if (conf->ssl_list[i] && (SSL_get_fd(conf->ssl_list[i]) == sock)) {
				ssl = conf->ssl_list[i];
				break;
			}
		}
		length = SSL_read(ssl, *packet, IPFIX_HEADER_LENGTH);
		if (length < 0) {
			/* read operation was not successful */
			if (SSL_get_error(ssl, length) == SSL_ERROR_SYSCALL) {
				if (errno == EINTR) {
					return INPUT_INTR;
				}
				MSG_ERROR(msg_module, "Failed to receive IPFIX packet header: %s", strerror(errno));
				return INPUT_ERROR;
			}
		}
	} else {
#endif
		/* receive without TLS */
		length = recv(sock, *packet, IPFIX_HEADER_LENGTH, MSG_WAITALL);
		if (length == -1) {
			if (errno == EINTR) {
				return INPUT_INTR;
			}
			MSG_ERROR(msg_module, "Failed to receive IPFIX packet header: %s", strerror(errno));
			return INPUT_ERROR;
		}
#ifdef TLS_SUPPORT
	}
#endif
	
	if (length >= IPFIX_HEADER_LENGTH) { /* header received */
		/* get packet total length */
		packet_length = ntohs(((struct ipfix_header *) *packet)->length);

		/* check whether buffer is big enough */
		if (packet_length > BUFF_LEN) {
			*packet = realloc(*packet, packet_length);
			if (*packet == NULL) {
				MSG_ERROR(msg_module, "Packet too big and realloc failed: %s", strerror(errno));
				return INPUT_ERROR;
			}
		}

		/* receive the rest of the ipfix packet */
		if (ntohs(((struct ipfix_header *) *packet)->version) == IPFIX_VERSION) {
			length = recv(sock, (*packet) + IPFIX_HEADER_LENGTH, packet_length - IPFIX_HEADER_LENGTH, MSG_WAITALL);
		} else {
			length = recv(sock, (*packet) + IPFIX_HEADER_LENGTH, BUFF_LEN - IPFIX_HEADER_LENGTH, 0);
		}
		if (length == -1) {
			if (errno == EINTR) {
				return INPUT_INTR;
			}
			MSG_WARNING(msg_module, "Failed to receive IPFIX packet: %s", strerror(errno));
			return INPUT_ERROR;

		} else if (length < packet_length - IPFIX_HEADER_LENGTH) {
			MSG_ERROR(msg_module, "Read IPFIX data is too short (%i): %s", length, strerror(errno));
		}

		length += IPFIX_HEADER_LENGTH;

		/* Convert packet from Netflow v5/v9/sflow to IPFIX format */
		if (htons(((struct ipfix_header *) (*packet))->version) != IPFIX_VERSION) {
			if (convert_packet(packet, &length, NULL) != 0) {
				MSG_WARNING(msg_module, "Message conversion error; skipping message...");
				return INPUT_INTR;
			}
		}

		/* Check if lengths are the same */
		if (length < htons(((struct ipfix_header *)*packet)->length)) {
			MSG_DEBUG(msg_module, "length = %d, header->length = %d", length, htons(((struct ipfix_header *)*packet)->length));
			return INPUT_INTR;
		} else if (length >  htons(((struct ipfix_header *)*packet)->length)) {
			length = htons(((struct ipfix_header*)*packet)->length);
		}
	} else if (length != 0) {
		MSG_ERROR(msg_module, "Packet header is incomplete; closing connection...");

		/* this will close the connection */
		length = 0;
	}

	/* get peer address from configuration */
	address = conf->sock_addresses[sock];

	/* go through input_info_list */
	for (info_list = conf->info_list; info_list != NULL; info_list = info_list->next) {
		/* ports must match */
		if (info_list->info.src_port == ntohs(((struct sockaddr_in*) address)->sin_port)) {
			/* compare addresses, dependent on IP protocol version*/
			if (info_list->info.l3_proto == 4) {
				if (info_list->info.src_addr.ipv4.s_addr == ((struct sockaddr_in*) address)->sin_addr.s_addr) {
					break;
				}
			} else {
				if (info_list->info.src_addr.ipv6.s6_addr32[0] == address->sin6_addr.s6_addr32[0] &&
						info_list->info.src_addr.ipv6.s6_addr32[1] == address->sin6_addr.s6_addr32[1] &&
						info_list->info.src_addr.ipv6.s6_addr32[2] == address->sin6_addr.s6_addr32[2] &&
						info_list->info.src_addr.ipv6.s6_addr32[3] == address->sin6_addr.s6_addr32[3]) {
					break;
				}
			}
		}
	}

	/* check whether we found the input_info */
	if (info_list == NULL) {
		MSG_WARNING(msg_module, "input_info not found, passing packet with NULL input info");
	} else {
		/* Set source status */
		*source_status = info_list->info.status;
		if (info_list->info.status == SOURCE_STATUS_NEW) {
			info_list->info.status = SOURCE_STATUS_OPENED;
			info_list->info.odid = ntohl(((struct ipfix_header *) *packet)->observation_domain_id);
		}
	}

	/* pass info to the collector */
	*info = (struct input_info*) &info_list->info;

	/* check whether socket closed */
	if (length == 0) {
#ifdef TLS_SUPPORT
		if (conf->tls) {
			if (SSL_get_shutdown(ssl) != SSL_RECEIVED_SHUTDOWN) {
				MSG_WARNING(msg_module, "SSL shutdown is incomplete");
			}

			/* send "close notify" shutdown alert back to the peer */
			retval = SSL_shutdown(ssl);
			if (retval == -1) {
				MSG_ERROR(msg_module, "Fatal error occured during TLS close notify");
			}
		}
#endif
		/* print info */
		if (conf->info.l3_proto == 4) {
			inet_ntop(AF_INET, (void *)&((struct sockaddr_in*) conf->sock_addresses[sock])->sin_addr, src_addr, INET6_ADDRSTRLEN);
		} else {
			inet_ntop(AF_INET6, &conf->sock_addresses[sock]->sin6_addr, src_addr, INET6_ADDRSTRLEN);
		}
		(*info)->status = SOURCE_STATUS_CLOSED;
		*source_status = SOURCE_STATUS_CLOSED;

		MSG_NOTICE(msg_module, "Exporter on address %s closed connection", src_addr);

		/* use mutex so that listening thread does not reuse the socket too quickly */
		pthread_mutex_lock(&mutex);
		close(sock);
		FD_CLR(sock, &conf->master);
		remove_sock_address(conf, sock);
		pthread_mutex_unlock(&mutex);

		return INPUT_CLOSED;
	}

	return length;
}

/**
 * \brief Input plugin "destructor".
 *
 * \param[in,out] config  plugin_info structure
 * \return 0 on success and config is changed to NULL, nonzero else.
 */
int input_close(void **config)
{
	int ret, error = 0, sock=0;
	struct plugin_conf *conf = (struct plugin_conf*) *config;
	struct input_info_list *info_list;

	/* kill the listening thread */
	if(pthread_cancel(listen_thread) != 0) {
		MSG_WARNING(msg_module, "Cannot cancel listening thread");
	} else {
		pthread_join(listen_thread, NULL);
	}

#ifdef TLS_SUPPORT
	if (conf->tls) {
		/* close TLS connection, if any */
		for (sock = 0; sock < conf->ssl_list_size; sock++) {
			if (conf->ssl_list[sock] == NULL) {
				continue;
			}
			ret = SSL_get_fd(conf->ssl_list[sock]);
			if (ret >= 0) {
				/* send close notify */
				ret = SSL_shutdown(conf->ssl_list[sock]);
				if (ret == -1) {
					MSG_ERROR(msg_module, "Fatal error occured during TLS close notify");
				}
			}

			SSL_free(conf->ssl_list[sock]);
		}
	
		/* we are done here */
		SSL_CTX_free(conf->ctx);
	}
#endif
	
	/* close listening socket */
	if ((ret = close(conf->socket)) == -1) {
		error++;
		MSG_ERROR(msg_module, "Cannot close listening socket: %s", strerror(errno));
	}

	/* close open sockets */
	for (sock = 0; sock <= conf->fd_max; sock++) {
		if (FD_ISSET(sock, &conf->master)) {
			if ((ret = close(sock)) == -1) {
				error++;
				MSG_ERROR(msg_module, "Cannot close socket: %s", strerror(errno));
			}
		}
	}

	destroy_sock_addresses(conf);

	/* free input_info list */
	while (conf->info_list) {
		info_list = conf->info_list->next;
#ifdef TLS_SUPPORT
		X509_free(conf->info_list->exporter_cert);
#endif
		free(conf->info_list);
		conf->info_list = info_list;
	}

	/* free configuration strings */
	if (conf->info.template_life_time != NULL) {
		free(conf->info.template_life_time);
	}
	if (conf->info.template_life_packet != NULL) {
		free(conf->info.template_life_packet);
	}
	if (conf->info.options_template_life_time != NULL) {
		free(conf->info.options_template_life_time);
	}
	if (conf->info.options_template_life_packet != NULL) {
		free(conf->info.options_template_life_packet);
	}
#ifdef TLS_SUPPORT
	/* free strings that are not default (thus static) */
	if (conf->ca_cert_file != NULL) {
		free(conf->ca_cert_file);
	}
	if (conf->server_cert_file != NULL) {
		free(conf->server_cert_file);
	}
	if (conf->server_pkey_file != NULL) {
		free(conf->server_pkey_file);
	}
#endif

	/* free allocated structures */
	FD_ZERO(&conf->master);
	free(*config);
	convert_close();
	*config = NULL;

	MSG_NOTICE(msg_module, "All allocated resources have been freed");

	return error;
}
/**@}*/
