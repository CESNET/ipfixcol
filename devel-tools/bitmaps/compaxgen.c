/**
 * \file bmgen.c
 * \author Radek Krejci <rkrejci@cesnet.cz>
 * \author Petr Kramolis <kramolis@cesnet.cz>
 * \brief ozrgen bitmap generator.
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

#define BITMAP_WORDSIZE 32
#define INIT_OBUFFER_SIZE 1024

#define OPTSTRING "c:hi:w:"

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
	printf ("Usage: bmgenhelp\n");
}

struct bmvalue_t {
	uint32_t index;
	bmword VAH_bm;
};

struct VAH_buffer_t {
	uint32_t offset;
	uint32_t offzero;
	uint32_t size;
	bmword* data;
};

/*check if bmword have only one dirty bajt and return its position*/
/*if there is more than one dirty bajt return -1 */
inline int8_t check_dirty_byte(bmword data)
{
	int dirty_byte=-1;
	int i;
	unsigned char *byte;
	data = data ^ TOP_BIT;
	byte = (unsigned char *)&data;
	//loop through bytes*/
	for(i=0;i<sizeof(bmword);i++){
		/* check if 'i' byte is dirty */
		if(*byte != 0){
			//printf("++dirty_byte:%u -%d\n",(unsigned char)*byte,i);
			if(dirty_byte>=0){
				/*second dirty_byte*/
				//printf("-------Dirty_word: %d - in: %d\n",dirty_byte, data & FULL30);
				return -1;
			}
			dirty_byte=i;
		}
		byte++;
	}
	//printf("Dirty_byte: %d - in: %d\n",dirty_byte, data);
	return dirty_byte;
}

/* return LFL word
 *  l1,l2 - bitmap word with dirty bajt
 *  p1, p2 - position of dirty bajts
 *  f - zero number (with filled only first bajt)
 */
inline bmword make_LFL(bmword *l1, int8_t *p1, bmword *f, bmword *l2, int8_t *p2){
	bmword flf=0;
	flf = TOP_BIT >> 2;
	flf = flf | (((bmword)*p1)<<(3+3*8));
	flf = flf | (((bmword)*p2)<<(1+3*8));
	flf = flf | (((TOP_BIT^(*l1)) >> (*p1*8)) << (2*8));
	flf = flf | (*f << 8);
	flf = flf | (((TOP_BIT^(*l2)) >> (*p2*8)));

	//printf("l1: %d p1: %d f:%d l2: %d p2: %d flf: %d\n", *l1, (uint32_t)*p1, *f, *l2, (uint32_t)*p2,flf);
	return flf;
}

/* return FLF word
 *  l - bitmap word with dirty bajt
 *  p - position of dirty bajt
 *  f1,f2 - zero number (with filled only first bajt)
 */
inline bmword make_FLF(bmword *f1, bmword *l, int8_t *p, bmword *f2){
	bmword lfl=0;
	lfl = TOP_BIT >> 1;
	lfl = lfl | (((bmword)*p)<<(3+3*8));
	lfl = lfl | (*f1 << 8*2);
	lfl = lfl | (((TOP_BIT^(*l)) >> (*p*8)) << 8);
	lfl = lfl | *f2; 

	//printf("f1: %d l: %d p: %d f2:%d  flf: %d\n", *f1, *l, (uint32_t)*p, *f2, lfl);
	return lfl;
}

/*
 */
inline int get_WAH_bmword (FILE *input, struct bmvalue_t* bmvalues, uint32_t cardinality)
{
	int i, j, ret = 1;
	uint16_t index;

	/* clean bmvalues */
	memset (bmvalues, 0, (sizeof(struct bmvalue_t)) * (BITMAP_WORDSIZE - 1));

	/* read items from input */
	for (i = (BITMAP_WORDSIZE - 2); i >= 0; i--) {

		ret = fscanf (input, "%hu\n", &index);

		/* For use with binary input file
		ret = read (input, &index, sizeof(uint16_t));
		*/


		if (ret == EOF) {
			ret = 0;
			break;
		}
  		 /* announce and ignore values higher than cardinality */
		if (index > (cardinality - 1)) {
			printf ("too high value %hu\n", index);
			continue;
		}

		/* find value index or pass next free one */
		j = 0;
		while (bmvalues[j].VAH_bm != 0) {
			if (bmvalues[j].index == index) {
				break;
			} else {
				j++;
			}
		}
		/* actualize bitmap */
		//if(index==53){
		//	printf("53\n");
		//}
		bmvalues[j].index = index;
		bmvalues[j].VAH_bm = bmvalues[j].VAH_bm | ((bmword)1 << i) | TOP_BIT;
		/*
		if(index==1){
			bmword x;
			uint64_t v;
			v = (uint64_t)(1 << (uint64_t)i);
			v = ((uint64_t)1 << i);
			x = bmvalues[j].VAH_bm;
			printf("\n");
		}*/
	}

	return (ret);
}


