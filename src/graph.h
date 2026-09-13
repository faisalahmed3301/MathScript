#ifndef GRAPH_H
#define GRAPH_H
#include "ast.h"
typedef struct
{
    double lo[3], hi[3];
    int samples;
} GraphSettings;
extern GraphSettings graph2d_settings, graph3d_settings;
/* /range and /samples update only the active graph mode. */
int graph_command(const char *line, int dimensions);
void graph_run(ASTNode *stmt, int show_tac);
void graph3d_run(ASTNode *stmt, int show_tac);
/* Three coordinate subdivisions per unit, coalesced at display resolution. */
int graph_axis_marks(double lo, double hi, int capacity, double *values);
int graph_free_vars(ASTNode *lhs, ASTNode *rhs, char names[][64]);
#endif
