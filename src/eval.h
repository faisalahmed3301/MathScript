#ifndef EVAL_H
#define EVAL_H

#include "ast.h"

/* Evaluates an expression AST (no '=' node) to a double.
 *
 * quiet = 0 : a math error (e.g. division by zero, sqrt of a
 *             negative number) is printed via math_error().
 * quiet = 1 : the same failure is reported only through *ok,
 *             nothing is printed. /graph2d uses this so that one
 *             out-of-domain sample point does not flood the
 *             screen with a repeated error message.
 *
 * *ok is set to 0 on ANY failure (lexical/syntax already excluded
 * by this point; semantic errors -- unknown name/function/arity --
 * are always printed, quiet only affects math errors).
 */
double eval(ASTNode *n, int quiet, int *ok);

/* Checks names/functions/arity without evaluating domain-sensitive math. */
int eval_validate(ASTNode *n);

#endif
