#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "graph.h"
#include "eval.h"
#include "symtab.h"
#include "rootfind.h"
#include "errors.h"
#include "ir.h"
#include "util.h"
#include "graph_export.h"

/* Default rotation exported to the HTML viewer (y-axis, counterclockwise).
   The user can change it freely inside the browser – no REPL prompt needed. */
#define DEFAULT_ROTATION_AXIS 'y'
#define DEFAULT_ROTATION_DIRECTION 1

void graph3d_run(ASTNode *stmt, int show_tac)
{
    ASTNode *lhs, *rhs, *owned = NULL;
    char found[8][64], names[3][64];

    if (stmt->kind == N_BINOP && stmt->op == '=')
    {
        lhs = stmt->left;
        rhs = stmt->right;
    }
    else
    {
        int n = graph_free_vars(stmt, NULL, found), has_z = 0;
        for (int i = 0; i < n; i++)
            if (!strcmp(found[i], "z"))
                has_z = 1;
        if (n > 2 || has_z)
        {
            semantic_error("/graph3d expects an equation, e.g. x^2+y^2+z^2=25, or a bare expression in x and y");
            return;
        }
        owned = ast_var("z");
        lhs = owned;
        rhs = stmt;
    }

    int n = graph_free_vars(lhs, rhs, found);
    if (n < 1 || n > 3)
    {
        semantic_error("/graph3d supports equations in one to three variables, found %d", n);
        ast_free(owned);
        return;
    }

    int standard = 1;
    for (int i = 0; i < n; i++)
        if (strcmp(found[i], "x") && strcmp(found[i], "y") && strcmp(found[i], "z"))
            standard = 0;

    if (standard)
    {
        strcpy(names[0], "x");
        strcpy(names[1], "y");
        strcpy(names[2], "z");
    }
    else
    {
        for (int i = 0; i < n; i++)
            memcpy(names[i], found[i], 64);
        for (int i = 0; i < n; i++)
            for (int j = i + 1; j < n; j++)
                if (strcmp(names[i], names[j]) > 0)
                {
                    char tmp[64];
                    memcpy(tmp, names[i], 64);
                    memcpy(names[i], names[j], 64);
                    memcpy(names[j], tmp, 64);
                }
        for (int i = n; i < 3; i++)
        {
            for (int k = 0; k < 3; k++)
            {
                char candidate[2] = {"xyz"[k], 0};
                int used = 0;
                for (int j = 0; j < i; j++)
                    if (!strcmp(names[j], candidate))
                        used = 1;
                if (!used)
                {
                    strcpy(names[i], candidate);
                    break;
                }
            }
        }
    }

    double saved[3] = {0};
    int had[3];
    for (int i = 0; i < 3; i++)
    {
        had[i] = symtab_lookup(names[i], saved + i);
        symtab_set(names[i], 0);
    }

    if (!eval_validate(lhs) || !eval_validate(rhs))
        goto cleanup;

    if (show_tac)
    {
        TACProgram tac;
        tac_init(&tac);
        ASTNode *diff = ast_binop('-', lhs, rhs);
        char operand[32];
        snprintf(operand, sizeof(operand), "%s", tac_build(&tac, diff));
        free(diff);
        printf("IR (TAC):\n");
        tac_print(&tac);
        printf("SURFACE %s = 0\nPLOT3D %s, %s, %s\n", operand, names[0], names[1], names[2]);
    }

    printf("3D graph generated (implicit surface in %s, %s, %s).\n", names[0], names[1], names[2]);
    printf("Range: %s [%g, %g], %s [%g, %g], %s [%g, %g].\n",
           names[0], graph3d_settings.lo[0], graph3d_settings.hi[0],
           names[1], graph3d_settings.lo[1], graph3d_settings.hi[1],
           names[2], graph3d_settings.lo[2], graph3d_settings.hi[2]);

    {
        char axis[2] = {DEFAULT_ROTATION_AXIS, 0};
        graph_export_view(3, lhs, rhs, names, &graph3d_settings,
                          "surface", axis, DEFAULT_ROTATION_DIRECTION);
    }
    printf("Open the HTML to explore the interactive 3D view (rotation controlled in browser).\n");

cleanup:
    ast_free(owned);
    for (int i = 0; i < 3; i++)
        if (had[i])
            symtab_set(names[i], saved[i]);
        else
            symtab_unset(names[i]);
}
