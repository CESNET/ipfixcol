#include "config.h"

#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/resource.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#include "ffilter_internal.h"
#include "ffilter_gram.h"
#include "ffilter.h"

/*
 * \brief Convert unit character to positive power of 10
 * \param[in] unit Suffix of number
 * \return Scale eg. (1k -> 1000) etc.
 */
uint64_t get_unit(char *unit)
{
	if (strlen(unit) > 1) return 0;

	switch (*unit) {
		case 'k':
		case 'K':
			return FF_SCALING_FACTOR;
		case 'm':
		case 'M':
			return FF_SCALING_FACTOR * FF_SCALING_FACTOR;
		case 'g':
		case 'G':
			return FF_SCALING_FACTOR * FF_SCALING_FACTOR * FF_SCALING_FACTOR;
		case 'T':
			return FF_SCALING_FACTOR * FF_SCALING_FACTOR * FF_SCALING_FACTOR * FF_SCALING_FACTOR;
		case 'E':
			return FF_SCALING_FACTOR * FF_SCALING_FACTOR * FF_SCALING_FACTOR * FF_SCALING_FACTOR * FF_SCALING_FACTOR;
		default:
			return 0;
	}

}

/*
 * \brief Function adds support for k/M/G/T/E suffixes to strtoll
 * \param[in] num Literal number
 * \param endptr Place to store an address where conversion finised
 */
int64_t strtoll_unit(char *num, char**endptr)
{
	int64_t tmp64;
	int64_t mult = 0;

	tmp64 = strtoll(num, endptr, 10);
	if (!**endptr) {
		return tmp64;
	} else if (!*(*endptr+1)) {
		mult = get_unit(*endptr);
		if (mult != 0) {
			/*Move conversion end potinter by one*/
			*endptr = (*endptr + 1);
		}
	}
	return tmp64*mult;
}

/* convert string into uint64_t */
/* also converts string with units (64k -> 64000) */
int str_to_uint(char *str, int type, char **res, int *vsize)
{

	uint64_t tmp64;
	uint32_t tmp32;
	uint32_t tmp16;
	uint32_t tmp8;
	void *tmp, *ptr;

	char* endptr;
	tmp64 = strtoll_unit(str, &endptr);
	if (*endptr){
		return 0;
	}

	switch (type) {
		case FF_TYPE_UINT64:
			*vsize = sizeof(uint64_t);
			tmp = &tmp64;
			break;
		case FF_TYPE_UINT32:
			*vsize = sizeof(uint32_t);
			tmp32 = tmp64;
			tmp = &tmp32;
			break;
		case FF_TYPE_UINT16:
			*vsize = sizeof(uint16_t);
			tmp16 = tmp64;
			tmp = &tmp16;
			break;
		case FF_TYPE_UINT8:
			*vsize = sizeof(uint8_t);
			tmp8 = tmp64;
			tmp = &tmp8;
			break;
		default: return 0;
	}

	ptr = malloc(*vsize);

	if (ptr == NULL) {
		return 0;
	}

	memcpy(ptr, tmp, *vsize);

	*res = ptr;

	return 1;
}

/* convert string into int64_t */
/* also converts string with units (64k -> 64000) */
int str_to_int(char *str, int type, char **res, int *vsize)
{

	int64_t tmp64;
	int32_t tmp32;
	int32_t tmp16;
	int32_t tmp8;
	void *tmp, *ptr;

	char *endptr;
	tmp64 = strtoll_unit(str, &endptr);
	if (*endptr){
		return 0;
	}

	switch (type) {
		case FF_TYPE_INT64:
			*vsize = sizeof(int64_t);
			tmp = &tmp64;
			break;
		case FF_TYPE_INT32:
			*vsize = sizeof(int32_t);
			tmp32 = tmp64;
			tmp = &tmp32;
			break;
		case FF_TYPE_INT16:
			*vsize = sizeof(int16_t);
			tmp16 = tmp64;
			tmp = &tmp16;
			break;
		case FF_TYPE_INT8:
			*vsize = sizeof(int8_t);
			tmp8 = tmp64;
			tmp = &tmp8;
			break;
		default: return 0;
	}

	ptr = malloc(*vsize);

	if (ptr == NULL) {
		return 0;
	}

	memcpy(ptr, tmp, *vsize);

	*res = ptr;

	return 1;
}

