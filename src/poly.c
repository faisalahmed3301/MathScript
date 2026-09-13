#include <math.h>
#include <float.h>
#include <string.h>
#include "poly.h"
#include "symtab.h"
#include "eval.h"

static int degree(const Polynomial *p) {
    for (int i = 3; i > 0; i--) if (p->c[i] != 0) return i;
    return 0;
}

static int multiply(Polynomial a, Polynomial b, Polynomial *out) {
    if (degree(&a) + degree(&b) > 3) return 0;
    *out = (Polynomial){{0}};
    for (int i = 0; i <= degree(&a); i++)
        for (int j = 0; j <= degree(&b); j++) out->c[i+j] += a.c[i]*b.c[j];
    return 1;
}

static int extract(ASTNode *n, const char *variable, Polynomial *out, int bound) {
    *out = (Polynomial){{0}};
    if (n->kind == N_NUM) { out->c[0] = n->num; return isfinite(n->num); }
    if (n->kind == N_VAR) {
        if (!strcmp(n->name, variable)) { out->c[1] = 1; return 1; }
        double v;
        if ((!bound && !symtab_is_constant(n->name)) || !symtab_lookup(n->name, &v)) return 0;
        out->c[0] = v;
        return isfinite(v);
    }
    Polynomial a, b;
    if (n->kind == N_UMINUS) {
        if (!extract(n->left, variable, out, bound)) return 0;
        for (int i = 0; i < 4; i++) out->c[i] = -out->c[i];
        return 1;
    }
    int power_call = n->kind == N_CALL && !strcmp(n->name, "pow") && n->argc == 2;
    if (n->kind == N_CALL && !power_call) {
        /* Constant function calls such as sqrt(4) may be coefficients. */
        char names[8][64]; int count = 0;
        ast_collect_vars(n, names, 8, &count);
        for (int i = 0; i < count; i++) if (!strcmp(names[i], variable) || (!bound && !symtab_is_constant(names[i]))) return 0;
        int ok = 1;
        out->c[0] = eval(n, 1, &ok);
        return ok && isfinite(out->c[0]);
    }
    if (n->kind != N_BINOP && !power_call) return 0;
    if (!extract(power_call ? n->args[0] : n->left, variable, &a, bound) ||
        !extract(power_call ? n->args[1] : n->right, variable, &b, bound)) return 0;
    int op = power_call ? '^' : n->op;
    switch (op) {
        case '+': case '-':
            for (int i = 0; i < 4; i++) out->c[i] = a.c[i] + (op == '+' ? b.c[i] : -b.c[i]);
            break;
        case '*': return multiply(a, b, out);
        case '/':
            if (degree(&b) || b.c[0] == 0) return 0;
            for (int i = 0; i < 4; i++) out->c[i] = a.c[i] / b.c[0];
            break;
        case '^': {
            if (degree(&b) || b.c[0] < 0 || b.c[0] > 3 || floorl(b.c[0]) != b.c[0]) return 0;
            out->c[0] = 1;
            for (int i = 0; i < (int)b.c[0]; i++)
                if (!multiply(*out, a, out)) return 0;
            break;
        }
        default: return 0;
    }
    for (int i = 0; i < 4; i++) if (!isfinite(out->c[i])) return 0;
    return 1;
}

int poly_solve(const Polynomial *p, PolyRoot roots[3]) {
    int d = degree(p);
    if (!d) return 0;
    if (d == 1) {
        roots[0] = (PolyRoot){-p->c[0]/p->c[1], 0};
    } else if (d == 2) {
        long double a=p->c[2], b=p->c[1], c=p->c[0];
        long double disc=b*b-4*a*c;
        if (disc < 0) {
            long double re=-b/(2*a), im=sqrtl(-disc)/(2*fabsl(a));
            roots[0]=(PolyRoot){re,-im}; roots[1]=(PolyRoot){re,im};
        } else {
            long double q=-0.5L*(b+copysignl(sqrtl(disc),b));
            roots[0]=(PolyRoot){q/a,0};
            roots[1]=(PolyRoot){q == 0 ? 0 : c/q,0};
        }
    } else {
        /* Depressed cubic t^3 + pt + q, with x = t - A/3. */
        long double A=p->c[2]/p->c[3], B=p->c[1]/p->c[3], C=p->c[0]/p->c[3];
        long double P=B-A*A/3, Q=2*A*A*A/27-A*B/3+C;
        long double u=Q*Q/4, v=P*P*P/27, disc=u+v;
        long double tol=64*LDBL_EPSILON*(fabsl(u)+fabsl(v));
        if (fabsl(disc) <= tol) disc=0;
        if (disc > 0) {
            long double s=cbrtl(-Q/2-copysignl(sqrtl(disc),Q));
            long double t=s == 0 ? 0 : -P/(3*s);
            long double sum=s+t, re=-sum/2-A/3, im=sqrtl(3)*fabsl(s-t)/2;
            roots[0]=(PolyRoot){sum-A/3,0};
            roots[1]=(PolyRoot){re,-im}; roots[2]=(PolyRoot){re,im};
        } else if (disc == 0) {
            long double s=cbrtl(-Q/2);
            roots[0]=(PolyRoot){2*s-A/3,0};
            roots[1]=roots[2]=(PolyRoot){-s-A/3,0};
        } else {
            long double r=2*sqrtl(-P/3), cosine=(3*Q/(2*P))*sqrtl(-3/P);
            cosine=fmaxl(-1,fminl(1,cosine));
            long double theta=acosl(cosine)/3, pi=acosl(-1);
            for (int i=0;i<3;i++) roots[i]=(PolyRoot){r*cosl(theta-2*pi*i/3)-A/3,0};
        }
    }
    for (int i=0;i<d;i++) for (int j=i+1;j<d;j++)
        if (roots[j].real < roots[i].real ||
            (roots[j].real == roots[i].real && roots[j].imag < roots[i].imag)) {
            PolyRoot tmp=roots[i]; roots[i]=roots[j]; roots[j]=tmp;
        }
    return d;
}

int poly_extract(ASTNode *n, const char *variable, Polynomial *out) {
    return extract(n, variable, out, 0);
}

int poly_extract_slice(ASTNode *n, const char *variable, Polynomial *out) {
    return extract(n, variable, out, 1);
}
