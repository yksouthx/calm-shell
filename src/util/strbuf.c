#include "util/strbuf.h"

#include <stdlib.h>
#include <string.h>

#include "util/xalloc.h"

void strbuf_init(StrBuf *sb) {
    sb->data = xmalloc(1);
    sb->data[0] = '\0';
    sb->len = 0;
    sb->cap = 1;
}

static void strbuf_reserve(StrBuf *sb, size_t extra) {
    size_t needed = sb->len + extra + 1;
    if (needed <= sb->cap) {
        return;
    }
    size_t new_cap = sb->cap == 0 ? 16 : sb->cap;
    while (new_cap < needed) {
        new_cap *= 2;
    }
    sb->data = xrealloc(sb->data, new_cap);
    sb->cap = new_cap;
}

void strbuf_append_n(StrBuf *sb, const char *s, size_t n) {
    strbuf_reserve(sb, n);
    memcpy(sb->data + sb->len, s, n);
    sb->len += n;
    sb->data[sb->len] = '\0';
}

void strbuf_append(StrBuf *sb, const char *s) {
    strbuf_append_n(sb, s, strlen(s));
}

void strbuf_append_char(StrBuf *sb, char c) {
    strbuf_reserve(sb, 1);
    sb->data[sb->len++] = c;
    sb->data[sb->len] = '\0';
}

char *strbuf_take(StrBuf *sb) {
    char *out = sb->data;
    sb->data = NULL;
    sb->len = 0;
    sb->cap = 0;
    return out;
}

void strbuf_free(StrBuf *sb) {
    free(sb->data);
    sb->data = NULL;
    sb->len = 0;
    sb->cap = 0;
}
