/**
 * \file ozrread.c
 * \author Petr Kramolis kramolis@cesnet.cz
 * \brief ozr bitmap reader
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
#include <string.h>

#define OPTSTRING "c:hi:"

#define BITMAP_WORDSIZE 32

#if BITMAP_WORDSIZE == 16 
        #define RECORDS_PER_BLOCK ((4096)-1) 
        #define TOP_BIT 0x8000 
        typedef uint16_t bmword; 
#elif BITMAP_WORDSIZE == 32 
        #define RECORDS_PER_BLOCK ((2048)-1) 
        #define TOP_BIT 0x80000000 
        typedef uint32_t bmword; 
#elif BITMAP_WORDSIZE == 64 
        #define RECORDS_PER_BLOCK ((1024)-1) 
        #define TOP_BIT 0x8000000000000000 
        typedef uint64_t bmword; 
#endif


void print_help ()
{
	printf ("help\n");
}

int main (int argc, char* argv[])
{
	int i, search = 0, j = 0, in_fd = -1;
	uint64_t pozice=0;
	off_t  block_start;
	uint32_t block_size=0;
	uint32_t card = 65536;
        uint32_t *offset_list;
	uint32_t offset;
	bmword * bitmaps;

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

	/* read block */
	while(read(in_fd, &block_size, sizeof(uint32_t)) != 0) {
		j++;
		block_start = lseek (in_fd, 0, SEEK_CUR);
		//printf("Block %d, size %u, block_start: %ld\n", j, block_size, block_start-4);
		/* read position of offset for i value */
		//printf("r-count: %d, b-size:%d\n",records_count,bitmap_size);
		

		/* list of offsets */
		if ((offset_list = (uint32_t *) malloc (sizeof(uint32_t) * card)) == NULL) {
			perror ("Allocating memory for offset array failed");
			return (1);
		}
		memset(offset_list, 0 ,sizeof(uint32_t) * card);

		if(read(in_fd, offset_list, card *sizeof(uint32_t)) ==0){
			printf("unable to read offset list\n");
			exit(1);
		}
		
		/*int i;
		for(i=0;i<card;i++){
			printf("value: %d ,offset: %d\n",i,offset_list[i]);
		}*/
		
		/* there is at last one bitmap for searched value */
		if(offset_list[search]!=0){
			//read searched value offset
			lseek(in_fd, block_start + card *sizeof(uint32_t) + offset_list[search] -1, SEEK_SET);
			if(read(in_fd, &offset, sizeof(uint32_t)) ==0){
				printf("unable to read offset\n");
				exit(1);
			}
			
			if ((bitmaps = (bmword *) malloc (sizeof(bmword) * (offset +1))) == NULL) {
				perror ("Allocating memory for bitmap array failed");
				return (1);
			}
			memset(bitmaps, 0 , sizeof(bmword) * (offset +1)); // coment out


			if(read(in_fd, bitmaps, sizeof(bmword) * (offset +1)) ==0){
				printf("unable to read bitmaps list\n");
				exit(1);
			}

			for(i=0; i<=offset; i++){
				//check L word
				if(bitmaps[i] & TOP_BIT){
					int j;
					for(j=1;j<BITMAP_WORDSIZE;j++){
						if(TOP_BIT & (bitmaps[i]<<j)){
							printf("pozice: %lu\n",pozice+j);
						}
					}
					pozice+=BITMAP_WORDSIZE-1;
				} else { // F word
					pozice+=bitmaps[i]*(BITMAP_WORDSIZE-1);
				}
				
			}
		} 	
	
		//set psition to next block header
		lseek(in_fd, block_start + block_size, SEEK_SET);
	}

	return (0);
}
