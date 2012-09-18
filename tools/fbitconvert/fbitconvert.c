

#if HAVE_CONFIG_H
#include <config.h>
#endif
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <stdint.h>
#include <dlfcn.h>
#include <time.h>
#include <signal.h>
#include <ipfixcol.h>


#include "nffile.h"
#include "ext_parse.h"
#include "ext_fill.h"


#define PLUGIN_PATH "/usr/local/share/ipfixcol/plugins/ipfixcol-fastbit-output.so"

#define ARGUMENTS "hbi:w:v:p:P:r:V"

char *msg_str = "fbitconvert";

volatile int stop = 0;
static int ctrl_c = 0;
void signal_handler(int signal_id){
	if(ctrl_c){
		MSG_WARNING(msg_str, "Forced quit");
		exit(1);
	} else {
		MSG_WARNING(msg_str, "I'll end as soon as possible");
		stop = 1;
		ctrl_c++;
	}
	signal(SIGINT,&signal_handler);
}



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
	fprintf(stderr,"\n");
}

struct extension{
	uint16_t *value; //map array
	int values_count;
	int id;
	int tmp6_index;  //index of ipv6 template for this extension map
	int tmp4_index;  //index of ipv4 template for this extension map
};

struct extensions{
	unsigned int filled;
	unsigned int size;
	struct extension *map;
};

struct storage{
	int x;
};

void (*ext_parse[26]) (uint32_t *data, int *offset, uint8_t flags, struct ipfix_data_set *data_set) = {
		ext0_parse,
		ext1_parse,
		ext2_parse,
		ext3_parse,
		ext4_parse,
		ext5_parse,
		ext6_parse,
		ext7_parse,
		ext8_parse,
		ext9_parse,
		ext10_parse,
		ext11_parse,
		ext12_parse,
		ext13_parse,
		ext14_parse,
		ext15_parse,
		ext16_parse,
		ext17_parse,
		ext18_parse,
		ext19_parse,
		ext20_parse,
		ext21_parse,
		ext22_parse,
		ext23_parse,
		ext24_parse,
		ext25_parse
};

void (*ext_fill_tm[26]) (uint8_t flags, struct ipfix_template * template) = {
		ext0_fill_tm,
		ext1_fill_tm,
		ext2_fill_tm,
		ext3_fill_tm,
		ext4_fill_tm,
		ext5_fill_tm,
		ext6_fill_tm,
		ext7_fill_tm,
		ext8_fill_tm,
		ext9_fill_tm,
		ext10_fill_tm,
		ext11_fill_tm,
		ext12_fill_tm,
		ext13_fill_tm,
		ext14_fill_tm,
		ext15_fill_tm,
		ext16_fill_tm,
		ext17_fill_tm,
		ext18_fill_tm,
		ext19_fill_tm,
		ext20_fill_tm,
		ext21_fill_tm,
		ext22_fill_tm,
		ext23_fill_tm,
		ext24_fill_tm,
		ext25_fill_tm
};

#define HEADER_ELEMENTS 8
int header_elements[][2] = {
		//id,size
		{89,1},  //fwd_status
		{152,8}, //flowEndSysUpTime MILLISECONDS !
		{153,8}, //flowStartSysUpTime MILLISECONDS !
		{6,1},  //tcpControlBits flags
		{4,1},  //protocolIdentifier
		{5,1},  //ipClassOfService
		{7,2},  //sourceTransportPort
		{11,2} //destinationTransportPort
};

#define ALLOC_FIELDS_SIZE 60

