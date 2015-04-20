/**
 * \file parser.y
 * \author Michal Kozubik <kozubik@cesnet.cz>
 * \brief Bison parser for ipfixcol filter
 *
 * Copyright (C) 2015 CESNET, z.s.p.o.
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

/* generate C parser */
%language "C"
%pure-parser
%defines
%locations
%error-verbose

/* Needed structures */
%code requires { 

struct filter_treenode;
struct filter_parser_data;
struct filter_value;

}

/* Include headers */
%{
#include <stdint.h>
#include "filter.h"

%}

/* Parser parameter (profile) and lexer parameter (scanner) */
%parse-param { struct filter_parser_data *data }
%lex-param { void *scanner}

%{
int yylex(YYSTYPE* lvalp, YYLTYPE* llocp, void* scanner);

void yyerror(YYLTYPE *loc, struct filter_parser_data *p, const char *msg)
{
	(void) p; (void) loc;
	filter_error(msg, loc);
}

#define scanner  data->scanner
#define profile  data->profile
#define doc      data->doc
#define context  data->context
%}

%union {
char *s;
int i;
struct filter_value *v;
struct filter_treenode *n;
struct filter_field *f;
}

/* tokens that are later also included in lexer */
%token <s> FIELD		"field"
%token <s> NUMBER		"number"
%token <s> HEXNUM		"hexa number"
%token <s> CMP			"comparison"
%token <s> RAWFIELD		"raw field"
%token <s> OPERATOR		"operator"
%token <s> IPv4			"IPv4 address"
%token <s> IPv6			"IPv6 address"
%token <s> TIMESTAMP	"timestamp"
%token <s> OTHER		"symbol"
%token <s> EOL			"end of line"
%token <s> NOT			"not"
%token <s> EXISTS		"exists"
%token <s> STRING		"string"
%token <s> REGEX		"regexp"
%token END 0			"end of file"

%type <n> explist exp start
%type <v> value
%type <f> field

%left OPERATOR


%%
%start start;

start:
	explist { filter_set_root(profile, $$); }
	;

explist:
	  exp { $$ = $1; }
	| NOT exp { $$ = $2; filter_node_set_negated($$); }
	| '(' explist ')' { $$ = $2; }
	| explist OPERATOR explist { $$ = filter_new_parent_node($1, $2, $3); free($2); if (!$$) YYABORT;}
	;

exp:
	  field CMP value {  $$ = filter_new_leaf_node($1, $2, $3); free($2); if (!$$) YYABORT; }
	| value CMP field {  $$ = filter_new_leaf_node($3, $2, $1); free($2); if (!$$) YYABORT; }
	| field value { $$ = filter_new_leaf_node_opless($1, $2); if (!$$) YYABORT; }
	| EXISTS field { $$ = filter_new_exists_node($2); if (!$$) YYABORT; }
    ;

value:
	  NUMBER { $$ = filter_parse_number($1); free($1); if (!$$) YYABORT;}
	| HEXNUM { $$ = filter_parse_hexnum($1); free($1); if (!$$) YYABORT;}
	| STRING { $$ = filter_parse_string($1); free($1); if (!$$) YYABORT;}
	| REGEX  { $$ = filter_parse_regex($1);  free($1); if (!$$) YYABORT;}
	| IPv4   { $$ = filter_parse_ipv4($1);   free($1); if (!$$) YYABORT;}
	| IPv6   { $$ = filter_parse_ipv6($1);   free($1); if (!$$) YYABORT;}
	| TIMESTAMP { $$ = filter_parse_timestamp($1); free($1); if (!$$) YYABORT;}
	;
	
field:
	  FIELD    { $$ = filter_parse_field($1, doc, context); free($1); if (!$$) YYABORT; }
	| RAWFIELD { $$ = filter_parse_rawfield($1);            free($1); if (!$$) YYABORT; }
	;

%%
