#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <errno.h>
#include "graph.h"
#include "ir.h"
#include "eval.h"
#include "symtab.h"
#include "errors.h"
#include "util.h"
#include "rootfind.h"

#define GRAPH_WIDTH   200  /* columns sampled across the horizontal range */
#define GRAPH_HEIGHT  75  /* rows of the ASCII plot                       */
/* Terminal cells are approximately twice as tall as wide. */
#define DEFAULT_Y_HI (20.0*(GRAPH_HEIGHT-1)/(GRAPH_WIDTH-1))
GraphSettings graph2d_settings = {{-10,-DEFAULT_Y_HI,-10},{10,DEFAULT_Y_HI,10},32};
GraphSettings graph3d_settings = {{-10,-10,-10},{10,10,10},192};
#define AXIS_LO (graph2d_settings.lo[0])
#define AXIS_HI (graph2d_settings.hi[0])
#define VIEW_Y_LO (graph2d_settings.lo[1])
#define VIEW_Y_HI (graph2d_settings.hi[1])
#define GRAPH_SAMPLES ((GRAPH_WIDTH - 1) * graph2d_settings.samples + 1)
static char canvas[GRAPH_HEIGHT][GRAPH_WIDTH+1];
static int visible, clipped;

int graph_command(const char *line, int dimensions) {
    GraphSettings *s = dimensions == 3 ? &graph3d_settings : &graph2d_settings;
    if (!strncmp(line,"/range",6) && (line[6]==0 || isspace((unsigned char)line[6]))) {
        double v[6];
        const char *cursor=line+6; int valid=1;
        for (int i=0;i<dimensions*2;i++) {
            char *end; errno=0; v[i]=strtod(cursor,&end);
            if (cursor==end || errno==ERANGE) { valid=0; break; }
            cursor=end;
        }
        while (isspace((unsigned char)*cursor)) cursor++;
        if (!valid || *cursor) { printf("Usage: /range xmin xmax ymin ymax%s\n",dimensions==3?" zmin zmax":""); return 1; }
        for (int i=0;i<dimensions;i++) {
            if (!isfinite(v[2*i]) || !isfinite(v[2*i+1]) || v[2*i]>=v[2*i+1] || !isfinite(v[2*i+1]-v[2*i])) {
                printf("Range bounds must be finite and increasing.\n"); return 1;
            }
        }
        for (int i=0;i<dimensions;i++) { s->lo[i]=v[2*i]; s->hi[i]=v[2*i+1]; }
        printf("%dD plot range updated. Enter an equation to plot it.\n",dimensions);
        return 1;
    }
    if (!strncmp(line,"/samples",8) && (line[8]==0 || isspace((unsigned char)line[8]))) {
        const int max_samples=dimensions==3?512:256;
        char *end; errno=0; long n=strtol(line+8,&end,10);
        while (isspace((unsigned char)*end)) end++;
        if (end==line+8 || *end || errno==ERANGE || n<4 || n>max_samples) {
            printf("Usage: /samples N (integer from 4 to %d)\n",max_samples); return 1;
        }
        s->samples=(int)n;
        printf("%dD sampling set to %d%s. Enter an equation to plot it.\n",dimensions,(int)n,dimensions==2?" samples per terminal column":" subdivisions per axis");
        return 1;
    }
    return 0;
}
int graph_axis_marks(double lo,double hi,int capacity,double *values) {
    int n=0;
    for (int i=0;i<capacity;i++) {
        double lower=lo+(hi-lo)*fmax(0,(i-.5)/(capacity-1));
        double upper=lo+(hi-lo)*fmin(1,(i+.5)/(capacity-1));
        double value=ceil(lower*3)/3;
        if (isfinite(value) && value>=lo && value<=upper && value<=hi &&
            (!n || value>values[n-1])) values[n++]=value==0?0:value;
    }
    return n;
}
static void clear_plot(void) {
    visible=clipped=0;
    for (int r=0;r<GRAPH_HEIGHT;r++) { memset(canvas[r],' ',GRAPH_WIDTH); canvas[r][GRAPH_WIDTH]=0; }
    double axis_y=fmax(VIEW_Y_LO,fmin(0,VIEW_Y_HI));
    double axis_x=fmax(AXIS_LO,fmin(0,AXIS_HI));
    int zr=(int)round((VIEW_Y_HI-axis_y)/(VIEW_Y_HI-VIEW_Y_LO)*(GRAPH_HEIGHT-1));
    int zc=(int)round((axis_x-AXIS_LO)/(AXIS_HI-AXIS_LO)*(GRAPH_WIDTH-1));
    for (int col=0;col<GRAPH_WIDTH;col++) canvas[zr][col]='.';
    for (int row=0;row<GRAPH_HEIGHT;row++) canvas[row][zc]='.';
    double marks[GRAPH_WIDTH*2];
    int n=graph_axis_marks(AXIS_LO,AXIS_HI,GRAPH_WIDTH*2,marks);
    for (int i=0;i<n;i++) canvas[zr][(int)round((marks[i]-AXIS_LO)/(AXIS_HI-AXIS_LO)*(GRAPH_WIDTH-1))]='o';
    n=graph_axis_marks(VIEW_Y_LO,VIEW_Y_HI,GRAPH_WIDTH*2,marks);
    for (int i=0;i<n;i++) canvas[(int)round((VIEW_Y_HI-marks[i])/(VIEW_Y_HI-VIEW_Y_LO)*(GRAPH_HEIGHT-1))][zc]='o';
}
static void point(double x,double y) {
    if (!isfinite(x) || !isfinite(y)) return;
    if (x<AXIS_LO || x>AXIS_HI || y<VIEW_Y_LO || y>VIEW_Y_HI) { clipped++; return; }
    int c=(int)round((x-AXIS_LO)/(AXIS_HI-AXIS_LO)*(GRAPH_WIDTH-1));
    int r=(int)round((VIEW_Y_HI-y)/(VIEW_Y_HI-VIEW_Y_LO)*(GRAPH_HEIGHT-1));
    canvas[r][c]='.'; visible++;
}
/* Whole-unit coordinates, with a bounded number of ticks even when a
 * custom viewport spans billions of units. Wide views omit labels. */