void fill_basic_data(struct ipfix_data_set *data_set, struct common_record_s *record){

	data_set->records[data_set->header.length] = record->fwd_status;
	data_set->header.length += 1;
	*((uint64_t *) &(data_set->records[data_set->header.length])) = htobe64((uint64_t)record->first*1000+record->msec_first); //sec 2 msec
	data_set->header.length += 8;
	*((uint64_t *) &(data_set->records[data_set->header.length])) = htobe64((uint64_t)record->last*1000+record->msec_last); //sec 2 msec
	data_set->header.length += 8;
	data_set->records[data_set->header.length] =record->tcp_flags;
	data_set->header.length += 1;
	data_set->records[data_set->header.length] =record->prot;
	data_set->header.length += 1;
	data_set->records[data_set->header.length] =record->tos;
	data_set->header.length += 1;
	*((uint16_t *) &(data_set->records[data_set->header.length])) = htons(record->srcport);
	data_set->header.length += 2;
	*((uint16_t *) &(data_set->records[data_set->header.length])) = htons(record->dstport);
	data_set->header.length += 2;

}

int fbt = 0;
int s =0;
int iim =0;
void fill_basic_template(uint8_t flags, struct ipfix_template **template){
	static int template_id_counter = 1;

	(*template) = (struct ipfix_template *) malloc(sizeof(struct ipfix_template) + \
			ALLOC_FIELDS_SIZE * sizeof(template_ie));
	fbt++;
	if(*template == NULL){
		MSG_ERROR(msg_str, "Malloc faild to get space for ipfix template");
	}

	(*template)->template_type = TM_TEMPLATE;
	(*template)->last_transmission = time(NULL);
	(*template)->last_message = 0;
	(*template)->template_id = template_id_counter;
	template_id_counter++;

	(*template)->field_count = 0;
	(*template)->scope_field_count = 0;
	(*template)->template_length = 0;
	(*template)->data_length = 0;

	// add header elements into template
	int i;
	for(i=0;i<HEADER_ELEMENTS;i++){	
		(*template)->fields[(*template)->field_count].ie.id = header_elements[i][0];
		(*template)->fields[(*template)->field_count].ie.length = header_elements[i][1];
		(*template)->field_count++;
		(*template)->data_length += header_elements[i][1];  
		(*template)->template_length += 4;
	}

	//add mandatory extensions elements 
	//Extension 1
	ext_fill_tm[1] (flags, *template);
	//Extension 2
	ext_fill_tm[2] (flags, *template);
	//Extension 3
	ext_fill_tm[3] (flags, *template);
}
void init_ipfix_msg(struct ipfix_message *ipfix_msg){
	ipfix_msg->pkt_header = (struct ipfix_header *) malloc(sizeof(struct ipfix_header));
	iim++;
	ipfix_msg->pkt_header->version = 0x000a;
	ipfix_msg->pkt_header->length = 16; //header size 
	ipfix_msg->pkt_header->export_time = 0;
	ipfix_msg->pkt_header->sequence_number = 0;
	ipfix_msg->pkt_header->observation_domain_id = 0; 

	ipfix_msg->input_info = NULL;
	memset(ipfix_msg->templ_set,0,sizeof(struct ipfix_template_set *) *1024);
	memset(ipfix_msg->opt_templ_set,0,sizeof(struct ipfix_optional_template_set *) *1024);
	memset(ipfix_msg->data_couple,0,sizeof(struct data_template_couple) *1023);
}

void clean_ipfix_msg(struct ipfix_message *ipfix_msg){
	int i;
	free(ipfix_msg->pkt_header);
	ipfix_msg->pkt_header = NULL;
	for(i=0;i<1023;i++){
		if(ipfix_msg->data_couple[i].data_set == NULL){
			break;
		} else {
			free(ipfix_msg->data_couple[i].data_set);
			ipfix_msg->data_couple[i].data_set = NULL;
			ipfix_msg->data_couple[i].data_template = NULL;
		}
	}
	for(i=0;i<1024;i++){
		if(ipfix_msg->templ_set[i] == NULL){
			break;
		} else {
			free(ipfix_msg->templ_set[i]);
			ipfix_msg->templ_set[i] = NULL;
		}
	}
}

