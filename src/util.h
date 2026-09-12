#ifndef UTIL_H
#define UTIL_H

/* Formats v the way the example transcripts in the project
   report do: whole numbers print without a trailing ".0"
   (sqrt(25) -> "5", not "5.0"), everything else uses %g. */
const char *format_number(double v);

#endif
