#ifndef CODEGEN_H
#define CODEGEN_H

#include "ast.h"

/* Generates a standalone C program equivalent to one /calc
 * statement (output/generated.c), compiles it with the system
 * C compiler, runs it, and prints what it printed. This is the
 * "generate executable code and demonstrate executable
 * generation" requirement made concrete: MathScript's own
 * evaluator (eval.c) already computed the answer -- this proves
 * an independent, compiled program agrees with it.
 *
 * Returns 1 on success, 0 if generation/compilation/run failed
 * (a message is printed either way).
 */
int codegen_run(ASTNode *stmt);

#endif