/* convert string into lnf_ip_t */
//TODO: Solve masked addresses (maybe add one more ip addr as mask so eval wold be & together ip and mask then eval
int str_to_addr(ff_t *filter, char *str, char **res, int *numbits)
{
	ff_ip_t *ptr;

	ptr = malloc(sizeof(ff_ip_t));

	if (ptr == NULL) {
		return 0;
	}

	char *saveptr;
	char *ip_str = strdup(str);
	char *ip;
	char *mask;

	memset(ptr, 0x0, sizeof(ff_ip_t));

	*numbits = 0;

	*res = (char *)ptr;

	ip = strtok_r(ip_str, "\\/", &saveptr);
	mask = strtok_r(NULL, "\\/", &saveptr);

	/* IPv4 only */
	if (mask != NULL) {
		*numbits = strtoul(mask, &saveptr, 10);
		if (*saveptr){
			return 1;
		}

		int req_oct = (*numbits - 1) / 8 + 1;

		/* If string passes this filter, it has enough octets, rest ist concatenated */
		char *ip_dup = strdup(ip_str);
		int octet = 0;
		for (ip = strtok_r(ip_dup, ".", &saveptr); octet < req_oct; octet++ ) {
			if (ip == NULL) {
				free(ip_str);
				free(ip_dup);
				return 0;
			}

			ip = strtok_r(NULL, ".", &saveptr);
		}
		free(ip_dup);

		char *suffix = malloc(8);
		suffix[0] = 0;
		for (; req_oct < 4; req_oct++) {
			strcat(suffix, ".0");
		}
		ip = realloc(ip_str, strlen(ip_str)+8);
		strcat(ip, suffix);

	}

	if (inet_pton(AF_INET, ip, &((*ptr).data[3]))) {
		free(ip);
		return 1;
	}

	if (inet_pton(AF_INET6, ip, ptr)) {
		free(ip);
		return 1;
	}

	ff_set_error(filter, "Can't convert '%s' into IP address", str);

	free(ip_str);
	return 0;
}

ff_error_t ff_type_cast(yyscan_t *scanner, ff_t *filter, char *valstr, ff_node_t* node){

	/* determine field type and assign data to lvalue */
	switch (node->type) {
		//switch (lnf_fld_type(field)) {

		case FF_TYPE_UINT64:
		case FF_TYPE_UINT32:
		case FF_TYPE_UINT16:
		case FF_TYPE_UINT8:
			if (str_to_uint(valstr, node->type, &node->value, &node->vsize) == 0) {
				ff_set_error(filter, "Can't convert '%s' into numeric value", valstr);
				return FF_ERR_OTHER;
			}
			break;
		case FF_TYPE_INT64:
		case FF_TYPE_INT32:
		case FF_TYPE_INT16:
		case FF_TYPE_INT8:
			if (str_to_int(valstr, node->type, &node->value, &node->vsize) == 0) {
				ff_set_error(filter, "Can't convert '%s' into numeric value", valstr);

				return FF_ERR_OTHER;
			}
			break;
		case FF_TYPE_ADDR:
			if (str_to_addr(filter, valstr, &node->value, &node->numbits) == 0) {
				return FF_ERR_OTHER;
			}
			node->vsize = sizeof(ff_ip_t);
			break;

		case FF_TYPE_UNSIGNED_BIG:
			/* unsigned with undefined data size (internally mapped to uint64_t in network order) */
		case FF_TYPE_UNSIGNED:
			if (str_to_uint(valstr, FF_TYPE_UINT64, &node->value, &node->vsize) == 0) {
				node->value = malloc(sizeof(uint64_t));
				node->vsize = sizeof(uint64_t);
				if (node->value == NULL || filter->options.ff_translate_func == NULL) {
					ff_set_error(filter, "Can't convert '%s' into numeric value", valstr);
					return FF_ERR_OTHER;
				} else if (filter->options.ff_translate_func(filter, valstr, node->field,
									     node->value) != FF_OK) {
					ff_set_error(filter, "Can't convert '%s' into numeric value", valstr);
					return FF_ERR_OTHER;
				}
			}
//				*(uint64_t *)node->value = htonll(*(uint64_t *)node->value);
			break;
		case FF_TYPE_SIGNED_BIG:
		case FF_TYPE_SIGNED:
			if (str_to_uint(valstr, FF_TYPE_INT64, &node->value, &node->vsize) == 0) {
				ff_set_error(filter, "Can't convert '%s' into numeric value", valstr);
				return FF_ERR_OTHER;
			}
//				*(uint64_t *)node->value = htonll(*(uint64_t *)node->value);
			break;

		default:
			return FF_ERR_OTHER;
	}
	return FF_OK;
}


