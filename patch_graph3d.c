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
   The user can change it freely inside the browser - no REPL prompt needed. */
#define DEFAULT_ROTATION_AXIS 'y'
#define DEFAULT_ROTATION_DIRECTION 1

typedef struct { double v[3]; } Point3;
typedef struct {
    Point3 *points;
    size_t count, capacity;
    Point3 current;
    int axis, failed;
} Cloud;

static void add_point(double root, void *context) {
    Cloud *c = context;
    if (c->failed || !isfinite(root)) return;
    for (int i=0; i<3; i++) if (i != c->axis && !isfinite(c->current.v[i])) return;
    if (c->count == c->capacity) {
        size_t capacity = c->capacity ? c->capacity * 2 : 8192;
        Point3 *p = realloc(c->points, capacity * sizeof(*p));
        if (!p) { c->failed = 1; return; }
        c->points = p; c->capacity = capacity;
    }
    c->current.v[c->axis] = root;
    c->points[c->count++] = c->current;
}

static int compare_points(const void *a, const void *b) {
    const Point3 *p = a, *q = b;
    for (int i=0; i<3; i++) {
        if (p->v[i] < q->v[i]) return -1;
        if (p->v[i] > q->v[i]) return 1;
    }
    return 0;
}

static void project(Point3 p, double *u, double *v, double *depth) {
    double scale = 0;
    for (int i=0; i<3; i++) scale = fmax(scale, graph3d_settings.hi[i] - graph3d_settings.lo[i]);
    for (int i=0; i<3; i++) p.v[i] = (p.v[i] - (graph3d_settings.lo[i]/2 + graph3d_settings.hi[i]/2)) / scale;
    *u = (p.v[0] + p.v[2]) * 0.7071067811865475;
    *v = p.v[1] * 0.816496580927726 + (p.v[0] - p.v[2]) * 0.408248290463863;
    *depth = (-p.v[0] + p.v[1] + p.v[2]) * 0.577350269189626;
}

#define WIDTH3 120
#define HEIGHT3 45

