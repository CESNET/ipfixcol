/**
 * \file mac.c
 * \author Michal Kozubik <kozubik@cesnet.cz>
 * \brief fbitdump plugin for parsing/formating MAC address
 * 
 * Copyright (C) 2015 CESNET, z.s.p.o.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "plugin_header.h"

struct mult_conf {
	double multiplier;
	int decimals;
};

char *info()
{
	return \
"Takes 2 parameters: multiplier (can be float number) and decimals (optional, default = 0)\n"
"Printing: number is multiplied by multiplier and printed with given number of decimals\n"
"Filtering: number is divided by multiplier\n"
"Example: multiplier(1000, 3) - each number in column is multiplier by 1000 and printed with precision to 3 decimal places";
}

int init(const char *params, void **conf)
{
	struct mult_conf *config = calloc(1, sizeof(struct mult_conf));
	char tmp[20];
	char *delim = strchr(params, ',');
	
	/* No delimiter = 0 decimals */
	if (delim == NULL) {
		config->multiplier = atof(params);
	} else {
		/* Split parameters */
		long len = (long) delim - (long) params;
		memcpy(tmp, params, len);
		tmp[len] = ' ';
		
		/* Get informations */
		config->multiplier = atof(tmp);
		config->decimals = atoi(delim + 1);
	}
	 
	*conf = config;
	return 0;
}

void toString(const plugin_arg_t *arg, char buff[PLUGIN_BUFFER_SIZE], double mult, int decs)
{
	switch (arg->type) {
	case INT8: 
		snprintf(buff, PLUGIN_BUFFER_SIZE, "%.*f", decs, arg->val->int8 * mult);
		break;
	case INT16: 
		snprintf(buff, PLUGIN_BUFFER_SIZE, "%.*f", decs, arg->val->int16 * mult);
		break;
	case INT32: 
		snprintf(buff, PLUGIN_BUFFER_SIZE, "%.*f", decs, arg->val->int32 * mult);
		break;
	case INT64:
		snprintf(buff, PLUGIN_BUFFER_SIZE, "%.*f", decs, arg->val->int64 * mult);
		break;
	case UINT8: 
		snprintf(buff, PLUGIN_BUFFER_SIZE, "%.*f", decs, arg->val->uint8 * mult);
		break;
	case UINT16: 
		snprintf(buff, PLUGIN_BUFFER_SIZE, "%.*f", decs, arg->val->uint16 * mult);
		break;
	case UINT32: 
		snprintf(buff, PLUGIN_BUFFER_SIZE, "%.*f", decs, arg->val->uint32 * mult);
		break;
	case UINT64:
		snprintf(buff, PLUGIN_BUFFER_SIZE, "%.*f", decs, arg->val->uint64 * mult);
		break;
	case FLOAT:
		snprintf(buff, PLUGIN_BUFFER_SIZE, "%.*f", decs, arg->val->flt * mult);
		break;
	case DOUBLE:
		snprintf(buff, PLUGIN_BUFFER_SIZE, "%.*f", decs, arg->val->dbl * mult);
		break;
	default:
		snprintf(buff, arg->val->blob.length, "%s", arg->val->blob.ptr);
		break;
	}
}

void format(const plugin_arg_t *arg, int plain_numbers, char buff[PLUGIN_BUFFER_SIZE], void *conf)
{	
	struct mult_conf *config = (struct mult_conf *) conf;
	
	if (plain_numbers) {
		toString(arg, buff, 1, 0);
		return;
	}
	
	toString(arg, buff, config->multiplier, config->decimals);
}


void parse(char *input, char out[PLUGIN_BUFFER_SIZE], void *conf)
{
	struct mult_conf *config = (struct mult_conf *) conf;
	
	if (config->multiplier == 0.0) {
		snprintf(out, PLUGIN_BUFFER_SIZE, "0");
		return;
	}
	
	snprintf(out, PLUGIN_BUFFER_SIZE, "%.*f", config->decimals, atof(input) / config->multiplier);
}

void close(void **conf)
{
	struct mult_conf *config = (struct mult_conf *) *conf;
	
	free(config);
	*conf = NULL;
}
