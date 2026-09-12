#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "rootfind.h"
#include "eval.h"
#include "symtab.h"
#include "util.h"

/* f(var_name) = lhs(...) - rhs(...), evaluated with var_name set to
 * x in the symbol table. Quiet: never prints, only reports success
 * via *ok, so a scan can skip a single out-of-domain sample instead
 * of aborting. */
static double difference_at(ASTNode *lhs, ASTNode *rhs, const char *var_name, double x, int *ok) {
    symtab_set(var_name, x);
    double a = eval(lhs, 1, ok);
    if (!*ok) return 0;
    double b = eval(rhs, 1, ok);
    if (!*ok) return 0;
    return a - b;
}

static int add_root(double *roots, int n, double r) {
    for (int i = 0; i < n; i++)
        if (fabs(roots[i] - r) < 1e-6) return n; /* duplicate */
    if (n < ROOTFIND_MAX) roots[n++] = r;
    return n;
}

static int cmp_double(const void *a, const void *b) {
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

/* Bisection scan: sweeps [lo, hi] for sign changes and refines each
 * bracket. Works for any continuous equation, not just polynomials --
 * the numerical fallback for equations that are not simple linear or
 * quadratic forms in var_name. */
static int bisection_scan(ASTNode *lhs, ASTNode *rhs, const char *var_name, double lo, double hi,
                           int samples, double *roots) {
    int n = 0;
    int have_prev = 0;
    double prev_x = 0, prev_f = 0;
    double step = (hi - lo) / samples;

    for (int i = 0; i <= samples; i++) {
        double x = lo + i * step;
        int ok = 1;
        double f = difference_at(lhs, rhs, var_name, x, &ok);
        if (!ok) { have_prev = 0; continue; }

        if (fabs(f) < 1e-7) {
            n = add_root(roots, n, x);
        } else if (have_prev && prev_f * f < 0) {
            double a = prev_x, b = x;
            for (int iter = 0; iter < 60; iter++) {
                double mid = (a + b) / 2;
                int mok = 1;
                double fm = difference_at(lhs, rhs, var_name, mid, &mok);
                if (!mok) break;
                if (fabs(fm) < 1e-12) { a = b = mid; break; }
                double fa;
                int aok = 1;
                fa = difference_at(lhs, rhs, var_name, a, &aok);
                if (aok && fa * fm < 0) b = mid; else a = mid;
            }
            n = add_root(roots, n, (a + b) / 2);
        }
        prev_x = x; prev_f = f; have_prev = 1;
    }
    return n;
}

int rootfind_solve(ASTNode *lhs, ASTNode *rhs, const char *var_name,
                    double lo, double hi, int scan_samples,
                    double *roots, int *ok) {
    *ok = 1;

    /* --- Try an exact quadratic/linear fit first, by sampling at
       four points and checking whether a parabola through the first
       three predicts the fourth (an identity for any true degree<=2
       polynomial, and a cheap way to rule out anything else). --- */
    double f0  = difference_at(lhs, rhs, var_name, 0.0, ok);
    double f1  = *ok ? difference_at(lhs, rhs, var_name, 1.0, ok) : 0;
    double fm1 = *ok ? difference_at(lhs, rhs, var_name, -1.0, ok) : 0;
    double f2  = *ok ? difference_at(lhs, rhs, var_name, 2.0, ok) : 0;

    if (!*ok) return 0;

    int nroots = 0;
    int used_closed_form = 0;

    double a = (f1 + fm1 - 2 * f0) / 2.0;
    double b = (f1 - fm1) / 2.0;
    double c = f0;
    double predicted_at_2 = a * 4 + b * 2 + c;

    if (fabs(predicted_at_2 - f2) < 1e-6) {
        used_closed_form = 1;
        /* Exact roots -- NOT clamped to [lo, hi]: that window is only
           the bisection fallback's scan range below. A caller that
           only wants roots inside its own display window (graph.c)
           filters the returned roots itself. */
        if (fabs(a) < 1e-9) {
            if (fabs(b) >= 1e-9) {
                nroots = add_root(roots, nroots, -c / b);
            }
            /* a==0, b==0: either "always true" (c==0) or "never true" --
               neither has a finite root to plot/report. */
        } else {
            double disc = b * b - 4 * a * c;
            if (disc > 1e-9) {
                double sq = sqrt(disc);
                nroots = add_root(roots, nroots, (-b - sq) / (2 * a));
                nroots = add_root(roots, nroots, (-b + sq) / (2 * a));
            } else if (disc > -1e-9) {
                nroots = add_root(roots, nroots, -b / (2 * a));
            }
        }
    }

    if (!used_closed_form) {
        nroots = bisection_scan(lhs, rhs, var_name, lo, hi, scan_samples, roots);
    }

    qsort(roots, nroots, sizeof(double), cmp_double);
    return nroots;
}