int main (int argc, char* argv[])
{
	int i, j;
	int card = -1;
	int repeat = 1;
	FILE *input = NULL;
	int out_fd = -1;
	struct VAH_buffer_t *obuffer;
	struct bmvalue_t bmvalues[BITMAP_WORDSIZE - 1];
	off_t block_start, block_end;
	uint32_t block_size;
	uint64_t zeros_offset=0;
	uint32_t *offset_list;
	uint32_t block_offset = 0;
	uint32_t header_size=0;


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
	if ((obuffer = (struct VAH_buffer_t*) malloc (sizeof(struct VAH_buffer_t) * card)) == NULL) {
		perror ("Allocating memory for output buffer array failed");
		return (1);
	}
	memset(obuffer, 0 ,sizeof(struct VAH_buffer_t) * card);


	if ((offset_list = (uint32_t *) malloc (sizeof(uint32_t) * card)) == NULL) {
		perror ("Allocating memory for offset array failed");
		return (1);
	}
	memset(offset_list, 0 ,sizeof(uint32_t) * card);

	zeros_offset=0;
	/* process the data */
	int cc=0;
	while (repeat) {
		cc++;
		for (j = 0; j < RECORDS_PER_BLOCK; j++) {
			repeat = get_WAH_bmword (input, &bmvalues[0], card);
			i = 0;
			while ((i <= (BITMAP_WORDSIZE - 2)) && (bmvalues[i].VAH_bm != 0)) {
				uint32_t index;
				index = bmvalues[i].index;
				//check alocated space for index bitmap
				if(obuffer[index].data==NULL){
					if ((obuffer[index].data = (bmword*) calloc(INIT_OBUFFER_SIZE, sizeof(bmword)))==NULL){
						perror ("Allocating memory for output buffer failed");
						return (1);
					}
					memset(obuffer[index].data,0,obuffer[index].size);
					obuffer[index].size = INIT_OBUFFER_SIZE;
					//printf("alocation i:%d - %p\n",bmvalues[i].index,obuffer[bmvalues[i].index].data);
				}

				//check zero-off
				uint64_t zero_diff;
				bmword data;
				int data_to_write;
				data_to_write=1;
				int lastF=0;
				int8_t p1,p2;
				while(obuffer[index].offzero < zeros_offset || data_to_write){
					zero_diff = zeros_offset - obuffer[index].offzero;
					//check if all zeros squeeze in one word
					if( zero_diff >= ((bmword) TOP_BIT >>2)){
						zero_diff = ((bmword)TOP_BIT-1);
					}
					if(zero_diff!=0){
						lastF=0;
						data= (bmword) zero_diff;
						//check LFL
						if(obuffer[index].offset>2){
							if(data<255){
								lastF=1;
								if(obuffer[index].data[obuffer[index].offset-2]<255){
									if((p1=check_dirty_byte(obuffer[index].data[obuffer[index].offset-1]))>=0){
										data=make_FLF(&(obuffer[index].data[obuffer[index].offset-2]), &(obuffer[index].data[obuffer[index].offset-1]), &p1, &data);
										obuffer[index].offset -= 2;
										lastF=0;
									}
								}
							}
						}
					}else{
						data=bmvalues[i].VAH_bm;
						data_to_write=0;
						if(obuffer[index].offset>2){
							if(lastF==1){
								//check LFL
								if((p1=check_dirty_byte(data))>=0){
									if((p2=check_dirty_byte(obuffer[index].data[obuffer[index].offset-2] ))>=0){
										//printf("LFL\n");
										data=make_LFL(&(obuffer[index].data[obuffer[index].offset-2]),&p2,&(obuffer[index].data[obuffer[index].offset-1]),&data,&p1);
										memset(&(obuffer[index].data[obuffer[index].offset-1]),0,sizeof(bmword));
										obuffer[index].offset -= 2;
									}
								}
							}
						}
						lastF=0;
					}
					//if(index==0){
					//	printf("data: %d offset: %d\n",data, obuffer[index].offset);	
					//}
					obuffer[index].data[obuffer[index].offset] = data;
					obuffer[index].offzero += zero_diff;
					obuffer[index].offset++;

					//check alocated space
					if (obuffer[index].offset == obuffer[index].size) {
						/* double size of the memory buffer for this index */
						obuffer[index].size *= 2;
						if((obuffer[index].data = (bmword*) realloc (obuffer[index].data, sizeof(bmword) * obuffer[index].size))==NULL){
							perror ("Reallocating memory for output buffer failed");
							return (1);
						}
						memset(&obuffer[index].data[obuffer[index].offset], 0, (obuffer[index].size / 2));
						//printf("Realocation i:%d - %p\n",bmvalues[i].index,obuffer[bmvalues[i].index].data);
					}
				}
				//increase zero-offset
				obuffer[index].offzero++;
				i++;

			}
			zeros_offset++;
			/* check end of input file */
			if (repeat == 0) {
				break;
			}

		}

		block_offset=1;
		/* flush all data */
		header_size =  (card+1) * sizeof(uint32_t);
		block_start = lseek(out_fd,header_size, SEEK_CUR) - header_size;
		for (i = 0; i < card; i++) {
			if (obuffer[i].data==NULL){
				offset_list[i]=0;
				continue;
			}

			if (obuffer[i].data[obuffer[i].offset] == 0) {
				obuffer[i].offset--;
			}

			//printf("\n%d\n",i);
			//printf("pozice %d offsetu v souboru: %ld \n",i,(unsigned long int)lseek(out_fd,0, SEEK_CUR));
			//if(i==53){
			//	printf("offset: %d\n",obuffer[i].offset);
			//}
			write (out_fd, &obuffer[i].offset, sizeof(uint32_t));
			offset_list[i]= block_offset;
			//printf("\tof:%d block_offset:%d\n\n",obuffer[i].offset,block_offset);
			block_offset += sizeof(uint32_t);

			
			/*int g=0;
			for(g=0;g<=obuffer[i].offset;g++){
				if(TOP_BIT & obuffer[i].data[g]){
					printf("\t1 ");
				}else{ 
					printf("\t0 ");
				}
				#if BITMAP_WORDSIZE == 16
					printf("%hd\n",(uint16_t)obuffer[i].data[g]);	
				#elif BITMAP_WORDSIZE == 32
					printf("%d\n",(uint32_t) obuffer[i].data[g]);	
				#elif BITMAP_WORDSIZE == 64
					printf("%ld\n",(uint64_t)obuffer[i].data[g]);	
				#endif

			}*/
			write (out_fd, obuffer[i].data, sizeof(bmword) * (obuffer[i].offset+1));
			block_offset += sizeof(bmword) * (obuffer[i].offset +1);

			if (obuffer[i].data != NULL) {

				fflush(stdout);
				free(obuffer[i].data);

				obuffer[i].data = NULL;
				obuffer[i].size = 0;
				obuffer[i].offset = 0;
			} else {
				obuffer[i].offset = 0;
			}
			//printf("block offset for %d:%d\n",i,offset_list[i]);
		}

		block_end = lseek(out_fd, 0, SEEK_CUR);
		lseek(out_fd, block_start, SEEK_SET);
		block_size = block_end - block_start - sizeof(uint32_t);
		write(out_fd, &block_size, sizeof(uint32_t));
		//write offsets 
		// write seznam offsetu
		write(out_fd, offset_list, card * sizeof(uint32_t));
		lseek(out_fd, block_end, SEEK_SET);
//printf("Block: )from %lu to %lu (size %u)\n", block_start, block_end, block_size);
	//	printf("Block %d, size %u, block_start: %ld\n", cc, block_size, block_start);
	}

	fclose (input);
	close (out_fd);
	free(offset_list);
	free(obuffer);

	return (0);
}
