#ifndef LS_LNFSTORE_H 
#define LS_LNFSTORE_H

#include <libxml/xmlstring.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

#define readui8(_ptr_)(*((uint8_t*)(_ptr_)))
#define readui16(_ptr_)(*((uint16_t*)(_ptr_)))
#define readui32(_ptr_)(*((uint32_t*)(_ptr_)))
#define readui64(_ptr_)(*((uint64_t*)(_ptr_)))

struct time_vars
{	
	char* dir;
	char* suffix;
	time_t window_start;	
};

struct lnfstore_conf
{
	xmlChar* prefix;
	xmlChar* suffix_mask;
	xmlChar* storage_path;
	xmlChar* ident;
	struct time_vars* t_vars;
	unsigned long time_window;
	bool align;
	bool compress;
	bool profiles;	
};

#endif //LS_LNFSTORE_H
