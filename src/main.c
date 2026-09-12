/* ============================================================
   main.c -- MathScript REPL / script driver

   Ties the whole pipeline together for one line of input:

       text  --[Flex]--> tokens --[Bison]--> AST
             --[semantic checks, inline in eval.c]--
             --[ir.c: AST -> TAC]--
             --[calc.c / graph.c / solver.c: mode backend]--> output

   Usage:
       mathscript                 interactive REPL
       mathscript path/to/file.ms run a MathScript script file
   ============================================================ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "symtab.h"
#include "errors.h"
#include "calc.h"
#include "graph.h"
#include "solver.h"
#include "codegen.h"

/* From the Bison/Flex generated files */
extern int yyparse(void);
extern ASTNode *g_parse_result;
extern int g_mode_switch;
typedef struct yy_buffer_state *YY_BUFFER_STATE;
extern YY_BUFFER_STATE yy_scan_string(const char *str);
extern void yy_delete_buffer(YY_BUFFER_STATE buffer);

#define MODE_NONE  0
#define MODE_CALC  1
#define MODE_GRAPH 2
#define MODE_EQN   3

static int  g_mode = MODE_NONE;
static int  g_show_tac = 1;

static const char *GRAMMAR_TEXT =
"MathScript grammar (BNF) -- this is the ACTUAL grammar parser.y hands to Bison.\n"
"\n"
"  input       -> CALC_MODE | GRAPH_MODE | EQN_MODE\n"
"               |  statement\n"
"\n"
"  statement   -> expr\n"
"               |  expr '=' expr\n"
"\n"
"  expr        -> expr '+' expr | expr '-' expr\n"
"               |  expr '*' expr | expr '/' expr\n"
"               |  expr '^' expr\n"
"               |  expr '<' expr | expr '>' expr\n"
"               |  expr '>=' expr | expr '<=' expr\n"
"               |  expr '==' expr | expr '!=' expr\n"
"               |  '-' expr                      (unary minus)\n"
"               |  '(' expr ')'\n"
"               |  NUMBER\n"
"               |  IDENTIFIER\n"
"               |  IDENTIFIER '(' arglist ')'     (function call)\n"
"               |  NUMBER IDENTIFIER              (implicit multiply: \"3x\")\n"
"               |  NUMBER '(' expr ')'            (implicit multiply: \"3(x+1)\")\n"
"\n"
"  arglist     -> (empty) | expr | arglist ',' expr\n"
"\n"
"The SAME grammar is used for /calc, /graph and /eqn. The active\n"
"mode only changes how main.c interprets an already-parsed\n"
"statement (see calc.c, graph.c, solver.c). /graph in particular\n"
"accepts any 'expr = expr': \"y = f(x)\" plots a curve, an equation\n"
"in one other variable (e.g. \"3*x = 1\") is solved and shown on a\n"
"number line, and an equation in two variables (e.g. \"x^2+y^2=25\")\n"
"is plotted as an implicit curve. See src/parser.y.\n";

static const char *PRECEDENCE_TEXT =
"Operator precedence (as declared in parser.y), LOWEST to HIGHEST:\n"
"\n"
"  1. <  >  >=  <=  ==  !=      (comparisons)          left-assoc\n"
"  2. +  -                      (add, subtract)        left-assoc\n"
"  3. *  /                      (multiply, divide)     left-assoc\n"
"  4. unary minus  (-a)                                 --\n"
"  5. ^                         (power)                right-assoc\n"
"\n"
"Bison resolves an ambiguous grammar using this table instead of a\n"
"pile of extra nonterminals, which is what keeps the grammar above\n"
"so short. Two worked examples:\n"
"\n"
"  2 + 3 * 4\n"
"    '*' binds tighter than '+', so this is 2 + (3 * 4) = 14,\n"
"    not (2 + 3) * 4 = 20.\n"
"\n"
"  -2 ^ 2\n"
"    '^' binds TIGHTER than unary minus (rule 5 is higher than\n"
"    rule 4), so this is -(2 ^ 2) = -4, matching standard math\n"
"    and languages such as Python's ** operator -- not (-2) ^ 2 = 4.\n"
"\n"
"  2 ^ 3 ^ 2\n"
"    '^' is %right, so this is 2 ^ (3 ^ 2) = 2 ^ 9 = 512, evaluated\n"
"    right-to-left instead of left-to-right.\n"
"\n"
"Full worked derivations: docs/OPERATOR_PRECEDENCE.md\n";

static const char *HELP_TEXT =
"MathScript commands:\n"
"  /calc            enter calculation mode\n"
"  /graph           enter graph mode\n"
"  /eqn             solve in x, y, z, etc.; degree 1-3 includes complex roots\n"
"  /help            show this help\n"
"  /grammar         print the MathScript grammar (BNF)\n"
"  /precedence      print the operator precedence table + explanation\n"
"  /symbols         print the current symbol table\n"
"  /tac on|off      toggle printing generated IR/TAC (default: on)\n"
"  /exit            quit MathScript\n"
"\n"
"Inside a mode:\n"
"  <expression>       evaluate / plot / solve, depending on the mode\n"
"  gen <expression>   (calc mode only) also generate + compile + run\n"
"                     equivalent C code, to demonstrate code generation\n"
"  exit               leave the current mode\n"
"\n"
"/graph accepts more than just \"y = ...\":\n"
"  y = x^2            a curve: y as a function of one variable\n"
"  y^2 or y^3         a sideways curve: shorthand for x = f(y)\n"
"  pow(y,2), pow(y,3) equivalent power-function syntax\n"
"  y^2 = x            an implicit curve with both real branches\n"
"  3*x = 1  (or 3x=1) an equation in one variable -> solved, shown on a number line\n"
"  x^2 + y^2 = 25     an equation in two variables -> plotted as an implicit curve\n";

