/* ============================================================
   parser.y -- MathScript grammar (Bison)

   This is the WHOLE language grammar. It is intentionally tiny:

       input      -> CALC_MODE | GRAPH_MODE | EQN_MODE
                   |  statement

       statement  -> expr
                   |  expr '=' expr

       expr       -> expr '+' expr | expr '-' expr
                   |  expr '*' expr | expr '/' expr
                   |  expr '^' expr
                   |  expr '<' expr | expr '>' expr
                   |  expr GE expr  | expr LE expr
                   |  expr EQ expr  | expr NE expr
                   |  '-' expr                      (unary minus)
                   |  '(' expr ')'
                   |  NUMBER
                   |  IDENTIFIER
                   |  IDENTIFIER '(' arglist ')'     (function call)

   The SAME grammar parses /calc, /graph and /eqn input -- the
   REPL (main.c) decides what a parsed line MEANS once it knows
   which mode is currently active. See docs/GRAMMAR.md and the
   /grammar command for the exact BNF, and docs/OPERATOR_PRECEDENCE.md
   for a worked explanation of the %left/%right table below.
   ============================================================ */

%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "errors.h"

extern int yylex(void);
extern int yylineno;

ASTNode *g_parse_result = NULL;   /* AST of a parsed statement       */
int      g_mode_switch  = 0;     /* 1=calc 2=graph 3=eqn, 0=none    */

void yyerror(const char *msg) {
    if (!g_lex_error)             /* lexer already explained the problem */
        syntax_error("%s", msg);
}
%}

%define parse.error verbose

%union {
    double   dval;
    char    *sval;
    ASTNode *node;
    struct { ASTNode **items; int count; } arglist;
}

%token <dval> NUMBER
%token <sval> IDENTIFIER
%token CALC_MODE GRAPH_MODE EQN_MODE
%token GE LE EQ NE

%type <node> expr statement
%type <arglist> arglist

/* Lowest precedence first, highest precedence last: this is what
   makes "2 + 3 * 4" parse as "2 + (3 * 4)" instead of left-to-right. */
%left '<' '>' GE LE EQ NE
%left '+' '-'
%left '*' '/'
%precedence UMINUS
%right '^'

%%

input
    : CALC_MODE          { g_mode_switch = 1; }
    | GRAPH_MODE         { g_mode_switch = 2; }
    | EQN_MODE           { g_mode_switch = 3; }
    | statement          { g_parse_result = $1; }
    ;

statement
    : expr               { $$ = $1; }
    | expr '=' expr      { $$ = ast_binop('=', $1, $3); }
    ;

expr
    : expr '+' expr      { $$ = ast_binop('+', $1, $3); }
    | expr '-' expr      { $$ = ast_binop('-', $1, $3); }
    | expr '*' expr      { $$ = ast_binop('*', $1, $3); }
    | expr '/' expr      { $$ = ast_binop('/', $1, $3); }
    | expr '^' expr      { $$ = ast_binop('^', $1, $3); }
    | expr '<' expr      { $$ = ast_binop('<', $1, $3); }
    | expr '>' expr      { $$ = ast_binop('>', $1, $3); }
    | expr GE expr       { $$ = ast_binop(OP_GE, $1, $3); }
    | expr LE expr       { $$ = ast_binop(OP_LE, $1, $3); }
    | expr EQ expr       { $$ = ast_binop(OP_EQ, $1, $3); }
    | expr NE expr       { $$ = ast_binop(OP_NE, $1, $3); }
    | '-' expr %prec UMINUS   { $$ = ast_uminus($2); }
    | '(' expr ')'       { $$ = $2; }
    | NUMBER             { $$ = ast_num($1); }
    | IDENTIFIER         { $$ = ast_var($1); free($1); }
    | IDENTIFIER '(' arglist ')' {
                             $$ = ast_call($1, $3.items, $3.count);
                             free($1);
                         }
    ;

arglist
    : /* empty */        { $$.items = NULL; $$.count = 0; }
    | expr                {
                             $$.items = malloc(sizeof(ASTNode *));
                             $$.items[0] = $1;
                             $$.count = 1;
                         }
    | arglist ',' expr    {
                             $$.items = realloc($1.items, sizeof(ASTNode *) * ($1.count + 1));
                             $$.items[$1.count] = $3;
                             $$.count = $1.count + 1;
                         }
    ;

%%
