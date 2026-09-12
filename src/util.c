#include <stdio.h>
#include <math.h>
#include "util.h"

const char *format_number(double v) {
    /* A small rotating pool -- not just one static buffer -- so
       that printf("%s .. %s", format_number(a), format_number(b))
       does not silently make both conversions print the same
       (last-written) text. */
    static char pool[8][64];
    static int i = 0;
    i = (i + 1) % 8;
    char *buf = pool[i];
    if (fabs(v - (long long)v) < 1e-9)
        snprintf(buf, 64, "%lld", (long long)v);
    else
        snprintf(buf, 64, "%.6g", v);
    return buf;
}
