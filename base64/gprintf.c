#include <stdio.h>

#include "gprintf.h"
/* Formatted debugging output */

#undef vgprintf
#undef gprintf

void vgprintf(char const *file, int line, char const *func, char const *fmt, va_list ap)
{
  fprintf(stderr, "%s:%d: %s(): ", file, line, func);
  vfprintf(stderr, fmt, ap);
}

void gprintf(char const *file, int line, char const *func, char const *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  vgprintf(file, line, func, fmt, ap);
  va_end(ap);
}
