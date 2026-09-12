#ifndef ERRORS_H
#define ERRORS_H

/* One flag per error class named in the assignment brief:
   lexical, syntax, semantic, and mathematical errors. Only one
   is ever set per line -- whichever stage fails first stops the
   pipeline, so later stages never run on bad input. */
extern int g_lex_error;
extern int g_syntax_error;
extern int g_semantic_error;
extern int g_math_error;

void errors_reset(void);
int  errors_any(void);

void lex_error(const char *fmt, ...);
void syntax_error(const char *fmt, ...);
void semantic_error(const char *fmt, ...);
void math_error(const char *fmt, ...);

#endif
