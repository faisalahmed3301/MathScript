#include <stdio.h>
#include <string.h>
#include <math.h>
#include "symtab.h"

static Symbol table[MAX_SYMBOLS];
static int count = 0;

static int find(const char *name) {
    for (int i = 0; i < count; i++)
        if (strcmp(table[i].name, name) == 0) return i;
    return -1;
}

void symtab_init(void) {
    count = 0;
    symtab_set("pi", M_PI);
    symtab_set("e", M_E);
    table[find("pi")].is_constant = 1;
    table[find("e")].is_constant = 1;
}

int symtab_lookup(const char *name, double *out_value) {
    int i = find(name);
    if (i < 0) return 0;
    *out_value = table[i].value;
    return 1;
}

void symtab_set(const char *name, double value) {
    int i = find(name);
    if (i >= 0) {
        table[i].value = value;
        return;
    }
    if (count >= MAX_SYMBOLS) {
        fprintf(stderr, "Symbol Table Error: table full\n");
        return;
    }
    strncpy(table[count].name, name, sizeof(table[count].name) - 1);
    table[count].value = value;
    table[count].is_constant = 0;
    count++;
}

void symtab_dump(void) {
    printf("+----------------------+---------------+\n");
    printf("| Name                 | Value         |\n");
    printf("+----------------------+---------------+\n");
    for (int i = 0; i < count; i++)
        printf("| %-20s | %-13g |\n", table[i].name, table[i].value);
    printf("+----------------------+---------------+\n");
}
