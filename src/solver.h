#ifndef SOLVER_H
#define SOLVER_H

#include "ast.h"

/* Solves expr = expr for its single non-constant variable. Degree 1-3
 * polynomials return all real/complex roots, including multiplicity;
 * other expressions retain the numerical real-root fallback. */
void solver_run(ASTNode *stmt, int show_tac);

#endif
