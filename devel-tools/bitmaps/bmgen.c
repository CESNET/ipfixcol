/**
 * \file bmgen.c
 * \author Radek Krejci <rkrejci@cesnet.cz>
 * \brief VAH (TODO: COMPAX) bitmap generator.
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>

#define BIT31 0x80000000
#define BITMAP_WORDSIZE 32
#define INIT_OBUFFER_SIZE 1024
#define RECORDS_PER_BLOCK ((1024)-1)
#define FULL_0FILL 0x7FFFFFFF

#define OPTSTRING "c:hi:w:"

void print_help ()
{
	printf ("Usage: bmgenhelp\n");
}

struct bmvalue_t {
	uint32_t index;
	uint32_t VAH_bm;
};

struct VAH_buffer_t {
	uint32_t offset;
	uint32_t size;
	uint32_t* data;
};

struct VAH_buffer_list_t {
	struct VAH_buffer_t* buffer;
	struct VAH_buffer_list_t* next;
};

/*
 */
inline int get_WAH_bmword (FILE *input, struct bmvalue_t* bmword, uint32_t cardinality)
{
	int i, j, ret = 1;
	uint16_t index;
	//	static int counter = 0;

	/* clean bmword */
	memset (bmword, 0, (sizeof(struct bmvalue_t)) * (BITMAP_WORDSIZE - 1));

	/* read 31 items from input */
	for (i = (BITMAP_WORDSIZE - 2); i >= 0; i--) {

/* For use with text input file */
		ret = fscanf (input, "%hu\n", &index);
/* For use with binary input file
		ret = read (input, &index, sizeof(uint16_t));
*/
		if (ret == EOF) {
			ret = 0;
			break;
		}
		/* Ignore values from file and generate own values
		 if (counter > 50000) {ret=0;break;}
		 index = counter++ % 3; ret = 1;
		 */
		if (index > (cardinality - 1)) {
			printf ("too high value %hu\n", index);
			continue;
		}

		j = 0;
		while (bmword[j].VAH_bm != 0) {
			if (bmword[j].index == index) {
				break;
			} else {
				j++;
			}
		}
		bmword[j].index = index;
		bmword[j].VAH_bm = bmword[j].VAH_bm | (1 << i) | BIT31;
	}

	return (ret);
}


