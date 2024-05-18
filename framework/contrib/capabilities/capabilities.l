%option noyywrap

%{
#include <stdio.h>

#define YY_DECL int yylex()

#include "capabilities.tab.h"

%}

%%

[ \t]	; // ignore all whitespace
[0-9]+(\.[0-9]+)+   {yylval.fval = atof(yytext); return T_VERSION;}
[0-9]+		    {yylval.ival = atoi(yytext); return T_INT;}
[a-z][a-z0-9]*	    {yylval.ival = atoi(yytext); return T_STRING;}
"<="		    {return T_LESS;}
">="		    {return T_GREATER;}
"<"		    {return T_LESS;}
">"		    {return T_GREATER;}
"!="		    {return T_NOTEQUAL;}
"="		    {return T_EQUAL;}
"!"		    {return T_NOT;}
"("		    {return T_LEFT;}
")"		    {return T_RIGHT;}

%%
