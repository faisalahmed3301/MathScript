#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "graph.h"
#include "ir.h"
#include "eval.h"
#include "symtab.h"
#include "errors.h"
#include "util.h"
#include "rootfind.h"

#define GRAPH_WIDTH   61   /* columns sampled across the horizontal range */
#define GRAPH_HEIGHT  21   /* rows of the ASCII plot                       */
#define AXIS_LO      -10.0 /* horizontal (plotted-variable) range          */
#define AXIS_HI       10.0
/* The vertical range is NOT auto-fit tightly to each function's own
 * data -- that hid slope differences entirely (a gently-sloped line
 * and a steep one both got stretched to fill the same frame). It is
 * fixed to this window by default and only grows if the data does
 * not fit, so "y = 5*x" visibly fills the frame while "y = x" only
 * uses a fraction of it. */
#define Y_DEFAULT_LO -50.0
#define Y_DEFAULT_HI  50.0
#define MAX_PTS_PER_COL 4  /* an implicit curve (e.g. a circle) can have more than one y per x */

typedef struct {
    double y[MAX_PTS_PER_COL];
    int n;
} Column;

/* Shared ASCII renderer for every /graph shape: a plain function
 * y = f(x), a one-variable equation solved to a set of points, or a
 * two-variable implicit relation (0..MAX_PTS_PER_COL y's per column). */
static void render(const char *h_name, const Column *cols, double h_lo, double h_hi) {
    double ymin = Y_DEFAULT_LO, ymax = Y_DEFAULT_HI;
    int any = 0;
    for (int i = 0; i < GRAPH_WIDTH; i++)
        for (int j = 0; j < cols[i].n; j++) {
            any = 1;
            if (cols[i].y[j] < ymin) ymin = cols[i].y[j];
            if (cols[i].y[j] > ymax) ymax = cols[i].y[j];
        }
    if (!any) {
        math_error("no points of this equation lie in the plotted range");
        return;
    }

    static char canvas[GRAPH_HEIGHT][GRAPH_WIDTH + 1];
    for (int r = 0; r < GRAPH_HEIGHT; r++) {
        memset(canvas[r], ' ', GRAPH_WIDTH);
        canvas[r][GRAPH_WIDTH] = '\0';
    }

    int zero_row = -1;
    if (ymin <= 0 && ymax >= 0)
        zero_row = (int)round((ymax - 0) / (ymax - ymin) * (GRAPH_HEIGHT - 1));
    int zero_col = -1;
    if (h_lo <= 0 && h_hi >= 0)
        zero_col = (int)round((0 - h_lo) / (h_hi - h_lo) * (GRAPH_WIDTH - 1));

    if (zero_row >= 0) for (int c = 0; c < GRAPH_WIDTH; c++) if (canvas[zero_row][c] == ' ') canvas[zero_row][c] = '-';
    if (zero_col >= 0) for (int r = 0; r < GRAPH_HEIGHT; r++) if (canvas[r][zero_col] == ' ') canvas[r][zero_col] = '|';
    if (zero_row >= 0 && zero_col >= 0) canvas[zero_row][zero_col] = '+';

    for (int i = 0; i < GRAPH_WIDTH; i++)
        for (int j = 0; j < cols[i].n; j++) {
            int row = (int)round((ymax - cols[i].y[j]) / (ymax - ymin) * (GRAPH_HEIGHT - 1));
            if (row < 0) row = 0;
            if (row >= GRAPH_HEIGHT) row = GRAPH_HEIGHT - 1;
            canvas[row][i] = '.';
        }

    printf("\nPlot (%s from %g to %g, y from %s to %s):\n", h_name, h_lo, h_hi,
           format_number(ymin), format_number(ymax));
    for (int r = 0; r < GRAPH_HEIGHT; r++)
        printf("  %s\n", canvas[r]);
    printf("  (y-axis: %s=0 marked '|', x-axis: y=0 marked '-', curve marked '.')\n", h_name);
}

/* Finds the free variables (excluding constants pi/e) across an
 * equation's two sides. Returns how many distinct names were found,
 * writing up to 8 into names[][]. */
static int free_vars(ASTNode *lhs, ASTNode *rhs, char names[][64]) {
    char all[8][64];
    int all_count = 0;
    ast_collect_vars(lhs, all, 8, &all_count);
    ast_collect_vars(rhs, all, 8, &all_count);
    int n = 0;
    for (int i = 0; i < all_count; i++)
        if (!symtab_is_constant(all[i])) strncpy(names[n++], all[i], 63);
    return n;
}