int main (int argc, char* argv[])
{
	int i, j;
	int card = -1;
	int repeat = 1;
	FILE *input = NULL;
	int out_fd = -1, in_fd = -1;
	struct VAH_buffer_t *obuffer;
	struct bmvalue_t bmword[BITMAP_WORDSIZE - 1];
	struct VAH_buffer_list_t obuffer_list, *obuffer_list_tail, *obuffer_list_aux;
	struct VAH_buffer_t* init_obuffer;
	off_t block_start, block_end;
	uint32_t block_size;

	while ((i = getopt (argc, argv, OPTSTRING)) != -1) {
		switch (i) {
		case 'c':
			card = atoi (optarg);
			break;
		case 'h':
			print_help ();
			return (0);
			break;
		case 'i':
/* For use with text input file */
			if ((input = fopen (optarg, "r")) == NULL) {
/* For use with binary input file
			if ((in_fd = open (optarg, O_RDONLY)) == -1) {
*/
				perror ("Opening input file failed");
				return (1);
			}
			break;
		case 'w':
			if ((out_fd = open (optarg, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) == -1) {
				perror ("Opening output file failed");
				return (1);
			}
			break;
		}
	}

	if (card <= 0) {
		printf ("Cardinality is too low (%d)\n", card);
		return (1);
	}

	/*
	 * init storage space
	 */
	/* output buffers list */
	if ((init_obuffer = (struct VAH_buffer_t*) malloc (sizeof(struct VAH_buffer_t))) == NULL) {
		perror ("Allocating memory failed");
		return (1);
	}
	if ((obuffer = (struct VAH_buffer_t*) malloc (sizeof(struct VAH_buffer_t) * card)) == NULL) {
		perror ("Allocating memory for output buffer array failed");
		return (1);
	}
	/* allocate output buffer for one value and make all values to point it */
	if ((init_obuffer->data = (uint32_t*) calloc (INIT_OBUFFER_SIZE, sizeof(uint32_t))) == NULL) {
		perror ("Allocating memory for output buffer failed");
		return (1);
	}
	init_obuffer->offset = 0;
	init_obuffer->size = INIT_OBUFFER_SIZE;
	for (i = 0; i < card; i++) {
		memcpy (&obuffer[i], init_obuffer, sizeof(struct VAH_buffer_t));
	}
	obuffer_list.buffer = init_obuffer;
	obuffer_list.next = NULL;
	obuffer_list_tail = &obuffer_list;

	/* process the data */
	while (repeat) {
		for (j = 0; j < RECORDS_PER_BLOCK; j++) {
			repeat = get_WAH_bmword (input, &bmword[0], card);
			i = 0;
			while ((i <= (BITMAP_WORDSIZE - 2)) && (bmword[i].VAH_bm != 0)) {
				if (obuffer[bmword[i].index].data == init_obuffer->data) {
					memcpy (&obuffer[bmword[i].index], init_obuffer, sizeof(struct VAH_buffer_t));
					if ((obuffer[bmword[i].index].data = (uint32_t*) calloc (init_obuffer->size, sizeof(uint32_t))) == NULL) {
						perror ("Allocating memory for output buffer failed");
						return (1);
					}
					memcpy (&obuffer[bmword[i].index].data, init_obuffer->data, sizeof(uint32_t) * init_obuffer->offset);
					if ((obuffer_list_tail->next = (struct VAH_buffer_list_t*) malloc ( sizeof(struct VAH_buffer_list_t))) == NULL) {
						perror ("Allocating memory for output buffer failed");
						return (1);
					}
					obuffer_list_tail = obuffer_list_tail->next;
					obuffer_list_tail->buffer = &obuffer[bmword[i].index];
					obuffer_list_tail->next = NULL;
				}

				/*check if is offset realy empty (zero count can be there!)*/
				if(obuffer[bmword[i].index].data[obuffer[bmword[i].index].offset] != 0 ){
					/*skip zero count by increasing offset (alocated memory check needed)*/
					obuffer[bmword[i].index].offset++;
					if (obuffer[bmword[i].index].offset == obuffer[bmword[i].index].size) {
						/* double size of the memory buffer for this index */
						obuffer[bmword[i].index].size *= 2;
						obuffer[bmword[i].index].data = (uint32_t*) realloc ( obuffer[bmword[i].index].data, sizeof(uint32_t) * obuffer[bmword[i].index].size);
						memset(&obuffer[bmword[i].index].data[obuffer_list_tail->buffer->offset], 0, (obuffer[bmword[i].index].size / 2));
					}
				}
				obuffer[bmword[i].index].data[obuffer[bmword[i].index].offset] = bmword[i].VAH_bm;
				i++;
			}
			/* increment 0-fills */
			obuffer_list_tail = &obuffer_list;
			do {
				if (((obuffer_list_tail->buffer->data[obuffer_list_tail->buffer->offset]) & BIT31) != 0) {
					/* changed in last run */
					obuffer_list_tail->buffer->offset++;
					if (obuffer_list_tail->buffer->offset == obuffer_list_tail->buffer->size) {
						/* double size of the memory buffer for this index */
						obuffer_list_tail->buffer->size *= 2;
						obuffer_list_tail->buffer->data = (uint32_t*) realloc (obuffer_list_tail->buffer->data, sizeof(uint32_t) * obuffer_list_tail->buffer->size);
						memset(&obuffer_list_tail->buffer->data[obuffer_list_tail->buffer->offset], 0, (obuffer_list_tail->buffer->size / 2));
					}
				} else {
					if (obuffer_list_tail->buffer->data[obuffer_list_tail->buffer->offset] == FULL_0FILL) {
						obuffer_list_tail->buffer->offset++;
						if (obuffer_list_tail->buffer->offset == obuffer_list_tail->buffer->size) {
							/* double size of the memory buffer for this index */
							obuffer_list_tail->buffer->size *= 2;
							obuffer_list_tail->buffer->data = (uint32_t*) realloc (obuffer_list_tail->buffer->data, sizeof(uint32_t) * obuffer_list_tail->buffer->size);
							memset(&obuffer_list_tail->buffer->data[obuffer_list_tail->buffer->offset], 0, (obuffer_list_tail->buffer->size / 2));
						}
					}
					obuffer_list_tail->buffer->data[obuffer_list_tail->buffer->offset]++;
				}
			} while ((obuffer_list_tail->next != NULL) && (obuffer_list_tail = obuffer_list_tail->next));

			/* check end of input file */
			if (repeat == 0) {
				break;
			}
		}

		/* flush all data */
		block_start = lseek(out_fd, sizeof(uint32_t), SEEK_CUR) - sizeof(uint32_t);
		for (i = 0; i < card; i++) {
			if (obuffer[i].data[obuffer[i].offset] == 0) {
				obuffer[i].offset--;
			}
			write (out_fd, &obuffer[i].offset, sizeof(uint32_t));
if (i == 80) {
	printf("size of buffer 80: %u\n", obuffer[i].offset);
}
			write (out_fd, obuffer[i].data, sizeof(uint32_t) * (obuffer[i].offset + 1));
			if (obuffer[i].data != init_obuffer->data) {
				free(obuffer[i].data);
				obuffer[i].data = init_obuffer->data;
				obuffer[i].size = init_obuffer->size;
				obuffer[i].offset = 0;
			} else {
				obuffer[i].offset = 0;
			}
		}
		memset(init_obuffer->data, 0, sizeof(uint32_t) * (init_obuffer->offset + 1));
		init_obuffer->offset = 0;

		block_end = lseek(out_fd, 0, SEEK_CUR);
		lseek(out_fd, block_start, SEEK_SET);
		block_size = block_end - block_start - sizeof(uint32_t);
		write(out_fd, &block_size, sizeof(uint32_t));
		lseek(out_fd, block_end, SEEK_SET);
//printf("Block: from %lu to %lu (size %u)\n", block_start, block_end, block_size);
		obuffer_list_tail = obuffer_list.next;
		obuffer_list.next = NULL;
		while (obuffer_list_tail) {
			obuffer_list_aux = obuffer_list_tail;
			obuffer_list_tail = obuffer_list_tail->next;
			free (obuffer_list_aux);
		}
		obuffer_list_tail = &obuffer_list;
	}

//	close (in_fd);
	fclose (input);
	close (out_fd);

	return (0);
}
