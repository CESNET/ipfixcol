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

%option outfile="lex.yy.c"
%option prefix="ff2_"
%option reentrant bison-bridge
%option noyywrap 
%option nounput
%option noinput

digit 	[0-9]
letter	[A-Za-z.:/]
ws		[ \t]


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
"="|"=="|eq		{ return EQ; }
">"|gt			{ return GT; }
"<"|lt			{ return LT; }
"("				{ return LP; }
")"				{ return RP; }

"src"{ws}+"ip"	{
					strncpy(yylval->string, "srcip", FF_MAX_STRING - 1);
					return STRING;
				}
"src"{ws}+"net"	{
					strncpy(yylval->string, "srcip", FF_MAX_STRING - 1);
					return STRING;
				}
"src"{ws}+"port" {
					strncpy(yylval->string, "srcport", FF_MAX_STRING - 1);
					return STRING;
				}
({letter}|{digit})+	{
					strncpy(yylval->string, yytext, FF_MAX_STRING - 1);
					return STRING;
				}
#.*             { ; }	//comments
{ws}+			{ ; }	//whitespace 
[\n]			{ ; }	//end of line 

.			//	v2_error(yyscanner, NULL, "invalid character");

%%

