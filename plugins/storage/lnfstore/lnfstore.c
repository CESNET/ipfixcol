#include "lnfstore.h"
#include "storage.h"
#include <ipfixcol.h>
	IPFIXCOL_API_VERSION


#include <libxml/parser.h>
#include <libxml/xmlmemory.h>
#include <stdlib.h>
#include <string.h>

#define xmlStrcmpBool(_xmlStr_, boolVal)( \
	xmlStrcmp(_xmlStr_, (const xmlChar*)((boolVal)?"yes":"no")) && \
	xmlStrcmp(_xmlStr_, (const xmlChar*)((boolVal)?"true":"false")) && \
	xmlStrcmp(_xmlStr_, (const xmlChar*)((boolVal)?"1":"0")) )

static const char* msg_module= "lnfstore_interface";

const struct lnfstore_conf def_conf= 
{
	NULL,
	(xmlChar*)"nfdump.",
	(xmlChar*)"%F%R",
	(xmlChar*)".\\",
	(xmlChar*)"",
	NULL,
	300,
	true,
	false,
	false,
};

void process_startup_xml(struct lnfstore_conf *conf, char *params)
{
        xmlDocPtr doc;
        xmlNodePtr cur, cur_sub;
		
        doc = xmlReadMemory(params, strlen(params), "nobase.xml", NULL, 0);

        cur = xmlDocGetRootElement(doc);

        if(xmlStrcmp(cur->name, (const xmlChar*) "fileWriter")){
            goto err_xml;
        }

        cur = cur->xmlChildrenNode;
        while(cur != NULL){
			xmlChar* nodeStr = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);

            if(!xmlStrcmp(cur->name, (const xmlChar*) "fileFormat")){
                if(xmlStrcmp(nodeStr, (const xmlChar*) "lnfstore") != 0){
					xmlFree(nodeStr);
                    goto err_xml_val;
                }
            }else if(!xmlStrcmp(cur->name, (const xmlChar*) "profiles")){
				conf->profiles = !xmlStrcmpBool(nodeStr, true);
				xmlFree(nodeStr);

            }else if(!xmlStrcmp(cur->name, (const xmlChar*) "compress")){
				conf->compress = !xmlStrcmpBool(nodeStr, true);
				xmlFree(nodeStr);

            }else if(!xmlStrcmp(cur->name, (const xmlChar*) "storagePath")){
				conf->storage_path = nodeStr;

            }else if(!xmlStrcmp(cur->name, (const xmlChar*) "prefix")){
				conf->prefix = nodeStr;

            }else if(!xmlStrcmp(cur->name, (const xmlChar*) "suffixMask")){
				conf->suffix_mask = nodeStr;

            }else if(!xmlStrcmp(cur->name, (const xmlChar*) "identificatorField")){
				conf->ident = nodeStr;

            }else if(!xmlStrcmp(cur->name, (const xmlChar*) "dumpInterval")){
				
				char* endptr;

                cur_sub = cur->xmlChildrenNode;
                while(cur_sub != NULL){
					xmlChar* nodeStr = xmlNodeListGetString(doc, cur_sub->xmlChildrenNode, 1);
                    if(!xmlStrcmp(cur_sub->name, (const xmlChar*) "timeWindow")){
						long window = strtol((const char*)nodeStr, &endptr, 10);
						if(*endptr != 0){
							//conversion failed
							conf->time_window = def_conf.time_window;
						}
						conf->time_window = window;

					}else if(!xmlStrcmp(cur_sub->name, (const xmlChar*) "align")){
							conf->align = !xmlStrcmpBool(nodeStr, true);
					}
					xmlFree(nodeStr);
					cur_sub = cur_sub->next;
                }
			}
			cur = cur->next;
		}
err_xml:
err_xml_val:
	xmlFree(doc);
}


/* plugin inicialization */
int storage_init (char *params, void **config)
{	

		/* Create configuration */
        struct lnfstore_conf *conf = malloc(sizeof(struct lnfstore_conf));
		struct time_vars *t_vars = malloc(sizeof(struct time_vars));	

		conf->t_vars = t_vars;
		t_vars->dir = NULL;
		t_vars->suffix = NULL;
		t_vars->window_start = 0;
		conf->pst = NULL;	
		/* Process params */
		process_startup_xml(conf, params);
		
		
		//! If profiles are to be used, alloc ads for prof. storage
		if(conf->profiles){
			conf->pst = stack_init(64*sizeof(base_t));
		}
		/* Save configuration */
		*config = conf;

	
	MSG_DEBUG(msg_module, "initialized");
	return 0;
}


int store_packet (void *config, const struct ipfix_message *ipfix_msg,
	const struct ipfix_template_mgr *template_mgr)
{
	(void) template_mgr;
	struct lnfstore_conf *conf = (struct lnfstore_conf *) config;
	
	for(int i = 0; i < ipfix_msg->data_records_count; i++)
	{
		store_record(&(ipfix_msg->metadata[i]), conf);
	}
	
	return 0;
}

int store_now (const void *config)
{
	(void) config;
	return 0;
}

int storage_close (void **config)
{
	MSG_DEBUG(msg_module, "CLOSING");
	struct lnfstore_conf *conf = (struct lnfstore_conf *) *config;
	
	/* Destroy configuration */
	xmlFree(conf->prefix);
	xmlFree(conf->suffix_mask);
	xmlFree(conf->ident);
	xmlFree(conf->storage_path);

	stack_del(conf->pst);
	*config = NULL;

	return 0;
}

//ADS general stack implementation

stack_t* stack_init(size_t size)
{
	stack_t* st = NULL;
	st = malloc(sizeof(stack_t));
	if(st != NULL){
		void* tmp = malloc(al4B(size)*sizeof(base_t));
		if(tmp != NULL){
			st->size = al4B(size); //!< number of dwords
			st->top = 0;
			st->data = tmp;
			return st;
		}
		free(st);
		free(tmp);
	}
	return NULL;
}

void stack_del(stack_t* st)
{
	free(st->data);
	free(st);
}


int stack_resize(stack_t* st, int size)
{
	unsigned ne = al4B(size);
	if(st->top < ne){
		void* tmp = realloc(st->data, ne*sizeof(st->data[0]));
		if(tmp != NULL){
			st->data = tmp;
			st->size = ne;
			return 0;
		}
	}
	return 1;
}

int stack_push(stack_t* st, void* data, int length)
{
	unsigned ne = al4B(length);
	if(st->top >= ne && st->top + ne <= st->size){
		memcpy(st->data+st->top, data, length);
		st->top += ne; 
		return 0;
	} else {
		while(ne + st->top > st->size){
			int x = stack_resize(st, st->size*2);
			if(x){
				fprintf(stderr, "Failed to allocate %u memory for stack_t\n",st->size*2);
				return 1;
			}
		}
		memcpy(st->data+st->top, data, length);
		st->top += ne; 
		return 0;
	}
	return 1;
}

int stack_pop(stack_t* st, int length)
{
	unsigned ne = al4B(length);
	if(st->top >= ne && st->top + ne <= st->size){
		st->top -= ne;
		return 0;
	}
	return 1;
}

int stack_top(stack_t* st, void* data, int length)
{	
	unsigned ne = al4B(length);
	if(st->top >= ne && st->top + ne <= st->size){
		memcpy(data, st->data+st->top-ne, length);
		return 0;
	}
	return 1;
}

bool stack_empty(stack_t* st)
{	
	return(st->top == 0);
}

int stack_size(stack_t* st)
{
	return(st->top*sizeof(base_t));
}
