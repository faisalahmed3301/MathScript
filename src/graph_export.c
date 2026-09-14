#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <time.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#include <process.h>
#include <windows.h>
#define process_id _getpid
#else
#include <unistd.h>
#define process_id getpid
#endif
#include "graph_export.h"
#include "symtab.h"
#include "graph3d_viewer.h"
#include "graph2d_viewer.h"

/* Output layout
   output/
     All2dGraphs/          <- every 2-D graph archive
     All3dGraphs/          <- every 3-D graph archive
     LiveGraphs/
       MathScriptLiveGraph.html   <- always the most-recently generated graph
*/
#define LIVE_FOLDER "output/LiveGraphs"
#define LIVE_FILE LIVE_FOLDER "/MathScriptLiveGraph.html"

static int ensure_directory(const char *path)
{
#ifdef _WIN32
    if (_mkdir(path) == 0)
        return 1;
#else
    if (mkdir(path, 0755) == 0)
        return 1;
#endif
    struct stat info;
    return errno == EEXIST && stat(path, &info) == 0 && (info.st_mode & S_IFDIR) != 0;
}

static void write_model(FILE *fp, const ASTNode *node, char names[][64])
{
    if (node->kind == N_NUM)
    {
        if (isfinite(node->num))
            fprintf(fp, "%.17g", node->num);
        else
            fputs("NaN", fp);
        return;
    }
    if (node->kind == N_VAR)
    {
        for (int a = 0; a < 3; a++)
            if (!strcmp(node->name, names[a]))
            {
                fprintf(fp, "[\"var\",%d]", a);
                return;
            }
        double value = 0;
        symtab_lookup(node->name, &value);
        if (isfinite(value))
            fprintf(fp, "%.17g", value);
        else
            fputs("NaN", fp);
        return;
    }
    if (node->kind == N_UMINUS)
    {
        fputs("[\"neg\",", fp);
        write_model(fp, node->left, names);
        fputc(']', fp);
        return;
    }
    if (node->kind == N_CALL)
    {
        fprintf(fp, "[\"%s\"", node->name);
        for (int i = 0; i < node->argc; i++)
        {
            fputc(',', fp);
            write_model(fp, node->args[i], names);
        }
        fputc(']', fp);
        return;
    }
    const char *op = node->op == OP_GE ? ">=" : node->op == OP_LE ? "<="
                                            : node->op == OP_EQ   ? "=="
                                            : node->op == OP_NE   ? "!="
                                                                  : NULL;
    if (op)
        fprintf(fp, "[\"%s\",", op);
    else
        fprintf(fp, "[\"%c\",", node->op);
    write_model(fp, node->left, names);
    fputc(',', fp);
    write_model(fp, node->right, names);
    fputc(']', fp);
}

