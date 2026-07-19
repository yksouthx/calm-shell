/* Allocation wrappers.
 *
 * Calm-shell is a small interactive tool, not a long-running server: an
 * allocation failure means the machine is critically out of memory, and
 * there's no graceful degradation worth offering. These abort with a
 * clear message instead of every call site needing its own NULL check.
 */
#ifndef CALM_UTIL_XALLOC_H
#define CALM_UTIL_XALLOC_H

#include <stddef.h>

void *xmalloc(size_t size);
void *xrealloc(void *ptr, size_t size);
char *xstrdup(const char *s);
char *xstrndup(const char *s, size_t n);

/* printf-style formatting into a newly allocated, NUL-terminated
 * string. Never returns NULL. */
char *xsprintf(const char *fmt, ...)
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((format(printf, 1, 2)))
#endif
    ;

#endif /* CALM_UTIL_XALLOC_H */
