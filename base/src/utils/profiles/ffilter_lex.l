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

%option prefix="ff2_"
%option reentrant bison-bridge
%option noyywrap 
%option nounput
%option noinput

digit 	[0-9]
letter	[A-Za-z:/\.\-]

ws	[ \t]

%{
	#include <stdlib.h>
	#include <string.h>
	#include "ffilter_internal.h"
	#include "ffilter_gram.h"
	#include "ffilter.h"
//	#define YY_FATAL_ERROR(msg) (printf("LEXXX> %s\n", msg))
%}

%%

and|"&&"		{ return AND; }
or|"||"			{ return OR; }
not|"!"			{ return NOT; }

"in ["             { return IN; }

"="|"=="|eq		{ return EQ; }
">"|gt			{ return GT; }
"<"|lt		    { return LT; }
"("			    { return LP; }
")"			    { return RP; }
"["			    { return LPS; }
"]"			    { return RPS; }

"next"{ws}+"ip"	{
                    // Grammar exceptions
					strncpy(yylval->string, "next ip", FF_MAX_STRING - 1);
					return STRING;
				}
"bgpnext"{ws}+"ip"	{
					strncpy(yylval->string, "bgpnext ip", FF_MAX_STRING - 1);
					return STRING;
				}
"router"{ws}+"ip"	{
					strncpy(yylval->string, "router ip", FF_MAX_STRING - 1);
					return STRING;
				}

"src and dst"|"dst and src"         { strncpy(yylval->string, yytext, FF_MAX_STRING - 1); return BIDIR_AND; }
"src or dst"|"dst or src"           { strncpy(yylval->string, yytext, FF_MAX_STRING - 1); return BIDIR_OR; }
"in src"|"in dst"|"out src"|"out dst"  { strncpy(yylval->string, yytext, FF_MAX_STRING - 1); return DIR_DIR_MAC; }
src|prev|in                         { strncpy(yylval->string, yytext, FF_MAX_STRING - 1); return DIR; }
dst|next|out                        { strncpy(yylval->string, yytext, FF_MAX_STRING - 1); return DIR; }

({letter}|{digit})+	{
					strncpy(yylval->string, yytext, FF_MAX_STRING - 1);
					return STRING;
				}

#.*         { ; }	//comments
{ws}+       { ; }	//whitespace
[\n]        { ; }	//end of line

.			//	v2_error(yyscanner, NULL, "invalid character");

%%

