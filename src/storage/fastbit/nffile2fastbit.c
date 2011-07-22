

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <commlbr.h>
//#include <sys/types.h>
#include <unistd.h>

#include "nffile.h"

void hex(void *ptr, int size){
        int i,space = 0;
        for(i=1;i<size+1;i++){
                if(!((i-1)%16)){
                        fprintf(stderr,"%p  ", &((char *)ptr)[i-1]);
                }
                fprintf(stderr,"%02hhx",((char *)ptr)[i-1]);
                if(!(i%8)){
                        if(space){
                                fprintf(stderr,"\n");
                                space = 0;
                                continue;
                        }
                        fprintf(stderr," ");
                        space = 1;
                }
                fprintf(stderr," ");
        }
}

struct extension{
	uint16_t *value; //map array
	int values_count;
	int id;
};

struct extensions{
	int filled;
	int size;
	struct extension *map;
};

struct storage{
	int x;
};

//EXTENSION 0 -- not a real extension its just pading ect
void ext0_parse(uint32_t *data, int *offset, uint8_t flags, struct storage *s){
	VERBOSE(CL_VERBOSE_ADVANCED,"\tZERO EXTENSION");
}

//EXTENSION 1	
void ext1_parse(uint32_t *data, int *offset, uint8_t flags, struct storage *s){
	if(TestFlag(flags, FLAG_IPV6_ADDR)){
		VERBOSE(CL_VERBOSE_ADVANCED,"\tIPv6-SRC: hight:%lu low:%lu",*((uint64_t *) &data[*offset]), \
			*((uint64_t *) &data[(*offset)+8]));
			*offset+=4;

		VERBOSE(CL_VERBOSE_ADVANCED,"\tIPv6-DST: hight:%lu low:%lu",*((uint64_t *) &data[*offset]), \
			*((uint64_t *) &data[(*offset)+8]));
			*offset+=4;
	} else {
		VERBOSE(CL_VERBOSE_ADVANCED,"\tIPv4-SRC: %u", *((uint32_t *) &data[*offset]));
		(*offset)++;
		VERBOSE(CL_VERBOSE_ADVANCED,"\tIPv4-DST: %u", *((uint32_t *) &data[*offset]));
		(*offset)++;
	}
}

//EXTENSION 2
void ext2_parse(uint32_t *data, int *offset, uint8_t flags, struct storage *s){
	if(TestFlag(flags, FLAG_PKG_64)){
		VERBOSE(CL_VERBOSE_ADVANCED,"\tPACKET COUNTER: %lu", *((uint64_t *) &data[*offset]));
		*offset+=2;
	} else {
		VERBOSE(CL_VERBOSE_ADVANCED,"\tPACKET COUNTER: %u", *((uint32_t *) &data[*offset]));
		(*offset)++;
	}
}
void ext3_parse(uint32_t *data, int *offset, uint8_t flags, struct storage *s){
	if(TestFlag(flags, FLAG_BYTES_64)){
		VERBOSE(CL_VERBOSE_ADVANCED,"\tBYTE COUNTER: %lu", *((uint64_t *) &data[*offset]));
		*offset+=2;
	} else {
		VERBOSE(CL_VERBOSE_ADVANCED,"\tBYTE COUNTER: %u", *((uint32_t *) &data[*offset]));
		(*offset)++;
	}
}

void (*ext_parse[26]) (uint32_t *data, int *offset, uint8_t flags, struct storage *s) = {
	ext0_parse,
	ext1_parse,
	ext2_parse,
	ext3_parse,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,

	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,

	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
};


