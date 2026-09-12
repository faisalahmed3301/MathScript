#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "solver.h"
#include "ir.h"
#include "eval.h"
#include "symtab.h"
#include "errors.h"
#include "util.h"

/* f(x) = lhs(x) - rhs(x). ok is set to 0 if evaluating either
 * side fails (undefined name, out-of-domain math, etc.). Always
 * called "quiet" (no error printed per sample) -- the caller has
 * already validated the equation once, loudly, before scanning. */
static double difference_at(ASTNode *lhs, ASTNode *rhs, double x, int *ok) {
    symtab_set("x", x);
    double a = eval(lhs, 1, ok);
    if (!*ok) return 0;
    double b = eval(rhs, 1, ok);
    if (!*ok) return 0;
    return a - b;
}

#define MAX_ROOTS 32

static int add_root(double *roots, int n, double r) {
    for (int i = 0; i < n; i++)
        if (fabs(roots[i] - r) < 1e-6) return n; /* duplicate */
    if (n < MAX_ROOTS) roots[n++] = r;
    return n;
}

static int cmp_double(const void *a, const void *b) {
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

/* Bisection fallback: scans [lo, hi] for sign changes and refines
 * each bracket. Works for any continuous equation, not just
 * polynomials -- this is the "numerical method" mentioned as a
 * fallback in the architecture design for equations that are not
 * simple linear/quadratic forms. */
static int bisection_scan(ASTNode *lhs, ASTNode *rhs, double lo, double hi,
                           int samples, double *roots) {
    int n = 0;
    int have_prev = 0;
    double prev_x = 0, prev_f = 0;
    double step = (hi - lo) / samples;

    for (int i = 0; i <= samples; i++) {
        double x = lo + i * step;
        int ok = 1;
        double f = difference_at(lhs, rhs, x, &ok);
        if (!ok) { have_prev = 0; continue; }

        if (fabs(f) < 1e-7) {
            n = add_root(roots, n, x);
        } else if (have_prev && prev_f * f < 0) {
            double a = prev_x, b = x;
            for (int iter = 0; iter < 60; iter++) {
                double mid = (a + b) / 2;
                int mok = 1;
                double fm = difference_at(lhs, rhs, mid, &mok);
                if (!mok) break;
                if (fabs(fm) < 1e-12) { a = b = mid; break; }
                double fa;
                int aok = 1;
                fa = difference_at(lhs, rhs, a, &aok);
                if (aok && fa * fm < 0) b = mid; else a = mid;
            }
            n = add_root(roots, n, (a + b) / 2);
        }
        prev_x = x; prev_f = f; have_prev = 1;
    }
    return n;
}

void solver_run(ASTNode *stmt, int show_tac) {
    if (stmt->kind != N_BINOP || stmt->op != '=') {
        semantic_error("/eqn mode expects an equation, e.g. x^2 - 5*x + 6 = 0");
        return;
    }
    ASTNode *lhs = stmt->left;
    ASTNode *rhs = stmt->right;

    double saved_x; int had_x = symtab_lookup("x", &saved_x);

    /* Validate once, loudly, so unknown functions/variables are
       reported clearly instead of silently skipped during scanning. */
    int ok = 1;
    difference_at(lhs, rhs, 0.0, &ok);
    if (!ok) { if (had_x) symtab_set("x", saved_x); return; }

    /* --- IR: EQUATION lhs - rhs = 0 ; SOLVE ------------------- */
    TACProgram tac;
    tac_init(&tac);
    ASTNode *diff = ast_binop('-', lhs, rhs); /* shallow wrapper, not owning lhs/rhs */
    const char *operand = tac_build(&tac, diff);
    tac_finish_eqn(&tac, operand);
    free(diff);
    if (show_tac) { printf("IR (TAC):\n"); tac_print(&tac); }

    /* --- Try an exact quadratic/linear fit first -------------- */
    int fit_ok = 1;
    double f0  = difference_at(lhs, rhs, 0.0, &fit_ok);
    double f1  = fit_ok ? difference_at(lhs, rhs, 1.0, &fit_ok) : 0;
    double fm1 = fit_ok ? difference_at(lhs, rhs, -1.0, &fit_ok) : 0;
    double f2  = fit_ok ? difference_at(lhs, rhs, 2.0, &fit_ok) : 0;

    double roots[MAX_ROOTS];
    int nroots = 0;
    int used_closed_form = 0;

    if (fit_ok) {
        double a = (f1 + fm1 - 2 * f0) / 2.0;
        double b = (f1 - fm1) / 2.0;
        double c = f0;
        double predicted_at_2 = a * 4 + b * 2 + c;

        if (fabs(predicted_at_2 - f2) < 1e-6) {
            used_closed_form = 1;
            if (fabs(a) < 1e-9) {
                if (fabs(b) < 1e-9) {
                    if (fabs(c) < 1e-9) printf("Infinitely many solutions (0 = 0).\n");
                    else printf("No solution.\n");
                } else {
                    nroots = add_root(roots, nroots, -c / b);
                }
            } else {
                double disc = b * b - 4 * a * c;
                if (disc > 1e-9) {
                    double sq = sqrt(disc);
                    nroots = add_root(roots, nroots, (-b - sq) / (2 * a));
                    nroots = add_root(roots, nroots, (-b + sq) / (2 * a));
                } else if (disc > -1e-9) {
                    nroots = add_root(roots, nroots, -b / (2 * a));
                } else {
                    printf("No real roots (discriminant = %s).\n", format_number(disc));
                }
            }
        }
    }

    if (!used_closed_form) {
        /* Not a plain linear/quadratic -- fall back to a numeric
           bisection scan over a generous default range. */
        nroots = bisection_scan(lhs, rhs, -25.0, 25.0, 5000, roots);
    }

    if (had_x) symtab_set("x", saved_x);

    if (nroots == 0) {
        printf("No roots found in the search range [-25, 25].\n");
        return;
    }

    qsort(roots, nroots, sizeof(double), cmp_double);
    if (nroots == 1) {
        printf("Root:\n");
    } else {
        printf("Roots:\n");
    }
    for (int i = 0; i < nroots; i++)
        printf("x = %s\n", format_number(roots[i]));
}
