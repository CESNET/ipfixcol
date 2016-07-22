/*

 Copyright (c) 2013-2015, Tomas Podermanski

 This file is part of libnf.net project.

 Libnf is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 Libnf is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with libnf.  If not, see <http://www.gnu.org/licenses/>.

*/

%defines
%pure-parser
%lex-param   	{ yyscan_t scanner }
%lex-param	{ ff_t *filter }
%parse-param 	{ yyscan_t scanner }
%parse-param 	{ ff_t *filter }
%name-prefix = "ff2_"

%{
	#include <stdio.h>
	#include "ffilter.h"
	#include "ffilter_internal.h"
	#include <string.h>

	#define YY_EXTRA_TYPE ff_t

//	int ff2_lex();

	void yyerror(yyscan_t scanner, ff_t *filter, char *msg)
	{
		ff_set_error(filter, msg);
	}


%}

%union {
	uint64_t	t_uint;
	double		t_double;
	char 		string[FF_MAX_STRING];
	void		*node;
};

%token AND OR NOT ANY
%token EQ LT GT ISSET
%token LP RP
%token LPS RPS IN
%token <string> STRING DIR BIDIR_AND BIDIR_OR DIR_DIR_MAC
%token BAD_TOKEN

%type <string> field value
%type <node> expr filter list

%left	OR
%left	AND
%left	NOT

%%

filter:
	expr                { filter->root = $1; }
	|                   { filter->root = NULL; }
	;

field:
	STRING              { strncpy($$, $1, FF_MAX_STRING - 1); }
	| DIR STRING        { snprintf($$, FF_MAX_STRING - 1, "%s %s", $1, $2); }
	| BIDIR_OR STRING   { snprintf($$, FF_MAX_STRING - 1, "%c%s", '|', $2); }
	| BIDIR_AND STRING  { snprintf($$, FF_MAX_STRING - 1, "%c%s", '&', $2); }
	| DIR_DIR_MAC STRING { snprintf($$, FF_MAX_STRING - 1, "%s %s", $1, $2); }
	;

value:
	STRING              { strncpy($$, $1, FF_MAX_STRING - 1); }
	| STRING STRING     { snprintf($$, FF_MAX_STRING - 1, "%s %s", $1, $2); }
	;

expr:
	ANY                 { $$ = ff_new_node(scanner, filter, NULL, FF_OP_YES, NULL); if ($$ == NULL) { YYABORT; }; }
	|NOT expr           { $$ = ff_new_node(scanner, filter, NULL, FF_OP_NOT, $2); if ($$ == NULL) { YYABORT; }; }
	| expr AND expr     { $$ = ff_new_node(scanner, filter, $1, FF_OP_AND, $3); if ($$ == NULL) { YYABORT; }; }
	| expr OR expr      { $$ = ff_new_node(scanner, filter, $1, FF_OP_OR, $3); if ($$ == NULL) { YYABORT; }; }
	| LP expr RP        { $$ = $2; }
	| field value       { $$ = ff_new_leaf(scanner, filter, $1, FF_OP_NOOP, $2); if ($$ == NULL) { YYABORT; } }
	| field ISSET value { $$ = ff_new_leaf(scanner, filter, $1, FF_OP_ISSET, $3); if ($$ == NULL) { YYABORT; } }
	| field EQ value    { $$ = ff_new_leaf(scanner, filter, $1, FF_OP_EQ, $3); if ($$ == NULL) { YYABORT; } }
	| field LT value    { $$ = ff_new_leaf(scanner, filter, $1, FF_OP_LT, $3); if ($$ == NULL) { YYABORT; } }
	| field GT value    { $$ = ff_new_leaf(scanner, filter, $1, FF_OP_GT, $3); if ($$ == NULL) { YYABORT; } }

	| field IN list     {  $$ = ff_new_leaf(scanner, filter, $1, FF_OP_IN, $3); if ($$ == NULL) { YYABORT; } }
	;

list:
	STRING list         { $$ = ff_new_mval(scanner, filter, $1, FF_OP_EQ, $2); if ($$ == NULL) { YYABORT; } }
	| STRING RPS        { $$ = ff_new_mval(scanner, filter, $1, FF_OP_EQ, NULL); if ($$ == NULL) { YYABORT; } }
	;

%%