static int unit_ticks(double lo,double hi,int capacity,double *values) {
    double step=fmax(1,ceil((hi-lo)/(capacity-1)));
    double first=ceil(lo/step)*step;
    int n=0;
    for (int i=0;i<capacity;i++) {
        double value=first+i*step;
        if (!isfinite(value) || value>hi) break;
        if (value>=lo && (n==0 || value>values[n-1])) values[n++]=value==0?0:value;
    }
    return n;
}
static void render(const char *h_name,const char *v_name) {
    double xticks[GRAPH_WIDTH],yticks[GRAPH_HEIGHT];
    int nx=unit_ticks(AXIS_LO,AXIS_HI,GRAPH_WIDTH,xticks);
    int ny=unit_ticks(VIEW_Y_LO,VIEW_Y_HI,GRAPH_HEIGHT,yticks);
    printf("\nPlot (%s from %g to %g, %s from %g to %g):\n",
           h_name, AXIS_LO, AXIS_HI, v_name, VIEW_Y_LO, VIEW_Y_HI);
    printf("  %s  |  %d x %d canvas\n", v_name, GRAPH_WIDTH, GRAPH_HEIGHT);
    for (int r = 0; r < GRAPH_HEIGHT; r++) {
        /* Label whole coordinate values at their actual projected rows. */
        int has_tick = 0;
        double tick_value = 0;
        for (int k = 0; k < ny; k++) {
            double value=yticks[k];
            int tick_row = (int)round((VIEW_Y_HI - value) / (VIEW_Y_HI - VIEW_Y_LO) * (GRAPH_HEIGHT - 1));
            if (tick_row == r) { has_tick = 1; tick_value = value; break; }
        }
        if (has_tick) printf("  %8g  ", tick_value);
        else printf("            ");
        printf("%s\n", canvas[r]);
    }
    char labels[GRAPH_WIDTH + 1];
    memset(labels, ' ', GRAPH_WIDTH); labels[GRAPH_WIDTH] = '\0';
    int last_end=-1, omitted=0;
    for (int k = 0; k < nx; k++) {
        char label[32];
        snprintf(label, sizeof(label), "%g", xticks[k]);
        int len = (int)strlen(label);
        int pos = (int)round((xticks[k]-AXIS_LO)/(AXIS_HI-AXIS_LO)*(GRAPH_WIDTH-1)) - len/2;
        if (pos < 0) pos = 0;
        if (pos + len > GRAPH_WIDTH) pos = GRAPH_WIDTH - len;
        if (pos<=last_end) { omitted=1; continue; }
        memcpy(labels + pos, label, len);
        last_end=pos+len;
    }
    printf("            %s  %s\n", labels, h_name);
    printf("  Curves/axis lines: .   Axis scale marks: o every 1/3 unit (3 intervals = 1 unit)\n");
    if (AXIS_LO>0 || AXIS_HI<0 || VIEW_Y_LO>0 || VIEW_Y_HI<0)
        printf("  Reference axes at x=%g, y=%g (zero is outside this view).\n",fmax(AXIS_LO,fmin(0,AXIS_HI)),fmax(VIEW_Y_LO,fmin(0,VIEW_Y_HI)));
    if (omitted || AXIS_HI-AXIS_LO>GRAPH_WIDTH-1 || VIEW_Y_HI-VIEW_Y_LO>GRAPH_HEIGHT-1)
        printf("  Some unit labels are omitted to fit this view; narrow /range to see each unit.\n");
    printf("  Sampling: %d points per column (%d horizontal positions); fractional coordinates included.\n",graph2d_settings.samples,GRAPH_SAMPLES);
    if (clipped) printf("  Portions outside the visible range are clipped.\n");
    if (!visible) printf("  No real curve points in this view.\n");
}

