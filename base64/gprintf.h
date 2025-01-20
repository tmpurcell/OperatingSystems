/* This header exposes the gprintf function for printing formatted
 * debugging diagnostic information
 */
#pragma once
#include <stdarg.h>

void vgprintf(char const *file, int line, char const *func, char const *fmt, va_list ap);
void gprintf(char const *file, int line, char const *func, char const *fmt, ...);

#ifdef NDEBUG
# define vgprintf(fmt, ap) ((void)(0))
# define gprintf(fmt, ...) ((void)(0))
#else 
# define vgprintf(fmt, ap) vgprintf(__FILE__, __LINE__, __func__, fmt, ap)
# define gprintf(fmt, ...) gprintf(__FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#endif
