%option noyywrap nounput noinput

/* ============================================================
   lexer.l -- MathScript lexical analyzer (Flex)

   Turns one line of raw text into a stream of tokens for the
   Bison parser (parser.y). Every rule below matches one kind of
   lexeme; whichever rule matches the LONGEST piece of input wins
   (that is how Flex always resolves e.g. "/calc" vs "/").
   ============================================================ */

%{
#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "errors.h"
#include "parser.tab.h"

extern int g_lex_error;
%}

DIGIT   [0-9]
NUMBER  {DIGIT}+(\.{DIGIT}+)?
ID      [a-zA-Z_][a-zA-Z0-9_]*

%%

[ \t]+                 { /* skip whitespace */ }

"/calc"                 { return CALC_MODE; }
"/graph"                { return GRAPH_MODE; }
"/eqn"                  { return EQN_MODE; }

{NUMBER}                { yylval.dval = atof(yytext); return NUMBER; }
{ID}                    { yylval.sval = strdup(yytext); return IDENTIFIER; }

">="                    { return GE; }
"<="                    { return LE; }
"=="                    { return EQ; }
"!="                    { return NE; }

[-+*/^=<>(),]           { return yytext[0]; }

.                       { lex_error("Invalid character '%c'", yytext[0]); return 0; }

%%