/* Finds the free variables (excluding constants pi/e) across an
 * equation's two sides. Returns how many distinct names were found,
 * writing up to 8 into names[][]. */
int graph_free_vars(ASTNode *lhs, ASTNode *rhs, char names[][64]) {
    char all[8][64];
    int all_count = 0;
    ast_collect_vars(lhs, all, 8, &all_count);
    ast_collect_vars(rhs, all, 8, &all_count);
    int n = 0;
    for (int i = 0; i < all_count; i++)
        if (!symtab_is_constant(all[i])) memcpy(names[n++], all[i], 64);
    return n;
}

typedef struct { double fixed; int reverse, print; } Slice2D;
static void slice_point(double root,void *context) {
    Slice2D *s=context;
    if (s->print) printf("  (%s, %s)\n",format_number(s->fixed),format_number(root));
    else if (s->reverse) point(root,s->fixed);
    else point(s->fixed,root);
}
/* Include integer coordinates exactly, even when the dense sample lattice
 * does not land on them. Fractional samples still supply the fine detail. */
static void sample_unit_slices(ASTNode *lhs,ASTNode *rhs,const char *h,const char *v,int reverse) {
    double ticks[GRAPH_WIDTH];
    int n=unit_ticks(reverse?VIEW_Y_LO:AXIS_LO,reverse?VIEW_Y_HI:AXIS_HI,GRAPH_WIDTH,ticks);
    for (int i=0;i<n;i++) {
        Slice2D slice={ticks[i],reverse,0};
        symtab_set(reverse?v:h,slice.fixed);
        rootfind_visit(lhs,rhs,reverse?h:v,reverse?AXIS_LO:VIEW_Y_LO,
                       reverse?AXIS_HI:VIEW_Y_HI,graph2d_settings.samples*64,slice_point,&slice);
    }
}
/* Sweep both axes: vertical components and steep sections need the
 * reverse pass; streamed roots have no per-column branch limit. */