/* set error to error buffer */
/* set error string */
void ff_set_error(ff_t *filter, char *format, ...) {
va_list args;

	va_start(args, format);
	vsnprintf(filter->error_str, FF_MAX_STRING - 1, format, args);
	va_end(args);
}

/* get error string */
const char* ff_error(ff_t *filter, const char *buf, int buflen) {

	strncpy((char *)buf, filter->error_str, buflen - 1);
	return buf;

}

//TODO: suppport more ids
ff_node_t* ff_branch_node(ff_node_t *node, ff_oper_t oper, ff_lvalue_t* lvalue) {

	ff_node_t *nodeR = ff_new_node(NULL, NULL, NULL, oper, NULL);
	if (nodeR == NULL){
		return NULL;
	}
	ff_node_t *root = ff_new_node(NULL, NULL, node,
				      lvalue->options == -1 ? FF_OP_OR : lvalue->options, nodeR);
	if (root == NULL) {
		free(nodeR);
		return NULL;
	}

	*nodeR = *node;
	nodeR->field = lvalue->id2;

	nodeR->value = malloc(nodeR->vsize);
	memcpy(nodeR->value, node->value, nodeR->vsize);

	if (nodeR->value == NULL) {
		free(nodeR);
		free(root);
		return NULL;
	}
	return root;
}

/* add leaf entry into expr tree */
//TODO: Free allocated memory for more ids in lvalue
ff_node_t* ff_new_leaf(yyscan_t scanner, ff_t *filter, char *fieldstr, ff_oper_t oper, char *valstr) {
	//int field;
	ff_node_t *node;
	ff_node_t *root, *nodeR;
	ff_lvalue_t lvalue;


	/* callback to fetch field type and additional info */
	if (filter->options.ff_lookup_func == NULL) {
		ff_set_error(filter, "Filter lookup function not defined");
		return NULL;
	}

	memset(&lvalue, 0x0, sizeof(ff_lvalue_t));
	lvalue.num = 0;
	lvalue.more = NULL;

	switch (*fieldstr) {
		case '|':
			lvalue.options = FF_OP_OR;
			fieldstr++;
			break;
		case '&':
			lvalue.options = FF_OP_AND;
			fieldstr++;
			break;
		default:
			lvalue.options = -1;
	}

	if (filter->options.ff_lookup_func(filter, fieldstr, &lvalue) != FF_OK) {
		ff_set_error(filter, "Can't lookup field type for %s", fieldstr);
		return NULL;
	}

	node = ff_new_node(scanner, filter, NULL, oper, NULL);
	if (node == NULL) {
		return NULL;
	}

	node->type = lvalue.type;
	node->field = lvalue.id;

	if (oper == FF_OP_IN) {
		//Htab nieje moc dobre riesenie
		//node->value = ff_htab_from_node(node, (ff_node_t*)valstr);
		node->value = valstr;
		return node;
	}

	if(ff_type_cast(scanner, filter, valstr, node) != FF_OK) {
		return NULL;
	}

	node->left = NULL;
	node->right = NULL;

	if (lvalue.id2.index == 0) {
		if (lvalue.options != -1) {
			free(node);
			return NULL;
		}
		return node;
	} else {
		//Setup nodes in or configuration for pair fields (src/dst etc.)
		ff_node_t* new_leaf = ff_branch_node(node, oper, &lvalue);
		if (new_leaf == NULL) {
			free(lvalue.more);
			free(node);
		}
		return new_leaf;
	}
}

