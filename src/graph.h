#ifndef GRAPH_H
#define GRAPH_H

#include "ast.h"

/* Runs one parsed /graph statement (any "expr = expr"). Handles
 * three shapes, dispatched by how many free variables appear:
 *   y = f(x)          -- the common case: a curve, y vs. one variable
 *   f(x) = c            (1 free var, no 'y')  -- solved and shown as
 *                        point(s) on a number line
 *   f(x, y) = g(x, y)    (2 free vars)  -- an implicit relation (e.g.
 *                        a line or circle), solved for the second
 *                        variable at each sampled value of the first
 * Renders ASCII art in the terminal (portable, no GUI dependency),
 * which matches the "X-axis, Y-axis, grid, curve" requirement from
 * the language design without needing a graphics library on either
 * Windows or macOS. */
void graph_run(ASTNode *stmt, int show_tac);

#endif
