#ifndef CALC_H
#define CALC_H

#include "ast.h"

/* Runs one parsed /calc statement (a bare expression, or an
   "identifier = expression" assignment). Prints the numeric
   result (or stores the assignment silently), and, if show_tac
   is set, prints the generated Three-Address Code first. */
void calc_run(ASTNode *stmt, int show_tac);

#endif