/* Atomically copy archive -> live file via a temp path */
static int publish_latest(const char *archive, const char *temporary, const char *latest)
{
    FILE *in = fopen(archive, "rb");
    if (!in)
        return 0;
    FILE *out = fopen(temporary, "wbx");
    if (!out)
    {
        fclose(in);
        return 0;
    }
    char buffer[16384];
    size_t n;
    int ok = 1;
    while ((n = fread(buffer, 1, sizeof(buffer), in)) > 0)
        if (fwrite(buffer, 1, n, out) != n)
        {
            ok = 0;
            break;
        }
    if (ferror(in))
        ok = 0;
    fclose(in);
    if (fclose(out) != 0)
        ok = 0;
    if (ok)
    {
#ifdef _WIN32
        ok = MoveFileExA(temporary, latest, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
        ok = rename(temporary, latest) == 0;
#endif
    }
    if (!ok)
        remove(temporary);
    return ok;
}

int graph_export_view(int dimensions, ASTNode *lhs, ASTNode *rhs, char names[][64],
                      const GraphSettings *settings, const char *kind,
                      const char *rotation_axis, int rotation_direction)
{

    /* Choose the right archive folder: All2dGraphs or All3dGraphs */
    char archive_folder[64];
    snprintf(archive_folder, sizeof(archive_folder),
             "output/%s", dimensions == 2 ? "All2dGraphs" : "All3dGraphs");

    /* Ensure output/, archive folder, and LiveGraphs/ all exist */
    if (!ensure_directory("output") ||
        !ensure_directory(archive_folder) ||
        !ensure_directory(LIVE_FOLDER))
    {
        fprintf(stderr, "Could not create output directories: %s\n", strerror(errno));
        return 0;
    }

    /* Create the archive file (unique name) */
    static unsigned serial = 0;
    char graph_id[128];
    char archive[256];
    FILE *fp = NULL;
    for (int attempt = 0; attempt < 1000; attempt++)
    {
        snprintf(graph_id, sizeof(graph_id),
                 "graph%dd_%lld_%ld_%04u",
                 dimensions,
                 (long long)time(NULL), (long)process_id(), ++serial);
        snprintf(archive, sizeof(archive),
                 "%s/%s.html",
                 archive_folder, graph_id);
        fp = fopen(archive, "wx");
        if (fp || errno != EEXIST)
            break;
    }
    if (!fp)
    {
        fprintf(stderr, "Could not create graph archive: %s\n", strerror(errno));
        return 0;
    }

    /* Write the self-contained HTML */
    fputs(dimensions == 3 ? GRAPH3D_HTML_HEAD : GRAPH2D_HTML_HEAD, fp);
    fprintf(fp, "\nconst names=['%s','%s','%s'],lo=[%.17g,%.17g,%.17g],hi=[%.17g,%.17g,%.17g];\n",
            names[0], names[1], names[2],
            settings->lo[0], settings->lo[1], settings->lo[2],
            settings->hi[0], settings->hi[1], settings->hi[2]);
    fprintf(fp, "const graphKind='%s',requestedDetail=%d;\n", kind, settings->samples);
    fprintf(fp, "const rotation={axis:'%s',direction:%d};\n", rotation_axis, rotation_direction);
    fprintf(fp, "const liveVersion='%s';\n", graph_id);
    fputs("const model={lhs:", fp);
    write_model(fp, lhs, names);
    fputs(",rhs:", fp);
    write_model(fp, rhs, names);
    fputs("};\n", fp);
    fputs(dimensions == 3 ? GRAPH3D_HTML_TAIL : GRAPH2D_HTML_TAIL, fp);
    int failed = ferror(fp);
    if (fclose(fp) != 0)
        failed = 1;
    if (failed)
    {
        remove(archive);
        fprintf(stderr, "Could not finish graph archive.\n");
        return 0;
    }

    /* Atomically update LiveGraphs/MathScriptLiveGraph.html */
    char temporary[256];
    snprintf(temporary, sizeof(temporary),
             "%s/.live_%ld_%u.tmp", LIVE_FOLDER, (long)process_id(), serial);
    if (!publish_latest(archive, temporary, LIVE_FILE))
    {
        fprintf(stderr, "Archive saved, but live graph could not be updated: %s\n", archive);
        return 0;
    }

    /* Update live watcher files so open browsers auto-refresh */
    char ping_file[256], json_file[256], js_file[256];
    snprintf(ping_file, sizeof(ping_file), "%s/live_ping.html", LIVE_FOLDER);
    snprintf(json_file, sizeof(json_file), "%s/live_version.json", LIVE_FOLDER);
    snprintf(js_file, sizeof(js_file), "%s/live_version.js", LIVE_FOLDER);

    FILE *ping_fp = fopen(ping_file, "w");
    if (ping_fp)
    {
        fprintf(ping_fp,
                "<!doctype html><html><head><meta charset=\"utf-8\"><title>Live Watcher</title></head><body>"
                "<script>\n"
                "var v=\"%s\";\n"
                "try{if(window.parent&&window.parent!==window){window.parent.postMessage({mathscript_live:v},\"*\");}}catch(e){}\n"
                "setTimeout(function(){location.reload();},600);\n"
                "</script></body></html>\n",
                graph_id);
        fclose(ping_fp);
    }

    FILE *json_fp = fopen(json_file, "w");
    if (json_fp)
    {
        fprintf(json_fp, "{\"version\":\"%s\",\"timestamp\":%lld}\n",
                graph_id, (long long)time(NULL));
        fclose(json_fp);
    }

    FILE *js_fp = fopen(js_file, "w");
    if (js_fp)
    {
        fprintf(js_fp, "window.__MATHSCRIPT_LIVE_VERSION__=\"%s\";\n", graph_id);
        fclose(js_fp);
    }

    printf("Live graph:     %s\nArchived graph: %s\n", LIVE_FILE, archive);
    return 1;
}
