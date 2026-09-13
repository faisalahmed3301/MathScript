#include <stdlib.h>
#include <math.h>
#include "rootfind.h"
#include "eval.h"
#include "symtab.h"
#include "poly.h"

static double difference_at(ASTNode *lhs, ASTNode *rhs, const char *name, double x, int *ok) {
    symtab_set(name, x);
    double a = eval(lhs, 1, ok), b = eval(rhs, 1, ok);
    double f = a - b;
    if (!isfinite(f)) *ok = 0;
    return f;
}

/* A crossing and a neighboring minimum may refine the same zero. Keep
 * a small rolling deduplication window, without limiting the root count. */
typedef struct { double recent[8]; unsigned count; RootVisitor visit; void *ctx; } UniqueRoots;
static void emit_root(double root, UniqueRoots *u) {
    unsigned n=u->count<8?u->count:8;
    for (unsigned i=0;i<n;i++)
        if (fabs(u->recent[i]-root)<=1e-10*(1+fabs(root))) return;
    u->recent[u->count++%8]=root;
    u->visit(root,u->ctx);
}

/* A bracket is accepted only when the residual shrinks. A sign change at
 * a pole (1/x, tan(x), etc.) is not a root. */
static void scan(ASTNode *lhs, ASTNode *rhs, const char *name,
                 double lo, double hi, int samples, RootVisitor visit, void *ctx) {
    UniqueRoots unique={.count=0,.visit=visit,.ctx=ctx};
    double px = 0, pf = 0, ppx = 0, ppf = 0;
    int history = 0;
    for (int i = 0; i <= samples; i++) {
        double x = lo + (hi-lo)*(i/(double)samples);
        int ok = 1;
        double f = difference_at(lhs, rhs, name, x, &ok);
        if (!ok) { history = 0; continue; }
        if (f == 0) emit_root(x, &unique);
        if (history && f != 0 && pf != 0 && signbit(f) != signbit(pf)) {
            double a = px, b = x, fa = pf;
            double scale = fmin(fabs(pf), fabs(f));
            for (int k = 0; k < 64; k++) {
                double m = a + (b-a)/2;
                int mok = 1;
                double fm = difference_at(lhs, rhs, name, m, &mok);
                if (!mok) break;
                if (fm == 0 || fabs(fm) < scale*1e-12) { emit_root(m, &unique); break; }
                if (m == a || m == b) {
                    /* At large coordinates the bracket can reach floating-point
                     * resolution before the relative residual target. */
                    if (fabs(fm) < scale*1e-6) emit_root(m, &unique);
                    break;
                }
                if (signbit(fa) != signbit(fm)) b = m;
                else { a = m; fa = fm; }
            }
        }
        /* Even-multiplicity roots touch zero without changing sign.
         * Refine a sampled local minimum of |f| and require a large
         * residual reduction, so a nonzero minimum is not plotted. */
        if (history >= 2 && pf != 0 && fabs(pf) < fabs(ppf) && fabs(pf) < fabs(f)) {
            double a = ppx, b = x;
            const double ratio = 0.6180339887498949;
            double c = b-ratio*(b-a), d = a+ratio*(b-a);
            int good = 1;
            double fc = fabs(difference_at(lhs,rhs,name,c,&good));
            double fd = fabs(difference_at(lhs,rhs,name,d,&good));
            for (int k = 0; k < 56 && good; k++) {
                if (fc < fd) { b=d; d=c; fd=fc; c=b-ratio*(b-a); fc=fabs(difference_at(lhs,rhs,name,c,&good)); }
                else { a=c; c=d; fc=fd; d=a+ratio*(b-a); fd=fabs(difference_at(lhs,rhs,name,d,&good)); }
            }
            if (good && fmin(fc,fd) < fmin(fabs(ppf),fabs(f))*1e-12)
                emit_root(fc < fd ? c : d, &unique);
        }
        ppx = px; ppf = pf; px = x; pf = f;
        if (history < 2) history++;
    }
}

void rootfind_visit(ASTNode *lhs, ASTNode *rhs, const char *name,
                    double lo, double hi, int samples, RootVisitor visit, void *ctx) {
    Polynomial a, b;
    if (poly_extract_slice(lhs,name,&a) && poly_extract_slice(rhs,name,&b)) {
        for (int i=0;i<4;i++) a.c[i] -= b.c[i];
        if (a.c[0]==0 && a.c[1]==0 && a.c[2]==0 && a.c[3]==0) {
            /* The whole slice satisfies the equation, e.g. x*y=0 at x=0. */
            for (int i=0;i<=samples;i++) visit(lo+(hi-lo)*(i/(double)samples),ctx);
            return;
        }
        PolyRoot roots[3];
        int n = poly_solve(&a,roots);
        for (int i=0;i<n;i++) {
            double r = (double)roots[i].real;
            if (roots[i].imag==0 && isfinite(r) && r>=lo && r<=hi &&
                (i==0 || roots[i].real!=roots[i-1].real)) visit(r,ctx);
        }
        return;
    }
    scan(lhs,rhs,name,lo,hi,samples,visit,ctx);
}

typedef struct { double *roots; int count; } RootList;
static void collect(double r, void *ctx) {
    RootList *list = ctx;
    for (int i=0;i<list->count;i++) if (fabs(list->roots[i]-r)<1e-6) return;
    if (list->count < ROOTFIND_MAX) list->roots[list->count++]=r;
}
static int cmp_double(const void *a,const void *b) {
    double x=*(const double *)a,y=*(const double *)b;
    return (x>y)-(x<y);
}
int rootfind_solve(ASTNode *lhs, ASTNode *rhs, const char *name,
                   double lo,double hi,int samples,double *roots,int *ok) {
    *ok=1;
    RootList list={roots,0};
    /* One-shot solvers retain unrestricted exact polynomial roots. */
    Polynomial a,b;
    if (poly_extract_slice(lhs,name,&a) && poly_extract_slice(rhs,name,&b)) {
        for (int i=0;i<4;i++) a.c[i]-=b.c[i];
        PolyRoot solutions[3]; int n=poly_solve(&a,solutions);
        for (int i=0;i<n;i++) if (solutions[i].imag==0 && isfinite(solutions[i].real)) collect((double)solutions[i].real,&list);
    } else scan(lhs,rhs,name,lo,hi,samples,collect,&list);
    qsort(roots,list.count,sizeof(double),cmp_double);
    return list.count;
}
