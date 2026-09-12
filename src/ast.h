#ifndef AST_H
#define AST_H

/* Every node is one of these kinds. Keeping the AST to a single
   struct with a "kind" tag (instead of one struct per node type)
   is what keeps this compiler small and easy to read. */
typedef enum {
    N_NUM,      /* a literal number,        e.g. 3.14        */
    N_VAR,      /* a variable/constant name, e.g. x, pi       */
    N_BINOP,    /* a binary operator,       e.g. a + b        */
    N_UMINUS,   /* unary minus,             e.g. -a           */
    N_CALL      /* a function call,        e.g. sqrt(x)      */
} NodeKind;

typedef struct ASTNode {
    NodeKind kind;
    double   num;            /* used by N_NUM                     */
    char    *name;           /* used by N_VAR and N_CALL           */
    int      op;             /* used by N_BINOP: '+','-','*','/','^','=','<','>',GE,LE,EQ,NE */
    struct ASTNode *left;    /* left operand / unary operand       */
    struct ASTNode *right;   /* right operand                      */
    struct ASTNode **args;   /* used by N_CALL: argument list       */
    int      argc;
} ASTNode;

ASTNode *ast_num(double value);
ASTNode *ast_var(const char *name);
ASTNode *ast_binop(int op, ASTNode *l, ASTNode *r);
ASTNode *ast_uminus(ASTNode *e);
ASTNode *ast_call(const char *name, ASTNode **args, int argc);

void ast_free(ASTNode *n);
void ast_print(const ASTNode *n, int indent); /* debug / -ast dump */

/* Collects up to max distinct N_VAR names read anywhere in the
 * expression, in first-seen order, into names[][] (each up to 63
 * chars). *count is set to how many were found. Used by graph.c
 * and solver.c to find "the" free variable in an expression
 * instead of assuming it is always called 'x'. */
void ast_collect_vars(const ASTNode *n, char names[][64], int max, int *count);

/* Token codes for the comparison operators (values > 255 so they
   never collide with a single character token like '+'). Bison's
   generated parser.tab.h defines the real values; these are only
   used by ast.c to label N_BINOP->op in ast_print. */
#define OP_GE 1001
#define OP_LE 1002
#define OP_EQ 1003
#define OP_NE 1004

#endif
