#include <stdio.h>
#include <string.h>
#include <math.h>
#include "graph.h"
#include "ir.h"
#include "eval.h"
#include "symtab.h"
#include "errors.h"
#include "util.h"

#define GRAPH_WIDTH  61   /* columns sampled across the x-range   */
#define GRAPH_HEIGHT 21   /* rows of the ASCII plot                */
#define X_LO  -10.0
#define X_HI   10.0

void graph_run(ASTNode *stmt, int show_tac) {
    if (stmt->kind != N_BINOP || stmt->op != '=') {
        semantic_error("/graph mode expects 'y = expression', e.g. y = x^2");
        return;
    }
    ASTNode *lhs = stmt->left;
    ASTNode *rhs = stmt->right;
    if (lhs->kind != N_VAR || strcmp(lhs->name, "y") != 0) {
        semantic_error("/graph mode expects the left side to be 'y'");
        return;
    }

    double saved_x; int had_x = symtab_lookup("x", &saved_x);

    /* Validate once, loudly (unknown function/variable etc.) */
    symtab_set("x", 1.0);
    int ok = 1;
    eval(rhs, 0, &ok);
    if (!ok) { if (had_x) symtab_set("x", saved_x); return; }

    TACProgram tac;
    tac_init(&tac);
    const char *operand = tac_build(&tac, rhs);
    tac_finish_graph(&tac, operand);
    if (show_tac) { printf("IR (TAC):\n"); tac_print(&tac); }

    double xs[GRAPH_WIDTH], ys[GRAPH_WIDTH];
    int valid[GRAPH_WIDTH];
    double ymin = 1e300, ymax = -1e300;
    int valid_count = 0;
    int skipped = 0;
    double step = (X_HI - X_LO) / (GRAPH_WIDTH - 1);

    for (int i = 0; i < GRAPH_WIDTH; i++) {
        double x = X_LO + i * step;
        xs[i] = x;
        symtab_set("x", x);
        int pok = 1;
        double y = eval(rhs, 1, &pok);
        valid[i] = pok;
        if (pok) {
            ys[i] = y;
            if (y < ymin) ymin = y;
            if (y > ymax) ymax = y;
            valid_count++;
        } else {
            skipped++;
        }
    }
    if (had_x) symtab_set("x", saved_x);

    if (valid_count == 0) {
        math_error("no points of this equation lie in the real-valued domain over [%g, %g]", X_LO, X_HI);
        return;
    }
    if (ymax - ymin < 1e-9) { ymax += 1; ymin -= 1; }

    printf("Graph generated.\n\n");

    /* Sample table: 9 evenly spaced points, like the design doc. */
    printf("Sample points:\n");
    for (int i = 0; i < GRAPH_WIDTH; i += (GRAPH_WIDTH - 1) / 8) {
        if (valid[i])
            printf("  x = %-6s -> y = %s\n", format_number(xs[i]), format_number(ys[i]));
        else
            printf("  x = %-6s -> y = (undefined)\n", format_number(xs[i]));
    }

    /* ASCII plot */
    static char canvas[GRAPH_HEIGHT][GRAPH_WIDTH + 1];
    for (int r = 0; r < GRAPH_HEIGHT; r++) {
        memset(canvas[r], ' ', GRAPH_WIDTH);
        canvas[r][GRAPH_WIDTH] = '\0';
    }

    int zero_row = -1;
    if (ymin <= 0 && ymax >= 0)
        zero_row = (int)round((ymax - 0) / (ymax - ymin) * (GRAPH_HEIGHT - 1));
    int zero_col = -1;
    if (X_LO <= 0 && X_HI >= 0)
        zero_col = (int)round((0 - X_LO) / (X_HI - X_LO) * (GRAPH_WIDTH - 1));

    if (zero_row >= 0) for (int c = 0; c < GRAPH_WIDTH; c++) if (canvas[zero_row][c] == ' ') canvas[zero_row][c] = '-';
    if (zero_col >= 0) for (int r = 0; r < GRAPH_HEIGHT; r++) if (canvas[r][zero_col] == ' ') canvas[r][zero_col] = '|';
    if (zero_row >= 0 && zero_col >= 0) canvas[zero_row][zero_col] = '+';

    for (int i = 0; i < GRAPH_WIDTH; i++) {
        if (!valid[i]) continue;
        int row = (int)round((ymax - ys[i]) / (ymax - ymin) * (GRAPH_HEIGHT - 1));
        if (row < 0) row = 0;
        if (row >= GRAPH_HEIGHT) row = GRAPH_HEIGHT - 1;
        canvas[row][i] = '*';
    }

    printf("\nPlot (x from %g to %g, y from %s to %s):\n", X_LO, X_HI, format_number(ymin), format_number(ymax));
    for (int r = 0; r < GRAPH_HEIGHT; r++)
        printf("  %s\n", canvas[r]);
    printf("  %s\n", "  (y-axis: x=0 marked '|', x-axis: y=0 marked '-', curve marked '*')");

    if (skipped > 0)
        printf("\n(%d of %d sample points were outside the real-valued domain and skipped.)\n",
               skipped, GRAPH_WIDTH);
}
