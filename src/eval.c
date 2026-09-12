#include <math.h>
#include <string.h>
#include "eval.h"
#include "symtab.h"
#include "errors.h"

static double call_function(const char *name, ASTNode **args, int argc,
                             int quiet, int *ok) {
    /* one argument functions */
    if (strcmp(name, "sqrt") == 0 || strcmp(name, "abs") == 0 ||
        strcmp(name, "sin") == 0  || strcmp(name, "cos") == 0  ||
        strcmp(name, "tan") == 0  || strcmp(name, "log") == 0) {
        if (argc != 1) {
            semantic_error("'%s' expects 1 argument, got %d", name, argc);
            *ok = 0; return 0;
        }
        double a = eval(args[0], quiet, ok);
        if (!*ok) return 0;

        if (strcmp(name, "sqrt") == 0) {
            if (a < 0) { if (!quiet) math_error("sqrt of negative number %g", a); *ok = 0; return 0; }
            return sqrt(a);
        }
        if (strcmp(name, "abs") == 0) return fabs(a);
        if (strcmp(name, "sin") == 0) return sin(a);
        if (strcmp(name, "cos") == 0) return cos(a);
        if (strcmp(name, "tan") == 0) return tan(a);
        /* log */
        if (a <= 0) { if (!quiet) math_error("log of non-positive number %g", a); *ok = 0; return 0; }
        return log(a);
    }

    /* two argument functions */
    if (strcmp(name, "pow") == 0) {
        if (argc != 2) {
            semantic_error("'pow' expects 2 arguments, got %d", argc);
            *ok = 0; return 0;
        }
        double a = eval(args[0], quiet, ok);
        if (!*ok) return 0;
        double b = eval(args[1], quiet, ok);
        if (!*ok) return 0;
        return pow(a, b);
    }

    semantic_error("Unknown function '%s'", name);
    *ok = 0;
    return 0;
}

double eval(ASTNode *n, int quiet, int *ok) {
    if (!*ok || !n) return 0;

    switch (n->kind) {
        case N_NUM:
            return n->num;

        case N_VAR: {
            double v;
            if (!symtab_lookup(n->name, &v)) {
                semantic_error("Undefined variable '%s'", n->name);
                *ok = 0;
                return 0;
            }
            return v;
        }

        case N_UMINUS:
            return -eval(n->left, quiet, ok);

        case N_CALL:
            return call_function(n->name, n->args, n->argc, quiet, ok);

        case N_BINOP: {
            double a = eval(n->left, quiet, ok);
            if (!*ok) return 0;
            double b = eval(n->right, quiet, ok);
            if (!*ok) return 0;

            switch (n->op) {
                case '+': return a + b;
                case '-': return a - b;
                case '*': return a * b;
                case '/':
                    if (b == 0) { if (!quiet) math_error("Division by zero"); *ok = 0; return 0; }
                    return a / b;
                case '^': return pow(a, b);
                case '<':  return a <  b ? 1 : 0;
                case '>':  return a >  b ? 1 : 0;
                case OP_GE: return a >= b ? 1 : 0;
                case OP_LE: return a <= b ? 1 : 0;
                case OP_EQ: return a == b ? 1 : 0;
                case OP_NE: return a != b ? 1 : 0;
                case '=':
                    /* '=' is only meaningful to the mode-specific
                       backends (assignment / equation), never here. */
                    semantic_error("'=' cannot appear inside an expression");
                    *ok = 0;
                    return 0;
            }
        }
    }
    *ok = 0;
    return 0;
}