static void sample_equation(ASTNode *lhs,ASTNode *rhs,const char *h,const char *v) {
    int count=GRAPH_SAMPLES-1;
    for (int reverse=0;reverse<2;reverse++) {
        double lo=reverse?VIEW_Y_LO:AXIS_LO, hi=reverse?VIEW_Y_HI:AXIS_HI;
        for (int i=0;i<=count;i++) {
            Slice2D slice={lo+(hi-lo)*(i/(double)count),reverse,0};
            symtab_set(reverse?v:h,slice.fixed);
            rootfind_visit(lhs,rhs,reverse?h:v,reverse?AXIS_LO:VIEW_Y_LO,
                           reverse?AXIS_HI:VIEW_Y_HI,graph2d_settings.samples*64,slice_point,&slice);
        }
        sample_unit_slices(lhs,rhs,h,v,reverse);
    }
}
static void graph_function(ASTNode *rhs,int show_tac) {
    char names[8][64]; int n=graph_free_vars(rhs,NULL,names);
    if (n>1) { semantic_error("/graph2d supports one independent variable; use /graph3d for surfaces"); return; }
    const char *var=n?names[0]:"x";
    double saved_h=0,saved_v=0; int had_h=symtab_lookup(var,&saved_h),had_v=symtab_lookup("y",&saved_v);
    symtab_set(var,0); symtab_set("y",0);
    if (!eval_validate(rhs)) goto restore;
    if (show_tac) {
        TACProgram tac; tac_init(&tac); const char *operand=tac_build(&tac,rhs);
        tac_finish_graph(&tac,operand,var,AXIS_LO,AXIS_HI); printf("IR (TAC):\n"); tac_print(&tac);
    }
    clear_plot();
    int skipped=0;
    for (int i=0;i<GRAPH_SAMPLES;i++) {
        double x=AXIS_LO+(AXIS_HI-AXIS_LO)*(i/(double)(GRAPH_SAMPLES-1));
        symtab_set(var,x); int ok=1; double y=eval(rhs,1,&ok);
        if (ok && isfinite(y)) point(x,y); else skipped++;
    }
    ASTNode *y=ast_var("y");
    /* Supplement direct samples with roots along horizontal scan lines. */
    for (int i=0;i<=GRAPH_HEIGHT*graph2d_settings.samples;i++) {
        Slice2D slice={VIEW_Y_LO+(VIEW_Y_HI-VIEW_Y_LO)*(i/(double)(GRAPH_HEIGHT*graph2d_settings.samples)),1,0};
        symtab_set("y",slice.fixed);
        rootfind_visit(y,rhs,var,AXIS_LO,AXIS_HI,graph2d_settings.samples*64,slice_point,&slice);
    }
    sample_unit_slices(y,rhs,var,"y",1);
    ast_free(y);
    printf("Graph generated.\n\nSample points:\n");
    double ticks[GRAPH_WIDTH]; int nticks=unit_ticks(AXIS_LO,AXIS_HI,GRAPH_WIDTH,ticks);
    for (int i=0;i<nticks;i++) {
        double x=ticks[i]; symtab_set(var,x);
        int ok=1; double value=eval(rhs,1,&ok);
        if (ok && isfinite(value)) point(x,value);
        printf("  %s = %-6s -> y = %s\n",var,format_number(x),ok&&isfinite(value)?format_number(value):"(undefined)");
    }
    render(var,"y");
    if (skipped) printf("\n(%d of %d sample points were outside the real-valued domain and skipped.)\n",skipped,GRAPH_SAMPLES);
restore:
    if (had_h) symtab_set(var,saved_h); else symtab_unset(var);
    if (had_v) symtab_set("y",saved_v); else symtab_unset("y");
}

typedef struct {
    char line[GRAPH_WIDTH+1];
    const char *name;
    int count;
} NumberLine;
static void number_line_root(double root,void *context) {
    NumberLine *line=context;
    int col=(int)round((root-AXIS_LO)/(AXIS_HI-AXIS_LO)*(GRAPH_WIDTH-1));
    if (col<0 || col>=GRAPH_WIDTH) return;
    line->line[col]='*';
    if (line->count<64) printf("%s = %s\n",line->name,format_number(root));
    line->count++;
}
/* One-variable equations remain number lines, with the same configurable
 * viewport and uncapped streamed root detection as the curve renderer. */
static void graph_equation_1var(ASTNode *lhs,ASTNode *rhs,const char *name,int show_tac) {
    double saved=0; int had=symtab_lookup(name,&saved);
    symtab_set(name,0);
    if (!eval_validate(lhs) || !eval_validate(rhs)) goto restore;
    if (show_tac) {
        TACProgram tac; tac_init(&tac); ASTNode *diff=ast_binop('-',lhs,rhs);
        const char *operand=tac_build(&tac,diff); tac_finish_eqn(&tac,operand); free(diff);
        printf("IR (TAC):\n"); tac_print(&tac);
    }
    NumberLine line={.name=name,.count=0};
    memset(line.line,'.',GRAPH_WIDTH); line.line[GRAPH_WIDTH]=0;
    double marks[GRAPH_WIDTH*2]; int n=graph_axis_marks(AXIS_LO,AXIS_HI,GRAPH_WIDTH*2,marks);
    for (int i=0;i<n;i++) line.line[(int)round((marks[i]-AXIS_LO)/(AXIS_HI-AXIS_LO)*(GRAPH_WIDTH-1))]='o';
    printf("Roots in the visible range:\n");
    rootfind_visit(lhs,rhs,name,AXIS_LO,AXIS_HI,graph2d_settings.samples*256,number_line_root,&line);
    if (!line.count) printf("No solution found for '%s' in the search range [%g, %g].\n",name,AXIS_LO,AXIS_HI);
    if (line.count>64) printf("(First 64 detected roots listed; all %d detected points drawn.)\n",line.count);
    printf("\nNumber line (%s from %g to %g, '.' forms the axis, 'o' marks 1/3 units, '*' marks a root):\n  %s\n",name,AXIS_LO,AXIS_HI,line.line);
restore:
    if (had) symtab_set(name,saved); else symtab_unset(name);
}

