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

#define GRAPH_WIDTH   200  /* columns sampled across the horizontal range */
#define GRAPH_HEIGHT  75  /* rows of the ASCII plot                       */
#define AXIS_LO      -10.0 /* horizontal (plotted-variable) range          */
#define AXIS_HI       10.0
/* Monospace terminal cells are usually about twice as tall as wide.
 * Match physical distance per math unit on both axes, independent of the
 * canvas dimensions. This keeps circles round and slopes proportional. */
#define CELL_HEIGHT_TO_WIDTH 2.0
#define VIEW_Y_HI ((AXIS_HI - AXIS_LO) * CELL_HEIGHT_TO_WIDTH * (GRAPH_HEIGHT - 1) / (2.0 * (GRAPH_WIDTH - 1)))
#define VIEW_Y_LO (-VIEW_Y_HI)
#define Y_DEFAULT_LO -50.0  /* implicit root search, independent of viewport */
#define Y_DEFAULT_HI  50.0
#define GRAPH_SAMPLES ((GRAPH_WIDTH - 1) * 16 + 1)
#define MAX_PTS_PER_COL 4  /* an implicit curve (e.g. a circle) can have more than one y per x */

typedef struct {
    double y[MAX_PTS_PER_COL];
    int n;
} Column;

/* Dense samples draw the curve with dots directly, without joining across
 * poles or undefined regions. Off-screen values are clipped, never clamped
 * onto the frame or used to stretch the axes. */