/* add node entry into expr tree */
ff_node_t* ff_new_node(yyscan_t scanner, ff_t *filter, ff_node_t* left, ff_oper_t oper, ff_node_t* right) {

	ff_node_t *node;

	node = malloc(sizeof(ff_node_t));

	if (node == NULL) {
		return NULL;
	}

	node->vsize = 0;
	node->type = 0;
	node->oper = oper;

	node->left = left;
	node->right = right;

	return node;
}

/* add new item to tree */
ff_node_t* ff_new_mval(yyscan_t scanner, ff_t *filter, char *valstr, ff_oper_t oper, ff_node_t* nptr) {

	ff_node_t *node;

	node = malloc(sizeof(ff_node_t));

	if (node == NULL) {
		return NULL;
	}

	node->vsize = strlen(valstr);
	node->value = strdup(valstr);
	node->type = FF_TYPE_STRING;
	node->oper = oper;

	node->left = NULL;
	node->right = nptr;

	return node;
}

/* evaluate node in tree or proces subtree */
/* return 0 - false; 1 - true; -1 - error  */
int ff_eval_node(ff_t *filter, ff_node_t *node, void *rec) {
	char buf[FF_MAX_STRING];
	int left, right, res;
	size_t size;

	if (node == NULL) {
		return -1;
	}

	left = 0;

	/* go deeper into tree */
	if (node->left != NULL ) {
		left = ff_eval_node(filter, node->left, rec);

		/* do not evaluate if the result is obvious */
		if (node->oper == FF_OP_NOT)              { return !left; };
		if (node->oper == FF_OP_OR  && left == 1) { return 1; };
		if (node->oper == FF_OP_AND && left == 0) { return 0; };
	}

	if (node->right != NULL ) {
		right = ff_eval_node(filter, node->right, rec);

		switch (node->oper) {
			case FF_OP_NOT: return !right; break;
			case FF_OP_OR:  return left || right; break;
			case FF_OP_AND: return left && right; break;
			default: break;
		}
	}

	/* operations on leaf -> compare values  */
	/* going to be callback */
	if (filter->options.ff_data_func(filter, rec, node->field, buf, &size) != FF_OK) {
		ff_set_error(filter, "Can't get data");
		return -1;
	}

	/* Equality of tcpControlBits must be handled differently */
	if (node->field.index == 6 && node->oper == FF_OP_EQ) {

		switch (node->type) {
		case FF_TYPE_UNSIGNED_BIG:
			switch (size) {
				case sizeof(uint16_t):
					res = (ntohs(*(uint16_t *) buf) & *(uint16_t *)node->value) ^
					      *(uint16_t *) node->value;
					break;
				case sizeof(uint8_t):
					res = ((*(uint8_t *) buf) & *(uint8_t *)node->value) ^
					      *(uint8_t *)node->value;
					break;
				default:
					return -1;
			}
			break;
		case FF_TYPE_UNSIGNED:
			switch (size) {
				case sizeof(uint16_t):
					res = ((*(uint16_t *) buf) & *(uint16_t *) node->value) ^
					      *(uint16_t *) node->value;
					break;
				case sizeof(uint8_t):
					res = ((*(uint8_t *) buf) & *(uint8_t *) node->value) ^
					      *(uint8_t *) node->value;
					break;
				default:
					return -1;
			}
			break;

			default: return -1;
		}
		return res == 0 ;
	}

	switch (node->type) {
		case FF_TYPE_UINT64: res = *(uint64_t *)&buf - *(uint64_t *)node->value; break;
		case FF_TYPE_UINT32: res = *(uint32_t *)&buf - *(uint32_t *)node->value; break;
		case FF_TYPE_UINT16: res = *(uint16_t *)&buf - *(uint16_t *)node->value; break;
		case FF_TYPE_UINT8:  res = *(uint8_t *)&buf - *(uint8_t *)node->value; break;
		case FF_TYPE_DOUBLE: res = *(double *)&buf - *(double *)node->value; break;
		case FF_TYPE_STRING: res = strcmp((char *)&buf, node->value); break;
		case FF_TYPE_UNSIGNED_BIG: {

			if (size > node->vsize) { return -1; }		/* too big integer */

			switch (size) {
			case sizeof(uint8_t):
				res = *(uint8_t*)buf - *(uint64_t*)(node->value);
				break;
			case sizeof(uint16_t):
				res = htons(*(uint16_t*)buf) - *(uint64_t*)(node->value);
				break;
			case sizeof(uint32_t):
				res = htonl(*(uint32_t*)buf) - *(uint64_t*)(node->value);
				break;
			case sizeof(uint64_t):
				res = htonll(*(uint64_t*)buf) - *(uint64_t*)(node->value);
				break;
			default:
				res = -1;
				break;
			}
			break;
		}

		case FF_TYPE_UNSIGNED: {

			if (size > node->vsize) { return -1; }		/* too big integer */

			switch (size) {
			case sizeof(uint8_t):
				res = *(uint8_t*)buf - *(uint64_t*)(node->value);
				break;
			case sizeof(uint16_t):
				res = *(uint16_t*)buf - *(uint64_t*)(node->value);
				break;
			case sizeof(uint32_t):
				res = *(uint32_t*)buf - *(uint64_t*)(node->value);
				break;
			case sizeof(uint64_t):
				res = *(uint64_t*)buf - *(uint64_t*)(node->value);
				break;
			default:
				res = -1;
				break;
			}

			break;
		}

		case FF_TYPE_SIGNED_BIG: {

			if (size > node->vsize) { return -1; }		/* too big integer */

			/* copy size bytes of the value to the top of tmp */
			switch (size) {
			case sizeof(int8_t):
				res = *(int8_t*)buf - *(int64_t*)(node->value);
				break;
			case sizeof(int16_t):
				res = htons(*(int16_t*)buf) - *(int64_t*)(node->value);
				break;
			case sizeof(int32_t):
				res = htonl(*(int32_t*)buf) - *(int64_t*)(node->value);
				break;
			case sizeof(int64_t):
				res = htonll(*(int64_t*)buf) - *(int64_t*)(node->value);
				break;
			default:
				res = -1;
				break;
			}
			break;
		}

		case FF_TYPE_SIGNED: {

			if (size > node->vsize) { return -1; }		/* too big integer */

			/* copy size bytes of the value to the top of tmp */
			switch (size) {
			case sizeof(int8_t):
				res = *(int8_t*)buf - *(int64_t*)(node->value);
				break;
			case sizeof(int16_t):
				res = *(int16_t*)buf - *(int64_t*)(node->value);
				break;
			case sizeof(int32_t):
				res = *(int32_t*)buf - *(int64_t*)(node->value);
				break;
			case sizeof(int64_t):
				res = *(int64_t*)buf - *(int64_t*)(node->value);
				break;
			default:
				res = -1;
				break;
			}

			break;
		}
		/* Compare 32bit ip to 128bit encapsulated ip */
		case FF_TYPE_ADDR: {

			/* Compare ip addresses by full length comparation*/
			char *node_data;
			int cmp_bytes_n;
			uint8_t mask = ~0;

			if (!node->numbits) {
				switch (size) {
					case sizeof(ff_ip_t):
						res = memcmp(buf, node->value, node->vsize);
						break;
					case sizeof(ff_ip_t) >> 2:
						res = memcmp(buf, &(((ff_ip_t *) node->value)->data[3]), node->vsize >> 2);
						break;
					default:
						return -1;
				}
			/* Compare masked ip addresses or ranges of addresses */
			} else if (node->oper == FF_OP_EQ){
				switch (size) {
					case sizeof(ff_ip_t):
						node_data = node->value;
						break;
					case sizeof(ff_ip_t) >> 2:
						node_data = &(((ff_ip_t *) node->value)->data[3]);
						break;
					default:
						return -1;
				}

				cmp_bytes_n = node->numbits >> 3; // div 8
				mask = ~(mask >> (node->numbits & 0b111));

				res = memcmp(node_data, buf, cmp_bytes_n);

				res += (node_data[cmp_bytes_n] & mask) ^ (buf[cmp_bytes_n] & mask);
			}
			break;
		}

	default: res = memcmp(buf, node->value, node->vsize); break;
	}

	/* simple comparsion */
	switch (node->oper) {
	case FF_OP_NOT:
	case FF_OP_OR:
	case FF_OP_AND: return -1 ; break;
	case FF_OP_EQ:  return res == 0; break;
	case FF_OP_NE:  return res != 0; break;
	case FF_OP_GT:  return res > 0; break;
	case FF_OP_LT:  return res < 0; break;
	}

	return -1;
}