void change_endianity(struct ipfix_message *ipfix_msg){
	ipfix_msg->pkt_header->version = htons(ipfix_msg->pkt_header->version);
	ipfix_msg->pkt_header->length = htons(ipfix_msg->pkt_header->length); 
	ipfix_msg->pkt_header->export_time = htonl(ipfix_msg->pkt_header->export_time);
	ipfix_msg->pkt_header->sequence_number = htonl(ipfix_msg->pkt_header->sequence_number);
	ipfix_msg->pkt_header->observation_domain_id = htonl(ipfix_msg->pkt_header->observation_domain_id); 
}

void add_data_set(struct ipfix_message *ipfix_msg, struct ipfix_data_set *data_set, struct ipfix_template *template){
	int i;
	for(i=0;i<1023;i++){
		if(ipfix_msg->data_couple[i].data_set == NULL){
			ipfix_msg->pkt_header->length += data_set->header.length;
			data_set->header.length = htons(data_set->header.length);
			ipfix_msg->data_couple[i].data_set = data_set;
			ipfix_msg->data_couple[i].data_template = template;
			break;
		}
	}
}

void add_template(struct ipfix_message *ipfix_msg, struct ipfix_template * template){
	int i;
	for(i=0;i<1024;i++){
		if(ipfix_msg->templ_set[i] == NULL){
			ipfix_msg->templ_set[i] = (struct ipfix_template_set *) \
					malloc(sizeof(struct ipfix_options_template_set)+template->data_length);

			ipfix_msg->templ_set[i]->header.flowset_id = 2;
			ipfix_msg->templ_set[i]->header.length = 8 + template->template_length;
			ipfix_msg->templ_set[i]->first_record.template_id = template->template_id;
			ipfix_msg->templ_set[i]->first_record.count = template->field_count;
			memcpy ( ipfix_msg->templ_set[i]->first_record.fields, template->fields, template->data_length);
			ipfix_msg->pkt_header->length += ipfix_msg->templ_set[i]->header.length;
			break;
		}
	}
}

void clean_tmp_manager(struct ipfix_template_mgr *manager){
	int i;
	for(i = 0; i <= manager->counter; i++ ){
		if(manager->templates[i]!=NULL){
			free(manager->templates[i]);
			manager->templates[i]=NULL;
		}
	}
	manager->counter = 0;
}

int
process_ext_record(struct common_record_s * record, struct extensions *ext,
		struct ipfix_template_mgr *template_mgr, void *config,
		int (*plugin_store) (void *, const struct ipfix_message *, const struct ipfix_template_mgr *)){

	struct ipfix_message ipfix_msg;
	int data_offset = 0;
	int id,eid;
	unsigned j;

	//check id -> most extensions should be on its index
	if(ext->map[record->ext_map].id == record->ext_map){
		id = record->ext_map;
	} else { //index does NOT match map id.. we have to find it
		for(j=0;j<ext->filled;j++){
			if(ext->map[j].id == record->ext_map){
				id = j;
			}
		}
	}

	struct ipfix_template *tmp= NULL;
	struct ipfix_data_set *set= NULL;
	if(TestFlag(record->flags, FLAG_IPV6_ADDR)){
		tmp = template_mgr->templates[ext->map[id].tmp6_index];
	} else {
		tmp = template_mgr->templates[ext->map[id].tmp4_index];
	}
	s++;
	set = (struct ipfix_data_set *) malloc(sizeof(struct ipfix_data_set)+ tmp->data_length);
	if(set == NULL){
		MSG_ERROR(msg_str, "Malloc failed getting memory for data set");
	}
	memset(set,0,sizeof(struct ipfix_data_set)+ tmp->data_length);
	set->header.flowset_id = htons(tmp->template_id);


	fill_basic_data(set,record);
	ext_parse[1](record->data, &data_offset, record->flags, set);
	ext_parse[2](record->data, &data_offset, record->flags, set);
	ext_parse[3](record->data, &data_offset, record->flags, set);

	for(eid=0;eid<ext->map[id].values_count;eid++){
		ext_parse[ext->map[id].value[eid]](record->data, &data_offset, record->flags, set);
	}

	set->header.length += sizeof(struct ipfix_set_header);

	init_ipfix_msg(&ipfix_msg);
	ipfix_msg.pkt_header->length += set->header.length;

	//fill IPFIX message and pass it to plug-in

	add_data_set(&ipfix_msg, set, tmp);
	change_endianity(&ipfix_msg);
	plugin_store (config, &ipfix_msg, template_mgr);
	clean_ipfix_msg(&ipfix_msg);
	return 0;
}

