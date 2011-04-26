/**
 * \file bmread.c
 * \author <name> <email>
 * \brief <Idea of what it does>
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

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#define OPTSTRING "c:hi:"

void print_help ()
{
	printf ("help\n");
}

int main (int argc, char* argv[])
{
	int i, search = 0, j = 0, in_fd = -1;
	off_t offset = 0, block_start;
	uint32_t block_size;

	while ((i = getopt (argc, argv, OPTSTRING)) != -1) {
		switch (i) {
		case 'c':
			search = atoi (optarg);
			break;
		case 'h':
			print_help ();
			return (0);
			break;
		case 'i':
			if ((in_fd = open (optarg, O_RDONLY)) == -1) {
				perror ("Opening input file failed");
				return (1);
			}
			break;
		}
	}

	while(read(in_fd, &block_size, sizeof(uint32_t)) != 0) {
		j++;
		block_start = lseek (in_fd, 0, SEEK_CUR);
		printf("Block %d, size %u, ", j, block_size);
		for (i = 0; i < search; i++) {
			read(in_fd, &offset, sizeof(uint32_t));
			lseek(in_fd, (offset + 1) * sizeof(uint32_t), SEEK_CUR);
		}
		read(in_fd, &offset, sizeof(uint32_t));
		printf("size of buffer %d: %lu\n", search, (offset + 1) * sizeof(uint32_t));
		lseek(in_fd, block_start + block_size, SEEK_SET);
	}

	return (0);
}
