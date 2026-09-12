#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "solver.h"
#include "ir.h"
#include "eval.h"
#include "symtab.h"
#include "errors.h"
#include "util.h"
#include "rootfind.h"
#include "poly.h"
#include <math.h>

/* Loudly evaluates lhs(var_name)-rhs(var_name) once at var_name=0,
 * so an undefined name or wrong-arity call is reported clearly
 * instead of silently producing zero roots. */
static void validate_once(ASTNode *lhs, ASTNode *rhs, const char *var_name, int *ok) {
    symtab_set(var_name, 0.0);
    double a = eval(lhs, 0, ok);
    if (!*ok) return;
    eval(rhs, 0, ok);
    (void)a;
}

void solver_run(ASTNode *stmt, int show_tac) {
    if (stmt->kind != N_BINOP || stmt->op != '=') {
        semantic_error("/eqn mode expects an equation, e.g. x^2 - 5*x + 6 = 0");
        return;
    }
    ASTNode *lhs = stmt->left;
    ASTNode *rhs = stmt->right;

    /* Find the one free variable to solve for (usually 'x', but any
       single name works: "y^2 = 4" solves for y). Constants (pi, e)
       don't count, and more than one free name is ambiguous. */
    char names[8][64];
    int name_count = 0;
    ast_collect_vars(lhs, names, 8, &name_count);
    ast_collect_vars(rhs, names, 8, &name_count);
    int free_count = 0;
    char var_name[64] = "x";
    for (int i = 0; i < name_count; i++) {
        if (symtab_is_constant(names[i])) continue;
        if (free_count == 0) strncpy(var_name, names[i], 63);
        free_count++;
    }
    if (free_count > 1) {
        semantic_error("/eqn supports exactly one free variable, found %d (e.g. '%s' and '%s')",
                        free_count, var_name, names[1]);
        return;
    }

    double saved_x; int had_x = symtab_lookup(var_name, &saved_x);

    int ok = 1;
    validate_once(lhs, rhs, var_name, &ok);
    if (!ok) { if (had_x) symtab_set(var_name, saved_x); else symtab_unset(var_name); return; }

    /* --- IR: EQUATION lhs - rhs = 0 ; SOLVE ------------------- */
    TACProgram tac;
    tac_init(&tac);
    ASTNode *diff = ast_binop('-', lhs, rhs); /* shallow wrapper, not owning lhs/rhs */
    const char *operand = tac_build(&tac, diff);
    tac_finish_eqn(&tac, operand);
    free(diff);
    if (show_tac) { printf("IR (TAC):\n"); tac_print(&tac); }

    Polynomial left, right;
    if (poly_extract(lhs, var_name, &left) && poly_extract(rhs, var_name, &right)) {
        for (int i = 0; i < 4; i++) left.c[i] -= right.c[i];
        PolyRoot solutions[3];
        int count = poly_solve(&left, solutions);
        if (had_x) symtab_set(var_name, saved_x); else symtab_unset(var_name);
        if (!count) {
            printf("%s\n", left.c[0] == 0 ? "Infinitely many solutions." : "No solution.");
            return;
        }
        printf("%s\n", count == 1 ? "Root:" : "Roots (including repeated roots):");
        for (int i = 0; i < count; i++) {
            if (solutions[i].imag == 0)
                printf("%s = %s\n", var_name, format_number((double)solutions[i].real));
            else
                printf("%s = %s %c %si\n", var_name,
                       format_number((double)solutions[i].real), solutions[i].imag < 0 ? '-' : '+',
                       format_number((double)fabsl(solutions[i].imag)));
        }
        return;
    }

    double roots[ROOTFIND_MAX];
    int nroots = rootfind_solve(lhs, rhs, var_name, -25.0, 25.0, 5000, roots, &ok);

    if (had_x) symtab_set(var_name, saved_x); else symtab_unset(var_name);

    if (nroots == 0) {
        printf("No roots found in the search range [-25, 25].\n");
        return;
    }

    if (nroots == 1) {
        printf("Root:\n");
    } else {
        printf("Roots:\n");
    }
    for (int i = 0; i < nroots; i++)
        printf("%s = %s\n", var_name, format_number(roots[i]));
}