int main(){
	char file[] = "/home/kramolis/nffile/nfcapd.201107200815";
	FILE *f;
	int i;
	char *buffer = NULL;
	int buffer_size = 0;
	struct file_header_s header;
	struct stat_record_s stats;
	struct data_block_header_s block_header;
	struct common_record_s *record;
	struct extension_map_s *extension_map;
	struct extensions ext = {0,20,NULL};
	int read_size;
	

	//inital space for extension map
	ext.map = (struct extension *) calloc(sizeof(struct extension),ext.size);
	if(ext.map == NULL){
		VERBOSE(CL_ERROR,"Can't read allocate memory for extension map");
		return 1;
        }

	verbose = CL_VERBOSE_ADVANCED;
	f = fopen(file,"r");

	if(f != NULL){
		//read header of nffile
		read_size = fread(&header, sizeof(struct file_header_s), 1,f);
		if (read_size != 1){
			VERBOSE(CL_ERROR,"Can't read file header: %s",file);
			fclose(f);
			return 1;
		}
		VERBOSE(CL_VERBOSE_ADVANCED,"Parsed header from: '%s'",file);
		VERBOSE(CL_VERBOSE_ADVANCED,"\tMAGIC: %x", header.magic);
		VERBOSE(CL_VERBOSE_ADVANCED,"\tVERSION: %i", header.version);
		VERBOSE(CL_VERBOSE_ADVANCED,"\tFLAGS: %i", header.flags);
		VERBOSE(CL_VERBOSE_ADVANCED,"\tNUMBER OF BLOCKS: %i", header.NumBlocks);
		VERBOSE(CL_VERBOSE_ADVANCED,"\tIDENT: '%s'", header.ident);


		read_size = fread(&stats, sizeof(struct stat_record_s), 1,f);
		if (read_size != 1){
			VERBOSE(CL_ERROR,"Can't read file statistics: %s",file);
			fclose(f);
			return 1;
		}
		
		VERBOSE(CL_VERBOSE_ADVANCED,"Parsed statistics from: '%s'",file);
		VERBOSE(CL_VERBOSE_ADVANCED,"\tFLOWS: %lu", stats.numflows);
		VERBOSE(CL_VERBOSE_ADVANCED,"\tBYTES: %lu", stats.numbytes);
		VERBOSE(CL_VERBOSE_ADVANCED,"\tPACKTES: %lu", stats.numpackets);
		VERBOSE(CL_VERBOSE_ADVANCED,"\tFLOWS-TCP: %lu", stats.numflows_tcp);
		VERBOSE(CL_VERBOSE_ADVANCED,"\tFLOWS-UDP: %lu", stats.numflows_udp);
		VERBOSE(CL_VERBOSE_ADVANCED,"\tFLOWS-ICMP: %lu", stats.numflows_icmp);
		VERBOSE(CL_VERBOSE_ADVANCED,"\tFLOWS-OTHER: %lu", stats.numflows_other);
		VERBOSE(CL_VERBOSE_ADVANCED,"\tBYTES-TCP: %lu", stats.numbytes_tcp);
		VERBOSE(CL_VERBOSE_ADVANCED,"\tBYTES-UDP: %lu", stats.numbytes_udp);
		VERBOSE(CL_VERBOSE_ADVANCED,"\tBYTES-ICMP: %lu", stats.numbytes_icmp);
		VERBOSE(CL_VERBOSE_ADVANCED,"\tBYTES-OTHER: %lu", stats.numbytes_other);
		VERBOSE(CL_VERBOSE_ADVANCED,"\tPACKETS-TCP: %lu", stats.numpackets_tcp);
		VERBOSE(CL_VERBOSE_ADVANCED,"\tPACKETS-UDP: %lu", stats.numpackets_udp);
		VERBOSE(CL_VERBOSE_ADVANCED,"\tPACKETS-ICMP: %lu", stats.numpackets_icmp);
		VERBOSE(CL_VERBOSE_ADVANCED,"\tPACKETS-OTHER: %lu", stats.numpackets_other);
		VERBOSE(CL_VERBOSE_ADVANCED,"\tFIRST-SEEN: %u", stats.first_seen);
		VERBOSE(CL_VERBOSE_ADVANCED,"\tLAST-SEEN: %u", stats.last_seen);
		VERBOSE(CL_VERBOSE_ADVANCED,"\tMSEC-FIRST: %hu", stats.msec_first);
		VERBOSE(CL_VERBOSE_ADVANCED,"\tMSEC-LAST: %hu", stats.msec_last);
		VERBOSE(CL_VERBOSE_ADVANCED,"\tSEQUENCE-FAIULURE: %u", stats.sequence_failure);

		
		for(i = 0; i < header.NumBlocks ; i++){
			VERBOSE(CL_VERBOSE_ADVANCED,"---------?C--------");
			read_size = fread(&block_header, sizeof(struct data_block_header_s), 1,f);
			if (read_size != 1){
				VERBOSE(CL_ERROR,"Can't read block header: %s",file);
				fclose(f);
				return 1;
			}

			VERBOSE(CL_VERBOSE_ADVANCED,"BLOCK: %u",i);
			VERBOSE(CL_VERBOSE_ADVANCED,"\tRECORDS: %u", block_header.NumRecords);
			VERBOSE(CL_VERBOSE_ADVANCED,"\tSIZE: %u", block_header.size);
			VERBOSE(CL_VERBOSE_ADVANCED,"\tID (block type): %u", block_header.id);
			VERBOSE(CL_VERBOSE_ADVANCED,"\tPADDING: %u", block_header.pad);

			//force buffer realocation if is too small for record
			if(buffer != NULL && buffer_size < block_header.size){
				free(buffer);      
				buffer = NULL;
				buffer_size = 0;
			}

			if(buffer == NULL){
				buffer = (char *) malloc(block_header.size);
				VERBOSE(CL_VERBOSE_ADVANCED,"Buffer malloc");
				if(buffer == NULL){
					VERBOSE(CL_ERROR,"Can't alocate memory for record data");
					return 1;
				}
				buffer_size = block_header.size;
			}
			VERBOSE(CL_VERBOSE_ADVANCED,"RECORDS OFFSET in file: %lu",ftell(f));

			
			read_size = fread(buffer, block_header.size, 1,f);
			if (read_size != 1){
				VERBOSE(CL_ERROR,"Can't read record data: %s",file);
				fclose(f);
				return 1;
			}
			hex(buffer, block_header.size);
		
			int size=0;
			while (size < block_header.size){
				VERBOSE(CL_VERBOSE_ADVANCED,"OFFSET: %u - %p",size,buffer);
				record = (struct common_record_s *) buffer;

				if(record->type == CommonRecordType){
					VERBOSE(CL_VERBOSE_ADVANCED,"Parsed record from: '%s'",file);
					VERBOSE(CL_VERBOSE_ADVANCED,"\tTYPE: %hu", record->type);
					VERBOSE(CL_VERBOSE_ADVANCED,"\tSIZE: %hu", record->size);
					VERBOSE(CL_VERBOSE_ADVANCED,"\tFLAGS: %hhu", record->flags);
					VERBOSE(CL_VERBOSE_ADVANCED,"\tEXPORTER-REF: %hhu", record->exporter_ref);
					VERBOSE(CL_VERBOSE_ADVANCED,"\tEXT-MAP: %hu", record->ext_map);
					VERBOSE(CL_VERBOSE_ADVANCED,"\tMSEC-FIRST: %hu", record->msec_first);
					VERBOSE(CL_VERBOSE_ADVANCED,"\tMSEC-LAST: %hu", record->msec_last);
					VERBOSE(CL_VERBOSE_ADVANCED,"\tFIRST: %u", record->first);
					VERBOSE(CL_VERBOSE_ADVANCED,"\tLAST: %u", record->last);
					VERBOSE(CL_VERBOSE_ADVANCED,"\tFWD-STATUS: %hhu", record->fwd_status);
					VERBOSE(CL_VERBOSE_ADVANCED,"\tTCP-FLAGS: %hhu", record->tcp_flags);
					VERBOSE(CL_VERBOSE_ADVANCED,"\tPROTOCOL: %hhu", record->prot);
					VERBOSE(CL_VERBOSE_ADVANCED,"\tTOS: %hhu", record->tos);
					VERBOSE(CL_VERBOSE_ADVANCED,"\tSRC-PORT: %hu", record->srcport);
					VERBOSE(CL_VERBOSE_ADVANCED,"\tDST-PORT: %hu", record->dstport);

					hex(record->data,record->size - sizeof(struct common_record_s)-4);
					int data_offset = 0; // record->data = uint32_t
					int id;
					int j,eid;

					//check id -> most extensions should be on its index
					if(ext.map[record->ext_map].id == record->ext_map){
						id = record->ext_map;
						VERBOSE(CL_VERBOSE_ADVANCED,"\tMAP-INDEX-MATCH: %hu", record->ext_map);
					} else { //index does NOT match map id.. we need to find it
						for(j=0;j<ext.filled;j++){
							if(ext.map[j].id == record->ext_map){
								id = j;
								VERBOSE(CL_VERBOSE_ADVANCED,"\tMAP-INDEX-NOT-MATCH: %hu - %hu",j, record->ext_map);
							}
						}
					}
		
					ext_parse[0](record->data, &data_offset, record->flags, NULL); 
					ext_parse[1](record->data, &data_offset, record->flags, NULL); 
					ext_parse[2](record->data, &data_offset, record->flags, NULL); 
					
					for(eid=0;eid<ext.map[id].values_count;eid++){
						VERBOSE(CL_VERBOSE_ADVANCED,"\tMAP: %hu EXT-ID %hu",id ,ext.map[id].value[eid]);
						//ext_parse[ext.map[id].value[eid]](record->data, &data_offset, record->flags, NULL); 
					}

				} else if(record->type == ExtensionMapType){
					extension_map = (struct extension_map_s *) buffer;
					hex(buffer,record->size);
					if(ext.filled == ext.size){
						//double the size of extension map array
						if(realloc(ext.map,ext.size*2)==NULL){
							VERBOSE(CL_ERROR,"Can't realloc extension map array");
							fclose(f);
							return 1;
						}
						ext.size*=2;
					}
					ext.filled++;
					ext.map[ext.filled].value = (uint16_t *) malloc(extension_map->extension_size);
					ext.map[ext.filled].values_count = 0;
					ext.map[ext.filled].id = extension_map->map_id;


					VERBOSE(CL_VERBOSE_ADVANCED,"RECORD = EXTENSION MAP");
					VERBOSE(CL_VERBOSE_ADVANCED,"\tTYPE: %hu", extension_map->type);
					VERBOSE(CL_VERBOSE_ADVANCED,"\tSIZE: %hu", extension_map->size);
					VERBOSE(CL_VERBOSE_ADVANCED,"\tMAP ID: %hu", extension_map->map_id);
					VERBOSE(CL_VERBOSE_ADVANCED,"\tEXTENSION_SIZE: %hu", extension_map->extension_size);

					int eid=0;
					for(eid = 0; eid < extension_map->extension_size/2;eid++){ // extension id is 2 byte
						VERBOSE(CL_VERBOSE_ADVANCED,"\tEXTENSION_ID: %hu - %p", extension_map->ex_id[eid],&extension_map->ex_id[eid]);
						ext.map[ext.filled].value[eid] = extension_map->ex_id[eid]; 
						ext.map[ext.filled].values_count++;
					}

				} else if(record->type == ExporterType){
					VERBOSE(CL_VERBOSE_ADVANCED,"RECORD = EXPORTER TYPE");
					VERBOSE(CL_VERBOSE_ADVANCED,"\tTYPE: %hu", record->type);
					VERBOSE(CL_VERBOSE_ADVANCED,"\tSIZE: %hu", record->size);
				} else {
					VERBOSE(CL_VERBOSE_ADVANCED,"UNKNOWN RECORD TYPE");
					VERBOSE(CL_VERBOSE_ADVANCED,"\tTYPE: %hu", record->type);
					VERBOSE(CL_VERBOSE_ADVANCED,"\tSIZE: %hu", record->size);
				}

				size += record->size;
				if(size >= block_header.size){
					VERBOSE(CL_VERBOSE_ADVANCED,"------- SIZE: %u - %u",size,block_header.size);
					break;
				}
				buffer+= record->size;
			}
		}	

		//TERE IS SOMETHING REALY BAD HERE SIGSEV ON FREE OR FCLOSE
		//VERBOSE(CL_VERBOSE_ADVANCED,"---------===========-----------");
		//if(buffer!=NULL){
 		//	free(buffer);
		//}
		//fclose(f);
	} else {
		VERBOSE(CL_ERROR,"Can't open file: %s",file);
	}
	
	return 0;
}
