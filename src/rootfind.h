#ifndef ROOTFIND_H
#define ROOTFIND_H
#include "ast.h"
#define ROOTFIND_MAX 32
/* Graphs stream every detected root without a per-slice storage cap.
 * Other variables must already be bound. Exact polynomial extraction up
 * to degree 3; otherwise a finite scan with pole rejection and tangency
 * refinement. Identity slices emit the whole sampled line. Domain gaps
 * are skipped, and a caller must validate semantic errors beforehand.
 * The scanned variable is left bound to its last evaluated value. */
typedef void (*RootVisitor)(double root, void *context);
void rootfind_visit(ASTNode *lhs, ASTNode *rhs, const char *name,
                    double lo, double hi, int samples, RootVisitor visit, void *context);
/* Bounded result list for the equation solver. Exact polynomial roots
 * may lie outside [lo,hi]; numerical roots are restricted to the scan. */
int rootfind_solve(ASTNode *lhs, ASTNode *rhs, const char *var_name,
                   double lo, double hi, int scan_samples, double *roots, int *ok);
#endif
