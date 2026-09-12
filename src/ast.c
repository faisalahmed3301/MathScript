#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"

static ASTNode *alloc_node(NodeKind kind) {
    ASTNode *n = calloc(1, sizeof(ASTNode));
    n->kind = kind;
    return n;
}

ASTNode *ast_num(double value) {
    ASTNode *n = alloc_node(N_NUM);
    n->num = value;
    return n;
}

ASTNode *ast_var(const char *name) {
    ASTNode *n = alloc_node(N_VAR);
    n->name = strdup(name);
    return n;
}

ASTNode *ast_binop(int op, ASTNode *l, ASTNode *r) {
    ASTNode *n = alloc_node(N_BINOP);
    n->op = op;
    n->left = l;
    n->right = r;
    return n;
}

ASTNode *ast_uminus(ASTNode *e) {
    ASTNode *n = alloc_node(N_UMINUS);
    n->left = e;
    return n;
}

ASTNode *ast_call(const char *name, ASTNode **args, int argc) {
    ASTNode *n = alloc_node(N_CALL);
    n->name = strdup(name);
    n->args = args;
    n->argc = argc;
    return n;
}

void ast_free(ASTNode *n) {
    if (!n) return;
    ast_free(n->left);
    ast_free(n->right);
    if (n->args) {
        for (int i = 0; i < n->argc; i++) ast_free(n->args[i]);
        free(n->args);
    }
    free(n->name);
    free(n);
}

void ast_collect_vars(const ASTNode *n, char names[][64], int max, int *count) {
    if (!n) return;
    if (n->kind == N_VAR) {
        for (int i = 0; i < *count; i++)
            if (strcmp(names[i], n->name) == 0) return;
        if (*count < max) {
            strncpy(names[*count], n->name, 63);
            names[*count][63] = '\0';
            (*count)++;
        }
        return;
    }
    ast_collect_vars(n->left, names, max, count);
    ast_collect_vars(n->right, names, max, count);
    for (int i = 0; i < n->argc; i++) ast_collect_vars(n->args[i], names, max, count);
}

static const char *op_name(int op) {
    switch (op) {
        case '+': return "+";
        case '-': return "-";
        case '*': return "*";
        case '/': return "/";
        case '^': return "^";
        case '=': return "=";
        case '<': return "<";
        case '>': return ">";
        case OP_GE: return ">=";
        case OP_LE: return "<=";
        case OP_EQ: return "==";
        case OP_NE: return "!=";
        default:   return "?";
    }
}

void ast_print(const ASTNode *n, int indent) {
    if (!n) return;
    for (int i = 0; i < indent; i++) printf("  ");
    switch (n->kind) {
        case N_NUM:
            printf("NUM %g\n", n->num);
            break;
        case N_VAR:
            printf("VAR %s\n", n->name);
            break;
        case N_UMINUS:
            printf("UMINUS\n");
            ast_print(n->left, indent + 1);
            break;
        case N_BINOP:
            printf("BINOP %s\n", op_name(n->op));
            ast_print(n->left, indent + 1);
            ast_print(n->right, indent + 1);
            break;
        case N_CALL:
            printf("CALL %s/%d\n", n->name, n->argc);
            for (int i = 0; i < n->argc; i++) ast_print(n->args[i], indent + 1);
            break;
    }
}
