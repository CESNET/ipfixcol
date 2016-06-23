/**
 * \file reader.c
 * \author Michal Kozubik <kozubik@cesnet.cz>
 * \brief Functions for reading IPFIX file
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

#include <stdio.h>
#include <stdlib.h>
#include <ipfixcol.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#include "reader.h"
#include "ipfixsend.h"

/**
 * \brief Free allocated memory for packets
 */
void free_packets(char **packets)
{
    int i;
    for (i = 0; packets[i]; ++i) {
        free(packets[i]);
        packets[i] = NULL;
    }
    
    free(packets);
}

/**
 * \brief Read packet header from file
 * 
 * @param buff read buffer
 * @param fd input file
 * @return On success returns packet length. On end of file returns READ_EOR.
 * Otherwise returns negative value.
 */
int read_header(char *buff, int fd)
{
    int len = read(fd, buff, IPFIX_HEADER_LENGTH);
	if (len == 0) {
		return READ_EOF;
	} else if (len != IPFIX_HEADER_LENGTH) {
        fprintf(stderr, "Cannot read packet header, malformed input file\n");
        return -1;
    }
    
	const struct ipfix_header *header = (struct ipfix_header *) buff;

	// Check header version
	if (ntohs(header->version) != IPFIX_VERSION) {
		fprintf(stderr, "Invalid version of packet header.\n");
		return -1;
	}
    
	return htons(header->length);
}

/**
 * \brief Read packet from file
 */
char *read_packet(int fd, int *status)
{
    int pkt_len;
    
    /* Allocate space for header */
    char *pkt = calloc(1, IPFIX_HEADER_LENGTH);
    if (!pkt) {
        ERR_MEM;
		*status = READ_ERROR;
        return NULL;
    }
    
    /* Read header */
    pkt_len = read_header(pkt, fd);
	if (pkt_len == 0) {
		free(pkt);
		*status = READ_EOF;
		return NULL;
	} else if (pkt_len < 0) {
        free(pkt);
		*status = READ_ERROR;
        return NULL;
    } else if (pkt_len > IPFIX_HEADER_LENGTH) {
        /* Create space for packet */
        pkt = realloc(pkt, pkt_len);
        if (!pkt) {
            ERR_MEM;
			*status = READ_ERROR;
            return NULL;
        }
        
        /* Read message body */
        int readed = read(fd, pkt + IPFIX_HEADER_LENGTH, pkt_len - IPFIX_HEADER_LENGTH);
        if (readed != pkt_len - IPFIX_HEADER_LENGTH) {
            fprintf(stderr, "Cannot read packet!\n");
            free(pkt);
			*status = READ_ERROR;
            return NULL;
        }
    }
    
	*status = READ_OK;
    return pkt;
}

/**
 * \brief Read packets from IPFIX file and store them into memory
 */
char **read_packets(char *input)
{
    int fd = open(input, O_RDONLY);
    int status = READ_OK;
	
    if (fd == -1) {
        fprintf(stderr, "Cannot open file \"%s\": %s.\n", input, strerror(errno));
        return NULL;
    }
    
    int pkt_cnt = 0, pkt_max = 32;
    char **packets = calloc(pkt_max, sizeof(char *));
    if (!packets) {
        ERR_MEM;
        close(fd);
        return NULL;
    }
    
    while (1) {
        /* Read packet */
        packets[pkt_cnt] = read_packet(fd, &status);
		if (status == READ_ERROR) {
			close(fd);
			free(packets);
			return NULL;
		} else if (status == READ_EOF) {
			break;
		}
        
        /* Move array index to next packet - resize array if needed */
        pkt_cnt++;
        if (pkt_cnt == pkt_max) {
            /* Not enough space in array */
            pkt_max *= 2;
            packets = realloc(packets, pkt_max * sizeof(char *));
            if (!packets) {
                ERR_MEM;
                close(fd);
                return NULL;
            }
            
            packets[pkt_cnt] = NULL;
        }
    }

    close(fd);
    
    packets[pkt_cnt] = NULL;
    return packets;
}

/**
 * \brief Get file length
 * 
 * @return file length
 */
long file_size(FILE *f)
{
	/* Go to end of file */
	fseek(f, 0, SEEK_END);
	
	/* Get position */
	long fsize = ftell(f);
	
	/* Back to beginning */
	fseek(f, 0, SEEK_SET);
	
	return fsize;
}

/**
 * \brief Read file
 */
char *read_file(char *input, long *fsize)
{
	FILE *f = fopen(input, "r");
	
	if (!f) {
		fprintf(stderr, "Cannot open file \"%s\"!", input);
		return NULL;
	}
	
	/* Get file size and allocate space */
	*fsize = file_size(f);
	if (*fsize < 0) {
		fprintf(stderr, "Cannot determine file size of \"%s\"!", input);
		fclose(f);
		return NULL;
	}

	char *data = calloc(1, *fsize + 1);
	if (!data) {
		ERR_MEM;
		fclose(f);
		return NULL;
	}
	
	/* Read file */
	size_t read = fread(data, *fsize, 1, f);
	data[read] = 0;
	
	/* Close */
	fclose(f);
	
	return data;
}
