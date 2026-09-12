#ifndef IR_H
#define IR_H

#include "ast.h"

#define TAC_MAX_LINES 256
#define TAC_LINE_LEN  160

/* A TAC (Three-Address Code) program is just a numbered list of
   text lines such as:
       t1 = 5 + 3
       t2 = t1 * 2
       RESULT = t2
   That is enough detail for demonstration and for code
   generation (codegen.c), and it is exactly the IR shown in the
   project proposal / architecture slides. */
typedef struct {
    char lines[TAC_MAX_LINES][TAC_LINE_LEN];
    int  count;
    int  next_temp;
} TACProgram;

void tac_init(TACProgram *p);
void tac_print(const TACProgram *p);

/* Walks an expression AST, emitting TAC lines into p, and returns
   the operand (a literal, a variable name, or a fresh "tN") that
   holds the expression's final value. The returned pointer is
   only valid until the next call (small static buffer pool). */
const char *tac_build(TACProgram *p, ASTNode *n);

/* High level helpers matching the three modes' IR shape from the
   architecture design (see docs/GRAMMAR.md, section "IR / TAC"). */
void tac_finish_calc(TACProgram *p, const char *operand);                 /* RESULT = operand           */
void tac_finish_assign(TACProgram *p, const char *var, const char *val);  /* var = val                   */
void tac_finish_graph(TACProgram *p, const char *operand, const char *var_name); /* RETURN operand; PLOT var ... */
void tac_finish_eqn(TACProgram *p, const char *operand);                  /* EQUATION operand = 0; SOLVE*/

#endif
