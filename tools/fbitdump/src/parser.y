/**
 * \file parser.y
 * \author Petr Velan <petr.velan@cesnet.cz>
 * \brief Bison parser for fbitdump filter
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

/* generate C++ parser */
%language "C++"
%define namespace "parser"
%defines
/* Use locations (built-in feature to track position in input) */
%locations
/* Print meaningful error messages */
%error-verbose

/* parse class name */
%define parser_class_name "Parser"

%code requires {
/* Forward declaration of Filter in fbitdump namespace*/
namespace fbitdump {
	class Filter;
	struct _parserStruct;
}
}

%{
#include <string>
#include <iostream> 
#include "Filter.h"
#include <vector>

/* this is a workaround for %lex-param which takes only last 
   part of filter.scaninfo: scaninfo */
#define SCANINFO filter.scaninfo
%}

/* Class constructor parameter */
%parse-param { fbitdump::Filter &filter }
/* parameters for lexer */
%lex-param { fbitdump::Filter &filter }
%lex-param { SCANINFO }

/* this is to be used for yylval type. */
%union {
	std::string *s;
	struct fbitdump::_parserStruct *ps;
	std::vector<struct fbitdump::_parserStruct *> *v;
}

/* tokens that are later also included in lexer */
%token <s> COLUMN		"column"
%token <s> NUMBER		"number"
%token <s> HEXNUM		"hexa number"
%token <s> FLOAT                "float number"
%token <s> CMP			"comparison"
%token <s> RAWCOLUMN	"raw column"
%token <s> OPERATOR		"operator"
%token <s> BITOPERATOR	"bit operator"
%token <s> MAC                  "MAC address"
%token <s> IPv4			"IPv4 address"
%token <s> IPv4_SUB		"IPv4 address with subnet"
%token <s> IPv6			"IPv6 address"
%token <s> IPv6_SUB		"IPv6 address with subnet"
%token <s> TIMESTAMP	"timestamp"
%token <s> OTHER		"symbol"
%token <s> EOL			"end of line"
%token <s> STRING		"string"
%token <s> IN			"in list"
%token <s> NOT			"not"
%token <s> EXISTS		"exists"
%token END 0			"end of file"

%type <s> explist exp start
%type <ps> value column
%type <v> list

/* operators for joining exps are left-associative, therefore the rule "explist OPERATOR explist" always reduces */
%left OPERATOR

%{
/* yylex function declaration */
extern YY_DECL;

/* implementation of error function */
void parser::Parser::error(location const &loc, const std::string &msg) {
	filter.error(loc, msg);
}

%}

%initial-action {
    // @$.begin.filename = @$.end.filename = new std::string("stdin");
    //{ $$ = new std::string(); } <-- prazdne pravidlo pro start?
}

%%
%start start;

start:
	explist { filter.setFilterString(*$1); delete $1;}
	;

explist:
	  exp { $$ = $1; }
	| NOT exp { $$ = new std::string("NOT (" + *$2 + ")"); delete $1; delete $2; }
	| list { $$ = new std::string(filter.parseExpList($1)); delete $1; }
	| NOT list { $$ = new std::string("NOT" + filter.parseExpList($2)); delete $2; }
	| '(' explist ')' { $$ = new std::string("(" + *$2 + ")"); delete $2; }
	| explist OPERATOR explist { $$ = new std::string(*$1 + " " + *$2 + " " + *$3); delete $1; delete $2; delete $3; }
	;

exp:
	  column CMP value {  $$ = new std::string(filter.parseExp($1, *$2, $3)); delete $1; delete $2; delete $3; }
	| value CMP column {  $$ = new std::string(filter.parseExp($3, *$2, $1)); delete $1; delete $2; delete $3; }
	| column CMP column { $$ = new std::string(filter.parseExp($1, *$2, $3)); delete $1; delete $2; delete $3; }
	| column value { $$ = new std::string(filter.parseExp($1, $2)); delete $1; delete $2; }
	| EXISTS column { $$ = new std::string(filter.parseExists($2)); delete $1; delete $2; }
    ;

list:
	  list value { $$ = $1; filter.parseListAdd($$, $2); }
	| column IN value { $$ = new std::vector<fbitdump::_parserStruct *>; filter.parseListCreate($$, "in", $1); filter.parseListAdd($$, $3); delete $2; }
	| column NOT IN value { $$ = new std::vector<fbitdump::_parserStruct *>; filter.parseListCreate($$, "not in", $1); filter.parseListAdd($$, $4); delete $2; delete $3; }
	;


value:
	  NUMBER { $$ = new fbitdump::_parserStruct; filter.parseNumber($$, *$1); delete $1; }
	| HEXNUM { $$ = new fbitdump::_parserStruct; filter.parseHex($$, *$1); delete $1; }
	| FLOAT  { $$ = new fbitdump::_parserStruct; filter.parseFloat($$, *$1); delete $1; }
	| MAC  { $$ = new fbitdump::_parserStruct; filter.parseString($$, *$1); delete $1; }
	| IPv4 { $$ = new fbitdump::_parserStruct; filter.parseIPv4($$, *$1); delete $1; }
	| IPv4_SUB { $$ = new fbitdump::_parserStruct; filter.parseIPv4Sub($$, *$1); delete $1; }
	| IPv6 { $$ = new fbitdump::_parserStruct; filter.parseIPv6($$, *$1); delete $1; }
	| IPv6_SUB { $$ = new fbitdump::_parserStruct; filter.parseIPv6Sub($$, *$1); delete $1; }
	| TIMESTAMP { $$ = new fbitdump::_parserStruct; filter.parseTimestamp($$, *$1); delete $1; }
	| STRING { $$ = new fbitdump::_parserStruct; filter.parseString($$, *$1); delete $1; }
	;
	
column:
	  COLUMN { $$ = new fbitdump::_parserStruct; $$->nParts = 0; filter.parseColumn($$, *$1); delete $1; }
	| RAWCOLUMN { $$ = new fbitdump::_parserStruct; filter.parseRawcolumn($$, *$1); delete $1; }
	| column BITOPERATOR value { $$ = new fbitdump::_parserStruct; filter.parseBitColVal($$, $1, *$2, $3); delete $1; delete $2; delete $3; }
	;

%%