static const char *mode_prompt(void) {
    switch (g_mode) {
        case MODE_CALC:  return "calc> ";
        case MODE_GRAPH: return "graph> ";
        case MODE_EQN:   return "eqn> ";
        default:         return "> ";
    }
}

static char *trim(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    char *end = s + strlen(s);
    while (end > s && (end[-1] == '\n' || end[-1] == '\r' || end[-1] == ' ' || end[-1] == '\t'))
        *--end = '\0';
    return s;
}

/* Parses one line of MathScript text. Returns 1 if a statement
 * (not a mode switch) was produced in g_parse_result. */
static int parse_line(const char *line) {
    errors_reset();
    g_parse_result = NULL;
    g_mode_switch = 0;
    YY_BUFFER_STATE buf = yy_scan_string(line);
    yyparse();
    yy_delete_buffer(buf);
    return g_parse_result != NULL;
}

static void process_line(char *line) {
    line = trim(line);
    if (line[0] == '\0' || line[0] == '#') return;

    if (strcmp(line, "/help") == 0)      { printf("%s", HELP_TEXT); return; }
    if (strcmp(line, "/grammar") == 0)   { printf("%s", GRAMMAR_TEXT); return; }
    if (strcmp(line, "/precedence") == 0){ printf("%s", PRECEDENCE_TEXT); return; }
    if (strcmp(line, "/symbols") == 0)   { symtab_dump(); return; }
    if (strcmp(line, "/tac on") == 0)    { g_show_tac = 1; printf("IR/TAC printing: on\n"); return; }
    if (strcmp(line, "/tac off") == 0)   { g_show_tac = 0; printf("IR/TAC printing: off\n"); return; }
    if (strcmp(line, "/exit") == 0)      { printf("Goodbye.\n"); exit(0); }

    if (strcmp(line, "exit") == 0) {
        if (g_mode == MODE_NONE) {
            printf("Not inside a mode. Use /exit to quit MathScript.\n");
        } else {
            const char *name = g_mode == MODE_CALC ? "CALC" : g_mode == MODE_GRAPH ? "GRAPH" : "EQN";
            printf("[Leaving %s mode]\n", name);
            g_mode = MODE_NONE;
        }
        return;
    }

    int also_codegen = 0;
    if (g_mode == MODE_CALC && strncmp(line, "gen ", 4) == 0) {
        also_codegen = 1;
        line += 4;
        line = trim(line);
    }

    if (!parse_line(line)) {
        if (g_mode_switch) {
            g_mode = g_mode_switch;
            const char *name = g_mode == MODE_CALC ? "CALC" : g_mode == MODE_GRAPH ? "GRAPH" : "EQN";
            printf("[%s mode activated]\n", name);
        }
        /* else: lexical/syntax error already reported by errors.c */
        return;
    }

    if (errors_any()) {
        /* A lexical error part-way through the line still lets
           Bison see a "complete" parse of the text before it (e.g.
           "10" out of "10 @ 5") -- discard that partial result so
           a bad line never silently produces an answer. */
        ast_free(g_parse_result);
        return;
    }

    if (g_mode == MODE_NONE) {
        printf("No mode active. Type /calc, /graph, or /eqn first.\n");
        ast_free(g_parse_result);
        return;
    }

    switch (g_mode) {
        case MODE_CALC:
            calc_run(g_parse_result, g_show_tac);
            if (also_codegen && !errors_any()) codegen_run(g_parse_result);
            break;
        case MODE_GRAPH:
            graph_run(g_parse_result, g_show_tac);
            break;
        case MODE_EQN:
            solver_run(g_parse_result, g_show_tac);
            break;
    }
    ast_free(g_parse_result);
}

static void print_banner(void) {
    printf("========================================\n");
    printf("           MathScript Compiler\n");
    printf("========================================\n\n");
    printf("Commands:\n  /calc\n  /graph\n  /eqn\n  /help\n  /exit\n\n");
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IOLBF, 0); /* line-buffered even when piped/redirected */
    symtab_init();

    if (argc > 1) {
        FILE *fp = fopen(argv[1], "r");
        if (!fp) { fprintf(stderr, "Cannot open script file '%s'\n", argv[1]); return 1; }
        print_banner();
        char line[1024];
        while (fgets(line, sizeof(line), fp)) {
            char *shown = trim(strdup(line));
            if (shown[0] != '\0' && shown[0] != '#')
                printf("%s%s\n", mode_prompt(), shown);
            free(shown);
            process_line(line);
        }
        fclose(fp);
        return 0;
    }

    print_banner();
    char line[1024];
    for (;;) {
        printf("%s", mode_prompt());
        fflush(stdout);
        if (!fgets(line, sizeof(line), stdin)) { printf("\nGoodbye.\n"); break; }
        process_line(line);
    }
    return 0;
}