ff_error_t ff_options_init(ff_options_t **poptions) {

	ff_options_t *options;

	options = malloc(sizeof(ff_options_t));

	if (options == NULL) {
		*poptions = NULL;
		return FF_ERR_NOMEM;
	}

	*poptions = options;

	return FF_OK;

}

/* release all resources allocated by filter */
ff_error_t ff_options_free(ff_options_t *options) {

	/* !!! memory cleanup */
	free(options);

	return FF_OK;

}


ff_error_t ff_init(ff_t **pfilter, const char *expr, ff_options_t *options) {

	yyscan_t scanner;
	YY_BUFFER_STATE buf;
	int parse_ret;
	ff_t *filter;

	filter = malloc(sizeof(ff_t));
	*pfilter = NULL;

	if (filter == NULL) {
		return FF_ERR_NOMEM;
	}

	filter->root = NULL;


	if (options == NULL) {
		free(filter);
		return FF_ERR_OTHER;

	}
	memcpy(&filter->options, options, sizeof(ff_options_t));

	ff_set_error(filter, "No Error.");

	ff2_lex_init(&scanner);
	buf = ff2__scan_string(expr, scanner);
	parse_ret = ff2_parse(scanner, filter);


	ff2_lex_destroy(scanner);

	/* error in parsing */
	if (parse_ret != 0) {
		*pfilter = filter;
		return FF_ERR_OTHER_MSG;
	}

	*pfilter = filter;

	return FF_OK;
}

/* matches the record agains filter */
/* returns 1 - record was matched, 0 - record wasn't matched */
int ff_eval(ff_t *filter, void *rec) {

	/* call eval node on root node */
	return ff_eval_node(filter, filter->root, rec);

}


void ff_free_node(ff_node_t* node) {

	if (node == NULL) {
		return;
	}

	ff_free_node(node->left);
	ff_free_node(node->left);

	free(node->value);

	free(node);

}

/* release all resources allocated by filter */
ff_error_t ff_free(ff_t *filter) {

	/* !!! memory cleanup */

	if (filter != NULL) {
		ff_free_node(filter->root);
		free(filter);
	}

	return FF_OK;

}