static void graph_equation_2var(ASTNode *lhs,ASTNode *rhs,const char *h,const char *v,int show_tac) {
    double saved_h=0,saved_v=0; int had_h=symtab_lookup(h,&saved_h),had_v=symtab_lookup(v,&saved_v);
    symtab_set(h,0); symtab_set(v,0);
    if (!eval_validate(lhs) || !eval_validate(rhs)) goto restore;
    if (show_tac) {
        TACProgram tac; tac_init(&tac); ASTNode *diff=ast_binop('-',lhs,rhs);
        const char *operand=tac_build(&tac,diff); tac_finish_eqn(&tac,operand); free(diff);
        printf("IR (TAC):\n"); tac_print(&tac);
    }
    clear_plot(); sample_equation(lhs,rhs,h,v);
    printf("Graph generated (implicit relation in %s and %s).\n\nSample points (%s, %s):\n",h,v,h,v);
    double ticks[GRAPH_WIDTH]; int nticks=unit_ticks(AXIS_LO,AXIS_HI,GRAPH_WIDTH,ticks);
    for (int i=0;i<nticks;i++) {
        Slice2D slice={ticks[i],0,1}; symtab_set(h,slice.fixed);
        rootfind_visit(lhs,rhs,v,VIEW_Y_LO,VIEW_Y_HI,graph2d_settings.samples*64,slice_point,&slice);
    }
    render(h,v);
restore:
    if (had_h) symtab_set(h,saved_h); else symtab_unset(h);
    if (had_v) symtab_set(v,saved_v); else symtab_unset(v);
}

void graph_run(ASTNode *stmt, int show_tac) {
    if (stmt->kind != N_BINOP || stmt->op != '=') {
        char names[8][64];
        int n = graph_free_vars(stmt, NULL, names);
        if (n != 1) {
            semantic_error("/graph2d expects a single-variable expression or an equation, e.g. y^2, y^3 = x, or y = x^2");
            return;
        }
        if (strcmp(names[0], "y") == 0) {
            /* A bare function of y means x = f(y). Borrow stmt;
             * only the temporary x node belongs to this call. */
            ASTNode *x = ast_var("x");
            graph_equation_2var(x, stmt, "x", "y", show_tac);
            ast_free(x);
        } else {
            graph_function(stmt, show_tac);
        }
        return;
    }
    ASTNode *lhs = stmt->left;
    ASTNode *rhs = stmt->right;

    /* The common, documented shape: an explicit function of one
       variable assigned to 'y'. Kept as its own fast path since it
       is the primary, best-tested feature. */
    char rhs_names[8][64];
    int rhs_count = graph_free_vars(rhs, NULL, rhs_names);
    int rhs_has_y = 0;
    for (int i = 0; i < rhs_count; i++)
        if (strcmp(rhs_names[i], "y") == 0) rhs_has_y = 1;
    if (lhs->kind == N_VAR && strcmp(lhs->name, "y") == 0 && !rhs_has_y) {
        graph_function(rhs, show_tac);
        return;
    }

    /* Otherwise: a general equation. Look at how many different
       variables it actually uses and dispatch accordingly. */
    char names[8][64];
    int n = graph_free_vars(lhs, rhs, names);

    if (n == 0) {
        semantic_error("/graph2d needs at least one variable to plot");
    } else if (n == 1) {
        graph_equation_1var(lhs, rhs, names[0], show_tac);
    } else if (n == 2) {
        const char *h = strcmp(names[0], "x") == 0 ? names[0] : strcmp(names[1], "x") == 0 ? names[1] : names[0];
        const char *v = (h == names[0]) ? names[1] : names[0];
        graph_equation_2var(lhs, rhs, h, v, show_tac);
    } else {
        semantic_error("/graph2d supports at most two variables, found %d", n);
    }
}
