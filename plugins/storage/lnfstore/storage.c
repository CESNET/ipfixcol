#include <ipfixcol.h>
#include <ipfixcol/profiles.h>

#include "storage.h"
#include "translator.h" 
#include <string.h>
#include <libxml/xmlstring.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <stdbool.h>


static const char* msg_module = "lnfstore_storage";

typedef struct prec_s
{
	uint8_t *address;
	lnf_file_t *lfp;
}prec_t;



int prec_compare(const void* pkey, const void* pelem)
{
	const prec_t *key, *elem;
	key = pkey;
	elem = pelem;
	
	if(key->address > elem->address){
		return -1;
	} else if(key->address == elem->address){
		return 0;
	}
	return 1;
}

int nstrlen(char** strings, int n)
{	
	int sum = 0;
	for(int x = 0; x< n; x++){
		if(strings[x] == NULL)
			continue;
		sum+=strlen(strings[x]);
	}
	return sum;
}


char* mkpath_string(struct lnfstore_conf *conf, const char* suffix)
{
	char* path_strings[5] = {
		(char*)conf->storage_path, 
		(char*)suffix,
		conf->t_vars->dir, (char*)conf->prefix, conf->t_vars->suffix
	};
	
	char* path = malloc(sizeof(char)*(nstrlen(path_strings, 5)+1));
	if(path == NULL){
		return NULL;
	}

	path[0] = 0; //*< Clear path string

	for(int i = 0; i < 5; i++){ 
		if(path_strings[i] == NULL)
			continue;
		strcat(path, path_strings[i]);
	}
	return path;
}

void mktime_window(time_t hint, struct lnfstore_conf *conf)
{
	struct time_vars* vars = conf->t_vars;
    char buffer[25] = "";
    
	free(vars->dir);
	free(vars->suffix);
	
    if(conf->align){
        vars->window_start = (hint / conf->time_window)* conf->time_window;
    }else{
		vars->window_start = hint;
    }
	
    strftime(buffer, 25, "/%Y/%m/%d/", gmtime(&vars->window_start));
    vars->dir = strdup(buffer);

	strftime(buffer, 25, (const char*)conf->suffix_mask, gmtime(&vars->window_start));
	vars->suffix = strdup(buffer);
} 

int mkdir_hierarchy(const char* path)
{
    struct stat s;
	
	char* pos = NULL;
	char* realp = NULL;
	unsigned subs_len = 0;
	while((pos = strchr(path + subs_len + 1, '/')) != NULL )
	{
		subs_len = pos - path;
		int status;
		bool failed = false;
		realp = strndup(path, subs_len);
try_again:
        status = stat(realp, &s);
        if(status == -1){
            if(ENOENT == errno){
                if(mkdir(realp, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)){
                    if(!failed){
                        failed = true;
                        goto try_again;
                    }
                    int err = errno;
                    MSG_ERROR(msg_module, "Failed to create directory: %s", realp);
                    errno = err;
                    perror(msg_module);
                    return 1;
                }
            }
        } else if(!S_ISDIR(s.st_mode)){
            MSG_ERROR(msg_module, "Failed to create directory, %s is file", realp);
            return 2;
        }
		free(realp);
    }
	return 0;
}	

