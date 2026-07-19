/* A growable, NUL-terminated byte buffer for building strings without
 * repeated strlen()+realloc() at every call site. Amortized O(1)
 * append via geometric growth. */
#ifndef CALM_UTIL_STRBUF_H
#define CALM_UTIL_STRBUF_H

#include <stddef.h>

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} StrBuf;

void strbuf_init(StrBuf *sb);
void strbuf_append(StrBuf *sb, const char *s);
void strbuf_append_n(StrBuf *sb, const char *s, size_t n);
void strbuf_append_char(StrBuf *sb, char c);

/* Returns the accumulated, NUL-terminated string, transferring
 * ownership to the caller (who must free() it), and resets `sb` to a
 * fresh, empty, still-usable buffer. */
char *strbuf_take(StrBuf *sb);

void strbuf_free(StrBuf *sb);

#endif /* CALM_UTIL_STRBUF_H */
