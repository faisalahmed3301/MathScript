#ifndef SOLVER_H
#define SOLVER_H

#include "ast.h"

/* Runs one parsed /eqn statement: stmt must be "expr = expr".
 * Solves for x using a closed-form quadratic formula when the
 * equation fits a degree <= 2 polynomial (detected by sampling,
 * see solver.c), and falls back to a bisection scan otherwise --
 * matching docs/GRAMMAR.md section on the /eqn back-end. */
void solver_run(ASTNode *stmt, int show_tac);

#endif
