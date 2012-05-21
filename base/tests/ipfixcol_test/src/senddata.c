/**
 * \file senddata.c
 * \author Petr Velan <petr.velan@cesnet.cz>
 * \brief IPFIX Colector input plugin tester

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
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define ERROR(message) fprintf(stderr, message"\n")

/* used socket */
int sock;

/**
 * \brief Creates connection on specified port using given socket type
 *
 * \param[in] port String specifying connection port
 * \param[in] socktype Type of the socket (SOCK_STREAM for TCP or SOCK_DGRAM for UDP)
 * \return 0 on success, 1 on socket error or 2 when plugin is not listening
 */
int create_connection(char *port, int socktype)
{
	struct addrinfo hints, *addrinfo;

	memset(&hints, 0, sizeof hints);
	/* protocol family, default is IPv4 */
	hints.ai_family = AF_INET;
	hints.ai_socktype = socktype;

	/* get server address */
	if (getaddrinfo("localhost", port, &hints, &addrinfo) != 0) {
		ERROR("Cannot get server info");
		return 1;
	}

	/* create socket */
	sock = socket(addrinfo->ai_family, addrinfo->ai_socktype, addrinfo->ai_protocol);
	if (sock == -1) {
		ERROR("Cannot create new socket");
		return 1;
	}

	/* connect to server */
	if (connect(sock, addrinfo->ai_addr, addrinfo->ai_addrlen) == -1) {
		close(sock);
		ERROR("Cannot connect to collector");
		return 2;
	}

    freeaddrinfo(addrinfo);

	return 0;
}

/**
 * \brief Close current connection
 */
void close_connection()
{
    close(sock);
}

/**
 * \brief Send data on current connection
 *
 * \param[in] data buffer with data to send
 * \param[in] length length of data in buffer
 * \return 0 on success, 1 on socket error or 2 when plugin is not listening
 */
int send_data(char *data, size_t length)
{
	ssize_t sent = 0;

    /* send data to plugin */
    if ((sent = send(sock, data, length, 0)) == -1)
    {
        ERROR("Cannot send data to collector");
        return 1;
    }

    return 0;
}

void usage()
{
	printf("Usage: senddata -t protocol [-p port] [-h] file1 file2 ...\n"
			"  -t protocol  One of \"udp\" or \"tcp\"\n"
			"  -p port      Set port. Default is 4739\n"
			"  -h           Print this help\n"
	);
}

int main(int argc, char *argv[]) {

	char *buff, *port = NULL;
	size_t length;
	int fd, i, opt, socktype = -1;
	struct stat st;

	/* parse input */
	while ((opt = getopt(argc, argv, "p:t:h")) != -1) {
		switch (opt) {
		case 'p':
			port = optarg;
			break;
		case 't':
			if (strcmp("udp", optarg) == 0) {
				socktype = SOCK_DGRAM;
			} else if (strcmp("tcp", optarg) == 0) {
				socktype = SOCK_STREAM;
			} else {
				ERROR("-t supports only udp or tcp");
				usage();
				return 1;
			}
			break;
		case 'h':
			usage();
			return 0;
			break;
		default:
			usage();
			return 1;
			break;
		}
	}

	/* set default port */
	if (port == NULL) {
		port = "4739";
	}

	if (socktype == -1) {
		usage();
		return 1;
	}

	if (optind >= argc) {
		ERROR("No files to send");
		return 2;
	}

	/* send every file */
	create_connection(port, socktype);

	for (i = optind; i < argc; i++) {
		/* open file */
		fd = open(argv[i], O_RDONLY);
		if (fd == -1) {
			perror("open");
			continue;
		}

		/* get file size and read all */
		fstat(fd, &st);
		buff = malloc(st.st_size);
		length = read(fd, buff, st.st_size);

		close(fd);
		send_data(buff, length);

		free(buff);
	}

	close_connection();

	return 0;
}