static void preview(const Cloud *c, char names[][64]) {
    char canvas[HEIGHT3][WIDTH3+1]; double depths[HEIGHT3][WIDTH3];
    for (int r=0; r<HEIGHT3; r++) {
        memset(canvas[r], ' ', WIDTH3); canvas[r][WIDTH3] = 0;
        for (int col=0; col<WIDTH3; col++) depths[r][col] = -INFINITY;
    }
    double near = -INFINITY, far = INFINITY;
    for (size_t i=0; i<c->count; i++) {
        double u, v, d; project(c->points[i], &u, &v, &d);
        near = fmax(near, d); far = fmin(far, d);
    }
    for (size_t i=0; i<c->count; i++) {
        double u, v, d; project(c->points[i], &u, &v, &d);
        int col = (int)round((WIDTH3-1)/2.0 + u*50), r = (int)round((HEIGHT3-1)/2.0 - v*25);
        if (r < 0 || r >= HEIGHT3 || col < 0 || col >= WIDTH3 || d < depths[r][col]) continue;
        depths[r][col] = d;
        int shade = near > far ? (int)((d-far)/(near-far)*4) : 2; 
        if (shade < 0) shade = 0; if (shade > 4) shade = 4;
        canvas[r][col] = ":oO@#"[shade];
    }
    /* Dotted reference axes pass through zero whenever it is visible. */
    for (int axis=0; axis<3; axis++) {
        Point3 p;
        for (int j=0; j<3; j++) p.v[j] = fmax(graph3d_settings.lo[j], fmin(0, graph3d_settings.hi[j]));
        /* Oversample the projected line so every crossed terminal cell
         * receives a dot, independently of mathematical unit spacing. */
        const int samples = WIDTH3 * 8;
        for (int k=0; k<=samples; k++) {
            p.v[axis] = graph3d_settings.lo[axis] + (graph3d_settings.hi[axis] - graph3d_settings.lo[axis]) * (k / (double)samples);
            double u, v, d; project(p, &u, &v, &d);
            int col = (int)round((WIDTH3-1)/2.0 + u*50), r = (int)round((HEIGHT3-1)/2.0 - v*25);
            if (r >= 0 && r < HEIGHT3 && col >= 0 && col < WIDTH3) canvas[r][col] = '.';
        }
        double marks[WIDTH3*8];
        int n = graph_axis_marks(graph3d_settings.lo[axis], graph3d_settings.hi[axis], WIDTH3*8, marks);
        for (int i=0; i<n; i++) {
            p.v[axis] = marks[i]; double u, v, d; project(p, &u, &v, &d);
            int col = (int)round((WIDTH3-1)/2.0 + u*50), row = (int)round((HEIGHT3-1)/2.0 - v*25);
            if (row >= 0 && row < HEIGHT3 && col >= 0 && col < WIDTH3) canvas[row][col] = 'o';
        }
        p.v[axis] = graph3d_settings.hi[axis];
        double u, v, d; project(p, &u, &v, &d);
        int col = 2 + (int)round((WIDTH3-1)/2.0 + u*50), r = (int)round((HEIGHT3-1)/2.0 - v*25);
        if (r >= 0 && r < HEIGHT3 && col >= 0 && col < WIDTH3) canvas[r][col] = "XYZ"[axis];
    }
    printf("\n3D isometric preview | %d x %d canvas\n", WIDTH3, HEIGHT3);
    for (int r=0; r<HEIGHT3; r++) printf("  %s\n", canvas[r]);
    printf("  Axes X=%s, Y=%s, Z=%s; axes: dotted lines, o at 1/3-unit intervals; : o O @ # shade by depth.\n", names[0], names[1], names[2]);
}

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
    
    Cloud cloud = {0};

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
    
    int samples = graph3d_settings.samples;
    /* Each axis becomes the solved variable in turn. This finds both
     * halves of spheres, vertical planes/cylinders and multiple sheets. */
    for (int axis = 0; axis < 3 && !cloud.failed; axis++) {
        cloud.axis = axis; int a = (axis + 1) % 3, b = (axis + 2) % 3;
        for (int i = 0; i <= samples && !cloud.failed; i++) {
            for (int j = 0; j <= samples && !cloud.failed; j++) {
                cloud.current.v[a] = graph3d_settings.lo[a] + (graph3d_settings.hi[a] - graph3d_settings.lo[a]) * (i / (double)samples);
                cloud.current.v[b] = graph3d_settings.lo[b] + (graph3d_settings.hi[b] - graph3d_settings.lo[b]) * (j / (double)samples);
                symtab_set(names[a], cloud.current.v[a]); symtab_set(names[b], cloud.current.v[b]);
                rootfind_visit(lhs, rhs, names[axis], graph3d_settings.lo[axis], graph3d_settings.hi[axis], samples * 4, add_point, &cloud);
            }
        }
    }

    if (cloud.failed) { 
        printf("Unable to allocate the 3D point cloud; reduce /samples.\n"); 
        goto cleanup; 
    }
    if (cloud.count) {
        qsort(cloud.points, cloud.count, sizeof(Point3), compare_points);
        size_t unique = 1;
        for (size_t i = 1; i < cloud.count; i++) 
            if (compare_points(cloud.points + i, cloud.points + unique - 1)) 
                cloud.points[unique++] = cloud.points[i];
        cloud.count = unique;
    }

    printf("3D graph generated (implicit surface in %s, %s, %s).\n", names[0], names[1], names[2]);
    printf("Range: %s [%g, %g], %s [%g, %g], %s [%g, %g].\n",
           names[0], graph3d_settings.lo[0], graph3d_settings.hi[0],
           names[1], graph3d_settings.lo[1], graph3d_settings.hi[1],
           names[2], graph3d_settings.lo[2], graph3d_settings.hi[2]);
           
    printf("%zu sampled surface points; %d subdivisions per axis, scans along all three axes.\n", cloud.count, samples);
    if (!cloud.count) printf("No real surface points found in this view.\n");

    preview(&cloud, names);
    
    {
        char axis[2] = {DEFAULT_ROTATION_AXIS, 0};
        graph_export_view(3, lhs, rhs, names, &graph3d_settings,
                          "surface", axis, DEFAULT_ROTATION_DIRECTION);
    }
    printf("Open the HTML to explore the interactive 3D view (rotation controlled in browser).\n");

cleanup:
    free(cloud.points);
    ast_free(owned);
    for (int i = 0; i < 3; i++)
        if (had[i])
            symtab_set(names[i], saved[i]);
        else
            symtab_unset(names[i]);
}
