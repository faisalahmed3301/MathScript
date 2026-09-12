#ifndef POLY_H
#define POLY_H
#include "ast.h"

/* Coefficients in ascending powers; extraction recognizes arithmetic,
 * constant divisors, and nonnegative integer ^ / pow exponents up to 3. */
typedef struct { long double c[4]; } Polynomial;
typedef struct { long double real, imag; } PolyRoot;
int poly_extract(ASTNode *n, const char *variable, Polynomial *out);
/* Returns degree roots, including multiplicity. Constant equations return 0. */
int poly_solve(const Polynomial *p, PolyRoot roots[3]);
#endif
