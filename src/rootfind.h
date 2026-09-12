#ifndef ROOTFIND_H
#define ROOTFIND_H

#include "ast.h"

#define ROOTFIND_MAX 32

/* Shared numeric core behind /eqn (solver.c) and implicit-equation
 * plotting (graph.c): finds every real value of var_name in
 * [lo, hi] that makes lhs(var_name) == rhs(var_name), holding any
 * OTHER free variable at whatever value is already in the symbol
 * table (this is what lets graph.c fix x and solve for y one
 * sample at a time, for equations like "3*x + 2*y = 6").
 *
 * Tries an exact closed-form fit for degree <= 2 polynomials in
 * var_name first (see rootfind.c), then falls back to a bisection
 * scan with `scan_samples` steps across [lo, hi] -- more samples
 * finds close-together roots more reliably but costs more
 * evaluations, so callers that solve many times (once per pixel
 * column) should pass a smaller count than a one-shot /eqn solve.
 *
 * Returns the number of distinct real roots found (0..ROOTFIND_MAX),
 * written into roots[] in ascending order. *ok is set to 0 (and 0
 * returned) if evaluating the equation at var_name = lo fails --
 * e.g. an undefined name or a function called with the wrong
 * number of arguments -- so the caller can report that once
 * instead of failing silently on every sample.
 */
int rootfind_solve(ASTNode *lhs, ASTNode *rhs, const char *var_name,
                    double lo, double hi, int scan_samples,
                    double *roots, int *ok);

#endif