static void render(const char *h_name, const char *v_name, const Column *cols) {
    static char canvas[GRAPH_HEIGHT][GRAPH_WIDTH + 1];
    /* Map the origin exactly like curve samples, including odd row counts. */
    int zero_row = (int)round(VIEW_Y_HI / (VIEW_Y_HI - VIEW_Y_LO) * (GRAPH_HEIGHT - 1));
    int zero_col = (int)round(-AXIS_LO / (AXIS_HI - AXIS_LO) * (GRAPH_WIDTH - 1));
    for (int r = 0; r < GRAPH_HEIGHT; r++) {
        memset(canvas[r], ' ', GRAPH_WIDTH);
        canvas[r][GRAPH_WIDTH] = '\0';
    }
    for (int c = 0; c < GRAPH_WIDTH; c++) canvas[zero_row][c] = '-';
    for (int r = 0; r < GRAPH_HEIGHT; r++) canvas[r][zero_col] = '|';
    canvas[zero_row][zero_col] = '+';

    int visible = 0, clipped = 0;
    for (int i = 0; i < GRAPH_SAMPLES; i++) {
        int col = (int)round(i * (GRAPH_WIDTH - 1.0) / (GRAPH_SAMPLES - 1));
        for (int j = 0; j < cols[i].n; j++) {
            double y = cols[i].y[j];
            if (!isfinite(y)) continue;
            if (y < VIEW_Y_LO || y > VIEW_Y_HI) { clipped++; continue; }
            int row = (int)round((VIEW_Y_HI - y) / (VIEW_Y_HI - VIEW_Y_LO) * (GRAPH_HEIGHT - 1));
            canvas[row][col] = '.';
            visible++;
        }
    }

    printf("\nPlot (%s from %g to %g, %s from %g to %g):\n",
           h_name, AXIS_LO, AXIS_HI, v_name, VIEW_Y_LO, VIEW_Y_HI);
    printf("  %s  |  %d x %d canvas\n", v_name, GRAPH_WIDTH, GRAPH_HEIGHT);
    for (int r = 0; r < GRAPH_HEIGHT; r++) {
        /* Label whole coordinate values at their actual projected rows. */
        int has_tick = 0;
        double tick_value = 0;
        for (double value = ceil(VIEW_Y_LO / 2) * 2; value <= VIEW_Y_HI; value += 2) {
            int tick_row = (int)round((VIEW_Y_HI - value) / (VIEW_Y_HI - VIEW_Y_LO) * (GRAPH_HEIGHT - 1));
            if (tick_row == r) { has_tick = 1; tick_value = value; break; }
        }
        if (has_tick) printf("  %8g  ", tick_value);
        else printf("            ");
        printf("%s\n", canvas[r]);
    }
    char labels[GRAPH_WIDTH + 1];
    memset(labels, ' ', GRAPH_WIDTH); labels[GRAPH_WIDTH] = '\0';
    for (int k = 0; k <= 10; k++) {
        char label[16];
        snprintf(label, sizeof(label), "%g", AXIS_LO + k * (AXIS_HI - AXIS_LO) / 10);
        int len = (int)strlen(label), pos = (int)round(k * (GRAPH_WIDTH - 1) / 10.0) - len / 2;
        if (pos < 0) pos = 0;
        if (pos + len > GRAPH_WIDTH) pos = GRAPH_WIDTH - len;
        memcpy(labels + pos, label, len);
    }
    printf("            %s  %s\n", labels, h_name);
    printf("  Curve: .   Axes: | - +   (equal unit scale for 2:1 terminal cells)\n");
    if (clipped) printf("  Portions outside the visible range are clipped.\n");
    if (!visible) printf("  No real curve points in this view.\n");
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
    eval(rhs, 1, &ok);
    if (g_semantic_error) { if (had) symtab_set(var_name, saved); else symtab_unset(var_name); return; }

    TACProgram tac;
    tac_init(&tac);
    const char *operand = tac_build(&tac, rhs);
    tac_finish_graph(&tac, operand, var_name);
    if (show_tac) { printf("IR (TAC):\n"); tac_print(&tac); }

    static Column cols[GRAPH_SAMPLES];
    double xs[GRAPH_SAMPLES];
    int skipped = 0;
    double step = (AXIS_HI - AXIS_LO) / (GRAPH_SAMPLES - 1);

    for (int i = 0; i < GRAPH_SAMPLES; i++) {
        double x = AXIS_LO + i * step;
        xs[i] = x;
        symtab_set(var_name, x);
        int pok = 1;
        double y = eval(rhs, 1, &pok);
        if (pok && isfinite(y)) { cols[i].y[0] = y; cols[i].n = 1; }
        else { cols[i].n = 0; skipped++; }
    }
    if (had) symtab_set(var_name, saved); else symtab_unset(var_name);

    printf("Graph generated.\n\n");
    printf("Sample points:\n");
    for (int i = 0; i < GRAPH_SAMPLES; i += (GRAPH_SAMPLES - 1) / 8) {
        if (cols[i].n)
            printf("  %s = %-6s -> y = %s\n", var_name, format_number(xs[i]), format_number(cols[i].y[0]));
        else
            printf("  %s = %-6s -> y = (undefined)\n", var_name, format_number(xs[i]));
    }

    render(var_name, "y", cols);

    if (skipped > 0)
        printf("\n(%d of %d sample points were outside the real-valued domain and skipped.)\n",
               skipped, GRAPH_SAMPLES);
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
    if (ok) eval(rhs, 1, &ok);
    if (g_semantic_error) { if (had) symtab_set(var_name, saved); else symtab_unset(var_name); return; }

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

    static Column cols[GRAPH_SAMPLES];
    double step = (AXIS_HI - AXIS_LO) / (GRAPH_SAMPLES - 1);
    for (int i = 0; i < GRAPH_SAMPLES; i++) {
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

    /* Keep readable coordinate samples independent of canvas resolution. */
    printf("\nSample points (%s, %s):\n", h_name, v_name);
    for (int i = 0; i <= 10; i++) {
        double h = AXIS_LO + i * (AXIS_HI - AXIS_LO) / 10;
        symtab_set(h_name, h);
        double roots[ROOTFIND_MAX];
        int rok = 1;
        int n = rootfind_solve(lhs, rhs, v_name, Y_DEFAULT_LO, Y_DEFAULT_HI, 400, roots, &rok);
        if (rok)
            for (int j = 0; j < n && j < MAX_PTS_PER_COL; j++)
                printf("  (%s, %s)\n", format_number(h), format_number(roots[j]));
    }
    if (had_h) symtab_set(h_name, saved_h); else symtab_unset(h_name);
    if (had_v) symtab_set(v_name, saved_v); else symtab_unset(v_name);

    render(h_name, v_name, cols);
}

void graph_run(ASTNode *stmt, int show_tac) {
    if (stmt->kind != N_BINOP || stmt->op != '=') {
        char names[8][64];
        int n = free_vars(stmt, NULL, names);
        if (n != 1) {
            semantic_error("/graph expects a single-variable expression or an equation, e.g. y^2, y^3 = x, or y = x^2");
            return;
        }
        if (strcmp(names[0], "y") == 0) {
            /* A bare function of y means x = f(y). Borrow stmt;
             * only the temporary x node belongs to this call. */
            ASTNode *x = ast_var("x");
            graph_equation_2var(x, stmt, "x", "y", show_tac);
            ast_free(x);
        } else {
            graph_function(stmt, show_tac);
        }
        return;
    }
    ASTNode *lhs = stmt->left;
    ASTNode *rhs = stmt->right;

    /* The common, documented shape: an explicit function of one
       variable assigned to 'y'. Kept as its own fast path since it
       is the primary, best-tested feature. */
    char rhs_names[8][64];
    int rhs_count = free_vars(rhs, NULL, rhs_names);
    int rhs_has_y = 0;
    for (int i = 0; i < rhs_count; i++)
        if (strcmp(rhs_names[i], "y") == 0) rhs_has_y = 1;
    if (lhs->kind == N_VAR && strcmp(lhs->name, "y") == 0 && !rhs_has_y) {
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
