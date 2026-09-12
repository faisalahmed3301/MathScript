#ifndef SYMTAB_H
#define SYMTAB_H

/* A tiny symbol table: a flat array is plenty for a REPL that
   rarely holds more than a few variables and constants. */
#define MAX_SYMBOLS 256

typedef struct {
    char   name[64];
    double value;
    int    is_constant; /* pi, e -> 1 (cannot be reassigned) */
} Symbol;

void    symtab_init(void);           /* registers pi, e */
int     symtab_lookup(const char *name, double *out_value);
void    symtab_set(const char *name, double value);
void    symtab_dump(void); /* used by /symbols */

#endif
