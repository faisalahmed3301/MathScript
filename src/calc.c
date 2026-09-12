#include <stdio.h>
#include <string.h>
#include "calc.h"
#include "ir.h"
#include "eval.h"
#include "symtab.h"
#include "errors.h"
#include "util.h"

void calc_run(ASTNode *stmt, int show_tac) {
    TACProgram tac;
    tac_init(&tac);

    if (stmt->kind == N_BINOP && stmt->op == '=') {
        ASTNode *lhs = stmt->left;
        ASTNode *rhs = stmt->right;

        if (lhs->kind != N_VAR) {
            semantic_error("left side of '=' must be a plain variable name in /calc mode");
            return;
        }

        int ok = 1;
        double v = eval(rhs, 0, &ok);
        if (!ok) return;

        char rhs_operand[32];
        snprintf(rhs_operand, sizeof(rhs_operand), "%s", tac_build(&tac, rhs));
        tac_finish_assign(&tac, lhs->name, rhs_operand);

        if (show_tac) { printf("IR (TAC):\n"); tac_print(&tac); }

        symtab_set(lhs->name, v);
        printf("%s = %s\n", lhs->name, format_number(v));
        return;
    }

    int ok = 1;
    double v = eval(stmt, 0, &ok);
    if (!ok) return;

    char operand[32];
    snprintf(operand, sizeof(operand), "%s", tac_build(&tac, stmt));
    tac_finish_calc(&tac, operand);

    if (show_tac) { printf("IR (TAC):\n"); tac_print(&tac); }

    printf("%s\n", format_number(v));
}
