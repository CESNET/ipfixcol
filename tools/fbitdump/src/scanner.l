/**
 * \file scanner.l
 * \author Petr Velan <petr.velan@cesnet.cz>
 * \brief Flex fbitdump filter scanner
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
%{
#include <iostream>
#include <string>
#include "Filter.h"
#include "parser.h"

/* this is called on every action - updates location */
#define YY_USER_ACTION yylloc->columns(yyleng);

/* typedef to simplify token usage */
typedef parser::Parser::token token;
%}

/* do not use yywrap, unput, default functions. make lexer reentrant */
%option noyywrap nounput nodefault reentrant

/* regexp aliases */
_COLUMN %[a-zA-Z0-9]+
_RAWCOLUMN e[0-9]+id[0-9]+(p[0-9]+)?
_NUMBER [0-9]+([kKmMgGtT]{0,1}|[uU]?[lL]?)
_HEXNUM 0x[0-9a-fA-F]+
_FLOAT  [0-9]+\.[0-9]+[kKmMgGtT]{0,1}
_MAC ([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}
_IPv4 (25[0-5]|2[0-4][0-9]|1[0-9][0-9]|[1-9]?[0-9])(\.(25[0-5]|2[0-4][0-9]|1[0-9][0-9]|[1-9]?[0-9])){3}
_IPv4_SUB (25[0-5]|2[0-4][0-9]|1[0-9][0-9]|[1-9]?[0-9])(.(25[0-5]|2[0-4][0-9]|1[0-9][0-9]|[1-9]?[0-9])){3}\/(3[0-2]|[1-2][0-9]|[1-9])
_IPv6 ((([0-9A-Fa-f]{1,4}:){7}([0-9A-Fa-f]{1,4}|:))|(([0-9A-Fa-f]{1,4}:){6}(:[0-9A-Fa-f]{1,4}|((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3})|:))|(([0-9A-Fa-f]{1,4}:){5}(((:[0-9A-Fa-f]{1,4}){1,2})|:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3})|:))|(([0-9A-Fa-f]{1,4}:){4}(((:[0-9A-Fa-f]{1,4}){1,3})|((:[0-9A-Fa-f]{1,4})?:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}))|:))|(([0-9A-Fa-f]{1,4}:){3}(((:[0-9A-Fa-f]{1,4}){1,4})|((:[0-9A-Fa-f]{1,4}){0,2}:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}))|:))|(([0-9A-Fa-f]{1,4}:){2}(((:[0-9A-Fa-f]{1,4}){1,5})|((:[0-9A-Fa-f]{1,4}){0,3}:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}))|:))|(([0-9A-Fa-f]{1,4}:){1}(((:[0-9A-Fa-f]{1,4}){1,6})|((:[0-9A-Fa-f]{1,4}){0,4}:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}))|:))|(:(((:[0-9A-Fa-f]{1,4}){1,7})|((:[0-9A-Fa-f]{1,4}){0,5}:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}))|:)))(%.+)?
_IPv6_SUB ((([0-9A-Fa-f]{1,4}:){7}([0-9A-Fa-f]{1,4}|:))|(([0-9A-Fa-f]{1,4}:){6}(:[0-9A-Fa-f]{1,4}|((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3})|:))|(([0-9A-Fa-f]{1,4}:){5}(((:[0-9A-Fa-f]{1,4}){1,2})|:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3})|:))|(([0-9A-Fa-f]{1,4}:){4}(((:[0-9A-Fa-f]{1,4}){1,3})|((:[0-9A-Fa-f]{1,4})?:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}))|:))|(([0-9A-Fa-f]{1,4}:){3}(((:[0-9A-Fa-f]{1,4}){1,4})|((:[0-9A-Fa-f]{1,4}){0,2}:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}))|:))|(([0-9A-Fa-f]{1,4}:){2}(((:[0-9A-Fa-f]{1,4}){1,5})|((:[0-9A-Fa-f]{1,4}){0,3}:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}))|:))|(([0-9A-Fa-f]{1,4}:){1}(((:[0-9A-Fa-f]{1,4}){1,6})|((:[0-9A-Fa-f]{1,4}){0,4}:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}))|:))|(:(((:[0-9A-Fa-f]{1,4}){1,7})|((:[0-9A-Fa-f]{1,4}){0,5}:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}))|:)))(%.+)?\/(12[0-8]|1[0-1][0-9]|[1-9][0-9]|[1-9])
_TIMESTAMP [0-9]{4}"/"[0-9]{2}"/"[0-9]{2}\.[0-9]{2}:[0-9]{2}:[0-9]{2}
_CMP ">"|"<"|"="|"=="|"=>"|"<="|">="|"=>"|"!="
_OPERATOR AND|OR|and|or|xor|XOR
_BITOPERATOR &|"|"
_IN in|IN
_NOT not|NOT
_EXISTS exists|EXISTS
_BRACKET "("|")"
_WHITESPACE " "+|\t+
_STRING \'[^']*\'|[^ =><!()]+|!
%%

%{
    /* initialise locations */
    yylloc->step();
%}

{_COLUMN}       { yylval->s = new std::string(yytext); return token::COLUMN; }
{_RAWCOLUMN}    { yylval->s = new std::string(yytext); return token::RAWCOLUMN; }
{_WHITESPACE} 	{ yylloc->step(); /* update location even when skipping the input */ }
\n+             { yylloc->lines(yyleng); /* update location on newline */ }
{_NUMBER} 	    { yylval->s = new std::string(yytext); return token::NUMBER; }
{_HEXNUM}		{ yylval->s = new std::string(yytext); return token::HEXNUM; }
{_FLOAT}        { yylval->s = new std::string(yytext); return token::FLOAT; }
{_MAC}          { yylval->s = new std::string(yytext); return token::MAC; }
{_IPv4}         { yylval->s = new std::string(yytext); return token::IPv4; }
{_IPv4_SUB}     { yylval->s = new std::string(yytext); return token::IPv4_SUB; }
{_IPv6}         { yylval->s = new std::string(yytext); return token::IPv6; }
{_IPv6_SUB}     { yylval->s = new std::string(yytext); return token::IPv6_SUB; }
{_TIMESTAMP}    { yylval->s = new std::string(yytext); return token::TIMESTAMP; }
{_CMP}          { yylval->s = new std::string(yytext); return token::CMP; }
{_OPERATOR}   	{ yylval->s = new std::string(yytext); return token::OPERATOR; }
{_BITOPERATOR} 	{ yylval->s = new std::string(yytext); return token::BITOPERATOR; }
{_IN}			{ yylval->s = new std::string(yytext); return token::IN; }
{_NOT}			{ yylval->s = new std::string(yytext); return token::NOT; }
{_EXISTS}		{ yylval->s = new std::string(yytext); return token::EXISTS; }
{_BRACKET}		{ return yytext[0]; }
{_STRING}		{ yylval->s = new std::string(yytext); return token::STRING; }

%%
 
