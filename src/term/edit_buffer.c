#include "term/edit_buffer.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "util/xalloc.h"

void editbuf_init(EditBuf *b) {
    b->cap = 128;
    b->data = xmalloc(b->cap);
    b->data[0] = '\0';
    b->len = 0;
    b->cursor = 0;
}

void editbuf_free(EditBuf *b) {
    free(b->data);
    b->data = NULL;
    b->len = 0;
    b->cap = 0;
    b->cursor = 0;
}

void editbuf_set(EditBuf *b, const char *s) {
    size_t n = strlen(s);
    if (n + 1 > b->cap) {
        b->cap = n + 1;
        b->data = xrealloc(b->data, b->cap);
    }
    memcpy(b->data, s, n + 1);
    b->len = n;
    b->cursor = n;
}

void editbuf_insert(EditBuf *b, char c) {
    if (b->len + 2 > b->cap) {
        b->cap *= 2;
        b->data = xrealloc(b->data, b->cap);
    }
    memmove(b->data + b->cursor + 1, b->data + b->cursor, b->len - b->cursor + 1);
    b->data[b->cursor] = c;
    b->len++;
    b->cursor++;
}

void editbuf_delete_before_cursor(EditBuf *b) {
    if (b->cursor == 0) {
        return;
    }
    memmove(b->data + b->cursor - 1, b->data + b->cursor, b->len - b->cursor + 1);
    b->len--;
    b->cursor--;
}

void editbuf_delete_at_cursor(EditBuf *b) {
    if (b->cursor >= b->len) {
        return;
    }
    memmove(b->data + b->cursor, b->data + b->cursor + 1, b->len - b->cursor);
    b->len--;
}

void editbuf_kill_to_end(EditBuf *b) {
    b->data[b->cursor] = '\0';
    b->len = b->cursor;
}

void editbuf_kill_whole_line(EditBuf *b) {
    b->data[0] = '\0';
    b->len = 0;
    b->cursor = 0;
}

void editbuf_kill_word_before(EditBuf *b) {
    size_t end = b->cursor;
    size_t start = end;
    while (start > 0 && isspace((unsigned char)b->data[start - 1])) {
        start--;
    }
    while (start > 0 && !isspace((unsigned char)b->data[start - 1])) {
        start--;
    }
    memmove(b->data + start, b->data + end, b->len - end + 1);
    b->len -= (end - start);
    b->cursor = start;
}
