#ifndef GRAPH_EXPORT_H
#define GRAPH_EXPORT_H
#include "graph.h"
/* Validated expressions only. Archives are immutable; latest is replaced atomically. */
int graph_export_view(int dimensions,ASTNode *lhs,ASTNode *rhs,char names[][64],
                      const GraphSettings *settings,const char *kind,
                      const char *rotation_axis,int rotation_direction);
#endif