/* --- y = f(var): the common case ------------------------------ */
static void graph_function(ASTNode *rhs, int show_tac) {
    char names[8][64];
    int name_count = 0;
    ast_collect_vars(rhs, names, 8, &name_count);
    int free_count = 0;
    char var_name[64] = "x";
    for (int i = 0; i < name_count; i++) {
        if (symtab_is_constant(names[i])) continue;
        if (free_count == 0) strncpy(var_name, names[i], 63);
        free_count++;
    }
    if (free_count > 1) {
        semantic_error("/graph supports exactly one free variable, found %d (e.g. '%s' and '%s')",
                        free_count, var_name, names[1]);
        return;
    }

    double saved; int had = symtab_lookup(var_name, &saved);
    symtab_set(var_name, 1.0);
    int ok = 1;
    eval(rhs, 0, &ok);
    if (!ok) { if (had) symtab_set(var_name, saved); else symtab_unset(var_name); return; }

    TACProgram tac;
    tac_init(&tac);
    const char *operand = tac_build(&tac, rhs);
    tac_finish_graph(&tac, operand, var_name);
    if (show_tac) { printf("IR (TAC):\n"); tac_print(&tac); }

    static Column cols[GRAPH_WIDTH];
    double xs[GRAPH_WIDTH];
    int skipped = 0;
    double step = (AXIS_HI - AXIS_LO) / (GRAPH_WIDTH - 1);

    for (int i = 0; i < GRAPH_WIDTH; i++) {
        double x = AXIS_LO + i * step;
        xs[i] = x;
        symtab_set(var_name, x);
        int pok = 1;
        double y = eval(rhs, 1, &pok);
        if (pok) { cols[i].y[0] = y; cols[i].n = 1; }
        else { cols[i].n = 0; skipped++; }
    }
    if (had) symtab_set(var_name, saved); else symtab_unset(var_name);

    printf("Graph generated.\n\n");
    printf("Sample points:\n");
    for (int i = 0; i < GRAPH_WIDTH; i += (GRAPH_WIDTH - 1) / 8) {
        if (cols[i].n)
            printf("  %s = %-6s -> y = %s\n", var_name, format_number(xs[i]), format_number(cols[i].y[0]));
        else
            printf("  %s = %-6s -> y = (undefined)\n", var_name, format_number(xs[i]));
    }

    render(var_name, cols, AXIS_LO, AXIS_HI);

    if (skipped > 0)
        printf("\n(%d of %d sample points were outside the real-valued domain and skipped.)\n",
               skipped, GRAPH_WIDTH);
}

/* --- an equation in exactly one variable, no dependent 'y' ------
 * e.g. "3*x = 1" or "x^2 = 4": solved like /eqn, then shown as
 * point(s) on a number line instead of a curve (there is no second
 * axis to plot against). */
static void graph_equation_1var(ASTNode *lhs, ASTNode *rhs, const char *var_name, int show_tac) {
    double saved; int had = symtab_lookup(var_name, &saved);
    symtab_set(var_name, 0.0);
    int ok = 1;
    eval(lhs, 0, &ok);
    if (ok) eval(rhs, 0, &ok);
    if (!ok) { if (had) symtab_set(var_name, saved); else symtab_unset(var_name); return; }

    TACProgram tac;
    tac_init(&tac);
    ASTNode *diff = ast_binop('-', lhs, rhs);
    const char *operand = tac_build(&tac, diff);
    tac_finish_eqn(&tac, operand);
    free(diff);
    if (show_tac) { printf("IR (TAC):\n"); tac_print(&tac); }

    double roots[ROOTFIND_MAX];
    int nroots = rootfind_solve(lhs, rhs, var_name, -25.0, 25.0, 5000, roots, &ok);
    if (had) symtab_set(var_name, saved); else symtab_unset(var_name);

    if (nroots == 0) {
        printf("No solution found for '%s' in the search range [-25, 25].\n", var_name);
        return;
    }
    printf("%s:\n", nroots == 1 ? "Root" : "Roots");
    for (int i = 0; i < nroots; i++) printf("%s = %s\n", var_name, format_number(roots[i]));

    char line[GRAPH_WIDTH + 1];
    memset(line, '-', GRAPH_WIDTH);
    line[GRAPH_WIDTH] = '\0';
    int zero_col = -1;
    if (AXIS_LO <= 0 && AXIS_HI >= 0) {
        zero_col = (int)round((0 - AXIS_LO) / (AXIS_HI - AXIS_LO) * (GRAPH_WIDTH - 1));
        line[zero_col] = '+';
    }
    int shown = 0, hidden = 0;
    for (int i = 0; i < nroots; i++) {
        if (roots[i] < AXIS_LO || roots[i] > AXIS_HI) { hidden++; continue; }
        int col = (int)round((roots[i] - AXIS_LO) / (AXIS_HI - AXIS_LO) * (GRAPH_WIDTH - 1));
        line[col] = '.';
        shown++;
    }
    printf("\nNumber line (%s from %g to %g, '+' marks %s=0, '.' marks a root):\n", var_name, AXIS_LO, AXIS_HI, var_name);
    printf("  %s\n", line);
    if (hidden > 0 && shown == 0)
        printf("(all %d root(s) fall outside [%g, %g] and are not shown above.)\n", hidden, AXIS_LO, AXIS_HI);
    else if (hidden > 0)
        printf("(%d of %d root(s) fall outside [%g, %g] and are not shown above.)\n", hidden, nroots, AXIS_LO, AXIS_HI);
}

