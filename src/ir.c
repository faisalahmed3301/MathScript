#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "ir.h"

/* Rotating pool of small operand buffers. tac_build() hands one
   of these back to its caller; a single line never needs more
   than a handful alive at once, so 64 is comfortable headroom. */
static char pool[64][32];
static int  pool_i = 0;

static char *next_buf(void) {
    pool_i = (pool_i + 1) % 64;
    return pool[pool_i];
}

void tac_init(TACProgram *p) {
    p->count = 0;
    p->next_temp = 1;
}

static void emit(TACProgram *p, const char *fmt, ...) {
    if (p->count >= TAC_MAX_LINES) return;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(p->lines[p->count], TAC_LINE_LEN, fmt, ap);
    va_end(ap);
    p->count++;
}

void tac_print(const TACProgram *p) {
    for (int i = 0; i < p->count; i++)
        printf("%s\n", p->lines[i]);
}

static const char *op_symbol(int op) {
    switch (op) {
        case '+': return "+";
        case '-': return "-";
        case '*': return "*";
        case '/': return "/";
        case '^': return "^";
        case '<': return "<";
        case '>': return ">";
        case OP_GE: return ">=";
        case OP_LE: return "<=";
        case OP_EQ: return "==";
        case OP_NE: return "!=";
        default:   return "?";
    }
}

const char *tac_build(TACProgram *p, ASTNode *n) {
    char *out = next_buf();

    switch (n->kind) {
        case N_NUM:
            snprintf(out, 32, "%g", n->num);
            return out;

        case N_VAR:
            snprintf(out, 32, "%s", n->name);
            return out;

        case N_UMINUS: {
            const char *a = tac_build(p, n->left);
            char temp[16];
            snprintf(temp, sizeof(temp), "t%d", p->next_temp++);
            emit(p, "%s = 0 - %s", temp, a);
            snprintf(out, 32, "%s", temp);
            return out;
        }

        case N_CALL: {
            char args[3][32] = {"", "", ""};
            for (int i = 0; i < n->argc && i < 2; i++)
                snprintf(args[i], 32, "%s", tac_build(p, n->args[i]));
            char temp[16];
            snprintf(temp, sizeof(temp), "t%d", p->next_temp++);
            if (n->argc == 2)
                emit(p, "%s = CALL %s(%s, %s)", temp, n->name, args[0], args[1]);
            else
                emit(p, "%s = CALL %s(%s)", temp, n->name, args[0]);
            snprintf(out, 32, "%s", temp);
            return out;
        }

        case N_BINOP: {
            const char *a = tac_build(p, n->left);
            char abuf[32]; snprintf(abuf, 32, "%s", a);
            const char *b = tac_build(p, n->right);
            char temp[16];
            snprintf(temp, sizeof(temp), "t%d", p->next_temp++);
            emit(p, "%s = %s %s %s", temp, abuf, op_symbol(n->op), b);
            snprintf(out, 32, "%s", temp);
            return out;
        }
    }
    snprintf(out, 32, "0");
    return out;
}

void tac_finish_calc(TACProgram *p, const char *operand) {
    emit(p, "RESULT = %s", operand);
}

void tac_finish_assign(TACProgram *p, const char *var, const char *val) {
    emit(p, "%s = %s", var, val);
}

void tac_finish_graph(TACProgram *p, const char *operand, const char *var_name, double lo, double hi) {
    emit(p, "RETURN %s", operand);
    emit(p, "PLOT %s FROM %g TO %g", var_name, lo, hi);
}

void tac_finish_eqn(TACProgram *p, const char *operand) {
    emit(p, "EQUATION %s = 0", operand);
    emit(p, "SOLVE");
}
