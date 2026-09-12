#ifndef GRAPH_H
#define GRAPH_H

#include "ast.h"

/* Runs one parsed /graph statement: stmt must be "y = expression".
 * Renders an ASCII-art plot in the terminal (portable, no GUI
 * dependency), which matches the "X-axis, Y-axis, grid, curve"
 * requirement from the language design without needing a
 * graphics library on either Windows or macOS. */
void graph_run(ASTNode *stmt, int show_tac);

#endif
