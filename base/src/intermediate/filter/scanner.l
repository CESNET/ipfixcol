/**
 * \file scanner.l
 * \author Michal Kozubik <kozubik@cesnet.cz>
 * \brief Flex ipfixcol filter scanner
 *
 * Copyright (C) 2015 CESNET, z->s.p.o.
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


%{
#include "parser.h"
#include "filter.h"
#include <string.h>
%}

 
/* do not use yywrap, unput, default functions. make lexer reentrant */
%option reentrant
%option bison-bridge 
%option bison-locations
%option noyywrap 
%option nounput 
%option nodefault  
%option yylineno

%{
#define YY_USER_ACTION yylloc->first_line = yylineno;
%}

/* regexp aliases */
_RAWFIELD e[0-9]+id[0-9]+
_NUMBER [0-9]+[kKmMgGtT]{0,1}
_HEXNUM 0x[0-9a-fA-F]+
_IPv4 (25[0-5]|2[0-4][0-9]|1[0-9][0-9]|[1-9]?[0-9])(\.(25[0-5]|2[0-4][0-9]|1[0-9][0-9]|[1-9]?[0-9])){3}
_IPv6 ((([0-9A-Fa-f]{1,4}:){7}([0-9A-Fa-f]{1,4}|:))|(([0-9A-Fa-f]{1,4}:){6}(:[0-9A-Fa-f]{1,4}|((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3})|:))|(([0-9A-Fa-f]{1,4}:){5}(((:[0-9A-Fa-f]{1,4}){1,2})|:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3})|:))|(([0-9A-Fa-f]{1,4}:){4}(((:[0-9A-Fa-f]{1,4}){1,3})|((:[0-9A-Fa-f]{1,4})?:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}))|:))|(([0-9A-Fa-f]{1,4}:){3}(((:[0-9A-Fa-f]{1,4}){1,4})|((:[0-9A-Fa-f]{1,4}){0,2}:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}))|:))|(([0-9A-Fa-f]{1,4}:){2}(((:[0-9A-Fa-f]{1,4}){1,5})|((:[0-9A-Fa-f]{1,4}){0,3}:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}))|:))|(([0-9A-Fa-f]{1,4}:){1}(((:[0-9A-Fa-f]{1,4}){1,6})|((:[0-9A-Fa-f]{1,4}){0,4}:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}))|:))|(:(((:[0-9A-Fa-f]{1,4}){1,7})|((:[0-9A-Fa-f]{1,4}){0,5}:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}))|:)))(%.+)?
_TIMESTAMP [0-9]{4}"/"[0-9]{2}"/"[0-9]{2}\.[0-9]{2}:[0-9]{2}:[0-9]{2}[smun]{0,1}
_CMP ">"|"<"|"="|"=="|"=>"|"<="|">="|"=>"|"!="
_OPERATOR AND|OR|and|or|"||"|&&
_NOT not|NOT
_EXISTS exists|EXISTS 
_BRACKET "("|")"
_FIELD [a-zA-Z][a-zA-Z0-9]+
_WHITESPACE " "+|\t+
_REGEX  \"\/[^(\/\")]*\/\"
_STRING \"[^"]*\" 
%%

{_RAWFIELD}    { yylval->s = strndup(yytext, yyleng); return RAWFIELD; }
{_WHITESPACE} 	{ yylloc->last_column++; yylloc->first_column++; /* update location even when skipping the input */ }
\n+             { yylloc->last_line += yyleng; yylloc->first_line = yylloc->last_line; /* update location on newline */ }
{_NUMBER} 	    { yylval->s = strndup(yytext, yyleng); return NUMBER; }
{_HEXNUM}		{ yylval->s = strndup(yytext, yyleng); return HEXNUM; }
{_IPv4}         { yylval->s = strndup(yytext, yyleng); return IPv4; }
{_IPv6}         { yylval->s = strndup(yytext, yyleng); return IPv6; }
{_TIMESTAMP}    { yylval->s = strndup(yytext, yyleng); return TIMESTAMP; }
{_CMP}          { yylval->s = strndup(yytext, yyleng); return CMP; }
{_OPERATOR}   	{ yylval->s = strndup(yytext, yyleng); return OPERATOR; }
{_NOT}			{ return NOT; }
{_EXISTS}		{ return EXISTS; }
{_FIELD}        { yylval->s = strndup(yytext, yyleng); return FIELD; }
{_BRACKET}		{ return yytext[0]; }
{_REGEX} 		{ yylval->s = strndup(yytext + 2, yyleng - 4); return REGEX; }
{_STRING}		{ yylval->s = strndup(yytext + 1, yyleng - 2); return STRING; }
.				{yylloc->last_column++; yylloc->first_column++;}
%%
 