/* --- an equation in exactly two variables ------------------------
 * e.g. "3*x + 2*y = 6" or "x^2 + y^2 = 25": for each sampled value
 * of the horizontal variable, solve the vertical one via the same
 * root-finder /eqn uses. A circle naturally yields two y's for most
 * x -- both are plotted, which is what makes this handle conics as
 * well as lines. */
static void graph_equation_2var(ASTNode *lhs, ASTNode *rhs, const char *h_name, const char *v_name, int show_tac) {
    double saved_h; int had_h = symtab_lookup(h_name, &saved_h);
    double saved_v; int had_v = symtab_lookup(v_name, &saved_v);

    symtab_set(h_name, 0.0);
    int ok = 1;
    double roots0[ROOTFIND_MAX];
    rootfind_solve(lhs, rhs, v_name, Y_DEFAULT_LO, Y_DEFAULT_HI, 400, roots0, &ok);
    if (!ok) {
        if (had_h) symtab_set(h_name, saved_h); else symtab_unset(h_name);
        if (had_v) symtab_set(v_name, saved_v); else symtab_unset(v_name);
        return;
    }

    TACProgram tac;
    tac_init(&tac);
    ASTNode *diff = ast_binop('-', lhs, rhs);
    const char *operand = tac_build(&tac, diff);
    tac_finish_eqn(&tac, operand);
    free(diff);
    if (show_tac) { printf("IR (TAC):\n"); tac_print(&tac); }

    printf("Graph generated (implicit relation in %s and %s).\n", h_name, v_name);

    static Column cols[GRAPH_WIDTH];
    double step = (AXIS_HI - AXIS_LO) / (GRAPH_WIDTH - 1);
    for (int i = 0; i < GRAPH_WIDTH; i++) {
        double h = AXIS_LO + i * step;
        symtab_set(h_name, h);
        double roots[ROOTFIND_MAX];
        int rok = 1;
        int n = rootfind_solve(lhs, rhs, v_name, Y_DEFAULT_LO, Y_DEFAULT_HI, 400, roots, &rok);
        cols[i].n = 0;
        if (rok) {
            for (int j = 0; j < n && cols[i].n < MAX_PTS_PER_COL; j++)
                cols[i].y[cols[i].n++] = roots[j];
        }
    }

    if (had_h) symtab_set(h_name, saved_h); else symtab_unset(h_name);
    if (had_v) symtab_set(v_name, saved_v); else symtab_unset(v_name);

    render(h_name, cols, AXIS_LO, AXIS_HI);
}

void graph_run(ASTNode *stmt, int show_tac) {
    if (stmt->kind != N_BINOP || stmt->op != '=') {
        semantic_error("/graph mode expects an equation, e.g. y = x^2, 3*x = 1, or x^2 + y^2 = 25");
        return;
    }
    ASTNode *lhs = stmt->left;
    ASTNode *rhs = stmt->right;

    /* The common, documented shape: an explicit function of one
       variable assigned to 'y'. Kept as its own fast path since it
       is the primary, best-tested feature. */
    if (lhs->kind == N_VAR && strcmp(lhs->name, "y") == 0) {
        graph_function(rhs, show_tac);
        return;
    }

    /* Otherwise: a general equation. Look at how many different
       variables it actually uses and dispatch accordingly. */
    char names[8][64];
    int n = free_vars(lhs, rhs, names);

    if (n == 0) {
        semantic_error("/graph needs at least one variable to plot");
    } else if (n == 1) {
        graph_equation_1var(lhs, rhs, names[0], show_tac);
    } else if (n == 2) {
        const char *h = strcmp(names[0], "x") == 0 ? names[0] : strcmp(names[1], "x") == 0 ? names[1] : names[0];
        const char *v = (h == names[0]) ? names[1] : names[0];
        graph_equation_2var(lhs, rhs, h, v, show_tac);
    } else {
        semantic_error("/graph supports at most two variables, found %d", n);
    }
}
