#ifndef LS_LNFSTORE_H 
#define LS_LNFSTORE_H

#include <libxml/xmlstring.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#define readui8(_ptr_)(*((uint8_t*)(_ptr_)))
#define readui16(_ptr_)(*((uint16_t*)(_ptr_)))
#define readui32(_ptr_)(*((uint32_t*)(_ptr_)))
#define readui64(_ptr_)(*((uint64_t*)(_ptr_)))

typedef uint32_t base_t;
struct time_vars
{	
	char* dir;
	char* suffix;
	time_t window_start;	
};

typedef struct stack_s{
	unsigned size;
	unsigned top;
	base_t *data;
}stack_t;

struct lnfstore_conf
{
	stack_t* pst;
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

//! \define Rounds bytelen to smallest possible multiple of bytesize of boundary type
#define aligned(bytelen, boundary)\
	(bytelen/(boundary)+(bytelen%(boundary) > 0 ? 1 : 0))


#define al4B(bytelen)\
	(bytelen/sizeof(base_t)+(bytelen%sizeof(base_t) > 0 ? 1 : 0))


#define ELNb(d, i) (i/8*sizeof(d[0]))
#define bIDX(d, i) (i%8*sizeof(d[0]))
#define GETb(d, i) (d[ELNb(d,i)] & 1LL << bIDX(d,i))
#define SETb(d, i, m) (d[ELNb(d,i)] = ((d[ELNb(d,i)] & ~(1 << bIDX(d,i))) | (-m & (1 << bIDX(d,i)))) )


stack_t* stack_init(size_t size);
void stack_del(stack_t* st);
int stack_resize(stack_t* st, int size);
int stack_push(stack_t* st, void* data, int length);
int stack_pop(stack_t* st, int length);
int stack_top(stack_t* st, void* data, int length);
bool stack_empty(stack_t* st);
int stack_size(stack_t* st);

#endif //LS_LNFSTORE_H