void store_record(struct metadata* mdata, struct lnfstore_conf *conf)
{
	static uint8_t buffer[(uint16_t)-1];
	
	static lnf_rec_t *recp = NULL;
	static lnf_file_t *lfp = NULL; 
	stack_t *smap = conf->pst;

	if( conf->profiles && !mdata->channels ){
		//Record wont be stored, it does not belong to any channel and profiling is activated
		return;
	}

	if(recp == NULL) {
		lnf_rec_init(&recp);
	} else { 
		lnf_rec_clear(recp);
	}

    uint16_t offset, length;
    offset = 0;
    struct ipfix_template *templ = mdata->record.templ;
    uint8_t *data_record = (uint8_t*) mdata->record.record;

    /* get all fields */
    for (uint16_t count = 0, index = 0; count < templ->field_count; ++count, ++index) {
		
		struct ipfix_lnf_map *item, key;
        
        /* Get Enterprise number and ID */
        key.ie = templ->fields[index].ie.id;
        length = templ->fields[index].ie.length;
        key.en = 0;

        if (key.ie & 0x8000) {
			key.ie &= 0x7fff;
            key.en = templ->fields[++index].enterprise_number;
        }

		item = bsearch(&key, tr_table, MAX_TABLE , sizeof(struct ipfix_lnf_map), ipfix_lnf_map_compare);
		
		int problem = 1;
		if(item != NULL){
			problem = item->func(data_record, &offset, &length, buffer, item);	
			lnf_rec_fset(recp, item->lnf_id, buffer);
		}
		
		if(problem){
			length = real_length(data_record, &offset, length);
		}

        offset += length;
    } //end of element processing

	//!< Decide whether close close files and create new time window
    time_t now = time(NULL);
    if(difftime(now, conf->t_vars->window_start) > conf->time_window){

		mktime_window(now, conf);
		if( conf->profiles ){
			prec_t* item = NULL;
			//Close all old files
			for(unsigned x = 0; x < smap->top; x += al4B(sizeof(prec_t))){
				item = (prec_t*)smap->data + x; 
				if(item->lfp == NULL)
					continue;
				lnf_close(item->lfp);
				item->lfp = NULL;
			}
		} else {
			lnf_close(lfp);
			lfp = NULL;
		}
    }
    
    if( conf->profiles ){
		//On stack allocation of bit array
		int ba_size = smap->top/(8*sizeof(int))+1;
		int ba[ba_size];
		memset(&ba[0], 0, ba_size*sizeof(int));
		int status = 0;
		
		prec_t *item = NULL;

		for( int i = 0; mdata->channels[i] != 0; i++ ){
			
			void* rec_prof = channel_get_profile(mdata->channels[i]);

			item = bsearch(&rec_prof, smap->data, smap->top/al4B(sizeof(prec_t)),
					al4B(sizeof(prec_t)), prec_compare);
			
			if( item == NULL ){
				//Profile is not in configuration
				prec_t entry;
				entry.address = rec_prof;
				entry.lfp = NULL;
				
				const char* prpath = profile_get_path(rec_prof);
				char* path = mkpath_string(conf, prpath);
				mkdir_hierarchy(path);
				status = lnf_open(&entry.lfp, path, LNF_WRITE | (conf->compress? LNF_COMP : 0), (char*)conf->ident);

				stack_push(smap, &entry, sizeof(prec_t));
				//Add resilience
				qsort(smap->data, smap->top/al4B(sizeof(prec_t)), 
						al4B(sizeof(prec_t)), prec_compare);
				
				free(path);
			} else if( item->lfp == NULL ){
				//Profile file is closed
				const char* prpath = profile_get_path(rec_prof);
				char* path = mkpath_string(conf, prpath);
				mkdir_hierarchy(path);
				status = lnf_open(&item->lfp, path, LNF_WRITE | (conf->compress? LNF_COMP : 0), (char*)conf->ident);
				//Get prof name and open aprop file
				free(path);
			}
		}
		void* profile = NULL;
		int i = 0;
		for(profile = channel_get_profile(mdata->channels[i=0]);
				mdata->channels[i] != 0;
				profile = channel_get_profile(mdata->channels[i++]) ){

			item = bsearch(&profile, smap->data, smap->top/al4B(sizeof(prec_t)),
					al4B(sizeof(prec_t)), prec_compare);
				
			int index = (((base_t*)item) - smap->data)/al4B(sizeof(prec_t));
			if( !GETb(ba, index) ){ //And profile is not shadow
				status = lnf_write(item->lfp, recp);
				SETb(ba, index, 1);
			}
		}
	} else {
		if( lfp == NULL ){
			int status;
			char* path = mkpath_string(conf, NULL);
			status = mkdir_hierarchy(path);
			status |= lnf_open(&lfp, path, LNF_WRITE | (conf->compress ? LNF_COMP : 0), (char*)conf->ident);
			if(status != LNF_OK) {
				MSG_ERROR(msg_module, "Failed to open file! ...at path: %s \n", path);
			}
			free(path);
		}
		lnf_write(lfp, recp);
	}
	return;
}


