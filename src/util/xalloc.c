#include "util/xalloc.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void die_out_of_memory(void) {
    fputs("calm: out of memory\n", stderr);
    exit(1);
}

void *xmalloc(size_t size) {
    /* malloc(0) is implementation-defined (may return NULL); round up
     * so a zero-length allocation is always a valid, distinct pointer
     * the caller can free() normally. */
    void *p = malloc(size != 0 ? size : 1);
    if (!p) {
        die_out_of_memory();
    }
    return p;
}

void *xrealloc(void *ptr, size_t size) {
    void *p = realloc(ptr, size != 0 ? size : 1);
    if (!p) {
        die_out_of_memory();
    }
    return p;
}

char *xstrdup(const char *s) {
    size_t len = strlen(s);
    char *out = xmalloc(len + 1);
    memcpy(out, s, len + 1);
    return out;
}

char *xstrndup(const char *s, size_t n) {
    char *out = xmalloc(n + 1);
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

char *xsprintf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    va_list args_copy;
    va_copy(args_copy, args);
    int needed = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);
    if (needed < 0) {
        va_end(args);
        return xstrdup("");
    }
    char *out = xmalloc((size_t)needed + 1);
    vsnprintf(out, (size_t)needed + 1, fmt, args);
    va_end(args);
    return out;
}
