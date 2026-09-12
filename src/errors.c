#include <stdio.h>
#include <stdarg.h>
#include "errors.h"

int g_lex_error = 0;
int g_syntax_error = 0;
int g_semantic_error = 0;
int g_math_error = 0;

void errors_reset(void) {
    g_lex_error = 0;
    g_syntax_error = 0;
    g_semantic_error = 0;
    g_math_error = 0;
}

int errors_any(void) {
    return g_lex_error || g_syntax_error || g_semantic_error || g_math_error;
}

static void report(const char *category, const char *fmt, va_list ap) {
    printf("%s Error:\n  ", category);
    vprintf(fmt, ap);
    printf("\n");
}

void lex_error(const char *fmt, ...) {
    g_lex_error = 1;
    va_list ap; va_start(ap, fmt);
    report("Lexical", fmt, ap);
    va_end(ap);
}

void syntax_error(const char *fmt, ...) {
    g_syntax_error = 1;
    va_list ap; va_start(ap, fmt);
    report("Syntax", fmt, ap);
    va_end(ap);
}

void semantic_error(const char *fmt, ...) {
    g_semantic_error = 1;
    va_list ap; va_start(ap, fmt);
    report("Semantic", fmt, ap);
    va_end(ap);
}

void math_error(const char *fmt, ...) {
    g_math_error = 1;
    va_list ap; va_start(ap, fmt);
    report("Math", fmt, ap);
    va_end(ap);
}
