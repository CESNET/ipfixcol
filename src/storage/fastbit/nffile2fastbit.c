

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <commlbr.h>

#include "nffile.h"



int main(){
	char file[] = "/home/kramolis/nffile/nfcapd.201107200815";
	FILE *f;
	f = fopen(file,"r");
	char buffer[200];
	struct file_header_s header;
	int read_size;
	

	verbose = CL_VERBOSE_ADVANCED;

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
		
		


		fclose(f);
	} else {
		VERBOSE(CL_ERROR,"Can't open file: %s",file);
	}
	
	return 0;
}