int
process_ext_map(struct extension_map_s * extension_map, struct extensions *ext,
		struct ipfix_template_mgr *template_mgr, void *config,
		int (*plugin_store) (void *, const struct ipfix_message *, const struct ipfix_template_mgr *)){

	struct ipfix_message ipfix_msg;
	struct ipfix_template *template1;
	struct ipfix_template *template2;

	ext->filled++;
	if(ext->filled == ext->size){
		//double the size of extension map array
		ext->map=(struct extension *) realloc(ext->map,(ext->size * 2)*sizeof(struct extension));
		if(ext->map==NULL){
			MSG_ERROR(msg_str, "Can't reallocation extension map array");
			return -1;
		}
		ext->size*=2;
	}

	ext->map[ext->filled].value = (uint16_t *) malloc(extension_map->extension_size);
	ext->map[ext->filled].values_count = 0;
	ext->map[ext->filled].id = extension_map->map_id;


	if(template_mgr->counter+2 >= template_mgr->max_length){
		//double the size of extension map array
		if((template_mgr->templates = (struct ipfix_template **)realloc(template_mgr->templates,sizeof(struct ipfix_template *)*(template_mgr->max_length*2)))==NULL){
			MSG_ERROR(msg_str, "Can't reallocation extension map array");
			return -1;
		}
		template_mgr->max_length*=2;
	}

	template_mgr->counter++;
	//template for this record with ipv4
	fill_basic_template(0, &(template_mgr->templates[template_mgr->counter]));
	template1 = template_mgr->templates[template_mgr->counter];
	ext->map[ext->filled].tmp4_index = template_mgr->counter;

	template_mgr->counter++;
	//template for this record with ipv6
	fill_basic_template(1, &(template_mgr->templates[template_mgr->counter]));
	template2 = template_mgr->templates[template_mgr->counter];
	ext->map[ext->filled].id = extension_map->map_id;
	ext->map[ext->filled].tmp6_index = template_mgr->counter;

	int eid=0;
	for(eid = 0; eid < extension_map->size/2;eid++){ // extension id is 2 byte
		if(extension_map->ex_id[eid] == 0){
			break;
		}
		ext->map[ext->filled].value[eid] = extension_map->ex_id[eid];
		ext->map[ext->filled].values_count++;
		ext_fill_tm[extension_map->ex_id[eid]] (0, template1);
		ext_fill_tm[extension_map->ex_id[eid]] (1, template2);
	}

	//create IPFIX message and pass it to storage plug-in
	init_ipfix_msg(&ipfix_msg);
	add_template(&ipfix_msg,template1);
	add_template(&ipfix_msg,template2);
	change_endianity(&ipfix_msg);
	plugin_store (config, &ipfix_msg, template_mgr);
	clean_ipfix_msg(&ipfix_msg);
	return 0;
}

int usage(){
	printf("Usage: %s -i input_file -w output_dir [-p prefix] [-P path] [-r limit] [-v level] [-hVb]\n", PACKAGE);
	printf(" -i input_file	path to nfdump file for conversion\n");
	printf(" -w output_dir	output directory for fastbit files\n");
	printf(" -b		build indexes\n");
	printf(" -p prefix	output files prefix\n");
	printf(" -P path	path to fastbit plug-in\n");
	printf(" -r limit	record limit for fastbit files\n");
	printf(" -h 		prints this help\n");
	printf(" -v level 	set verbose level\n");
	printf(" -V		show version\n");
	return 0;
}

int main(int argc, char *argv[]){
	FILE *f;
	unsigned int i;
	char *buffer = NULL;
	unsigned int buffer_size = 0;
	struct file_header_s header;
	struct stat_record_s stats;
	struct data_block_header_s block_header;
	struct common_record_s *record;
	struct extensions ext = {0,2,NULL};
	int read_size;
	void *config;
	void *dlhandle;
	int (*plugin_init) (char *, void **);
	int (*plugin_store) (void *, const struct ipfix_message *, const struct ipfix_template_mgr *);
	int (*plugin_close) (void **);
	char *error;
	struct ipfix_template_mgr template_mgr;
	unsigned int size = 0;

	//param handlers
	char *input_file = 0;
	char *output_dir = 0;
	char indexes[4] = "no";
	char def_prefix[] = "";
	char *prefix = def_prefix;
	char def_plugin[] = PLUGIN_PATH;
	char *plugin = def_plugin;
	char def_record_limit[] = "8000000";
	char *record_limit = def_record_limit;


	char c;
	while((c = getopt(argc, argv, ARGUMENTS)) != -1) {
		switch (c) {

		case 'i':
			input_file = optarg;
			break;

		case 'w':
			output_dir = optarg;
			break;
		case 'p':
			prefix = optarg;
			break;
		case 'P':
			if(optarg[0]=='/'){
				plugin = (char *) malloc(strlen(optarg)+1);
				strcpy(plugin,optarg);
			}else{
				plugin = (char *) malloc(strlen(optarg)+3);
				sprintf(plugin,"./%s",optarg);
			}
			break;
		case 'r':
			record_limit = optarg;
			break;
		case 'b':
			strcpy(indexes,"yes");
			break;
		case 'h':
			usage();
			return 1;
			break;
		case 'v':
			verbose = atoi(optarg);
			break;
		case 'V':
			printf("%s - version %s\n", PACKAGE, VERSION);
			return 1;
			break;
		default:
			MSG_ERROR(msg_str, "unknown option!\n\n");
			usage();
			return 1;
			break;
		}
	}

	if(input_file == 0){
		MSG_ERROR(msg_str, "no input file specified (option '-i')");
		return 1;
	}
	if(output_dir == 0){
		MSG_ERROR(msg_str, "no output directory specified (option '-w')");
		return 1;
	}

	signal(SIGINT,&signal_handler);

	dlhandle = dlopen (plugin, RTLD_LAZY);
	if (!dlhandle) {
		fputs (dlerror(), stderr);
		exit(1);
	}

	plugin_init = dlsym(dlhandle, "storage_init");
	if ((error = dlerror()) != NULL)  {
		fputs(error, stderr);
		exit(1);
	}

	plugin_store = dlsym(dlhandle, "store_packet");
	if ((error = dlerror()) != NULL)  {
		fputs(error, stderr);
		exit(1);
	}

	plugin_close = dlsym(dlhandle, "storage_close");
	if ((error = dlerror()) != NULL)  {
		fputs(error, stderr);
		exit(1);
	}

	//plugin configuration xml
	char params_template[] =
			"<?xml version=\"1.0\"?> \
	                 <fileWriter xmlns=\"urn:ietf:params:xml:ns:yang:ietf-ipfix-psamp\"> \
				<fileFormat>fastbit</fileFormat> \
				<path>%s</path> \
				<dumpInterval> \
					<timeWindow>0</timeWindow> \
					<timeAlignment>yes</timeAlignment> \
					<recordLimit>%s</recordLimit> \
				</dumpInterval> \
				<namingStrategy> \
					<type>incremental</type> \
					<prefix>%s</prefix> \
				</namingStrategy> \
				<onTheFlightIndexes>%s</onTheFlightIndexes> \
			</fileWriter>";

	char *params;
	params = (char *) malloc(strlen(params_template)+strlen(record_limit)+strlen(output_dir)+strlen(prefix)+strlen(indexes));

	sprintf(params, params_template, output_dir,record_limit,prefix,indexes);


	plugin_init(params, &config);

	//inital space for extension map
	ext.map = (struct extension *) calloc(ext.size,sizeof(struct extension));
	if(ext.map == NULL){
		MSG_ERROR(msg_str, "Can't read allocate memory for extension map");
		return 1;
	}

	template_mgr.templates = (struct ipfix_template **) calloc(ext.size,sizeof(struct ipfix_template *));
	if(ext.map == NULL){
		MSG_ERROR(msg_str, "Can't read allocate memory for templates");
		return 1;
	}
	memset(template_mgr.templates,0,ext.size*sizeof(struct ipfix_template *));
	template_mgr.max_length = ext.size;
	template_mgr.counter = 0;

	f = fopen(input_file,"r");

	if(f != NULL){
		//read header of nffile
		read_size = fread(&header, sizeof(struct file_header_s), 1,f);
		if (read_size != 1){
			MSG_ERROR(msg_str, "Can't read file header: %s",input_file);
			fclose(f);
			return 1;
		}

		read_size = fread(&stats, sizeof(struct stat_record_s), 1,f);
		if (read_size != 1){
			MSG_ERROR(msg_str, "Can't read file statistics: %s",input_file);
			fclose(f);
			return 1;
		}

		//TODO MSG_NOTICE(msg_str, "\tSEQUENCE-FAIULURE: %u", stats.sequence_failure);

		//template for this record with ipv4
		fill_basic_template(0, &(template_mgr.templates[template_mgr.counter]));
		ext.map[ext.filled].tmp4_index = template_mgr.counter;

		template_mgr.counter++;
		//template for this record with ipv6
		fill_basic_template(1, &(template_mgr.templates[template_mgr.counter]));
		ext.map[ext.filled].id = 0;
		ext.map[ext.filled].tmp6_index = template_mgr.counter;

		char * buffer_start = NULL;

		for(i = 0; i < header.NumBlocks && !stop ; i++){
			read_size = fread(&block_header, sizeof(struct data_block_header_s), 1,f);
			if (read_size != 1){
				MSG_ERROR(msg_str, "Can't read block header: %s",input_file);
				fclose(f);
				return 1;
			}

			//force buffer reallocation if is too small for record
			if(buffer_start != NULL && buffer_size < block_header.size){
				free(buffer_start);      
				buffer = NULL;
				buffer_start = NULL;
				buffer_size = 0;
			}

			if(buffer_start == NULL){
				buffer_start = (char *) malloc(block_header.size);
				if(buffer_start == NULL){
					MSG_ERROR(msg_str, "Can't allocate memory for record data");
					return 1;
				}
				buffer_size = block_header.size;
				buffer = buffer_start;
			}

			buffer = buffer_start;
			read_size = fread(buffer, block_header.size, 1,f);
			if (read_size != 1){
				perror("file read:");
				MSG_ERROR(msg_str, "Can't read record data: %s",input_file);
				fclose(f);
				return 1;
			}

			size = 0;
			//read block
			while (size < block_header.size && !stop){
				record = (struct common_record_s *) buffer;

				if(record->type == CommonRecordType){
					stop = process_ext_record(record, &ext, &template_mgr, config, plugin_store);

				} else if(record->type == ExtensionMapType){
					stop = process_ext_map((struct extension_map_s *) buffer, &ext, &template_mgr,
							config, plugin_store);

				} else if(record->type == ExporterType){
					MSG_DEBUG(msg_str, "RECORD = EXPORTER TYPE");
				} else {
					MSG_DEBUG(msg_str, "UNKNOWN RECORD TYPE");
				}

				size += record->size;
				if(size >= block_header.size){
					break;
				}
				buffer+= record->size;
			}
		}

		dlclose(dlhandle);	
		if(buffer_start!=NULL){
			free(buffer_start);
		}

		for(i=0;i<=ext.filled;i++){
			free(ext.map[i].value);
		}
		free(ext.map);
		free(params);
		clean_tmp_manager(&template_mgr);
		free(template_mgr.templates);
		fclose(f);
		plugin_close(&config);
		free(plugin);
	} else {
		MSG_ERROR(msg_str, "Can't open file: %s",input_file);
	}
	return 0;
}
