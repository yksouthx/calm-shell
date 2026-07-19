#include "util/strutil.h"

#include <stdlib.h>
#include <string.h>

#include "util/xalloc.h"

bool starts_with(const char *s, const char *prefix) {
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

bool ends_with(const char *s, const char *suffix) {
    size_t slen = strlen(s);
    size_t suflen = strlen(suffix);
    if (suflen > slen) {
        return false;
    }
    return strcmp(s + (slen - suflen), suffix) == 0;
}

bool is_blank(const char *s) {
    for (const char *p = s; *p; p++) {
        if (*p != ' ' && *p != '\t' && *p != '\r' && *p != '\n') {
            return false;
        }
    }
    return true;
}

static bool is_ws(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f';
}

void trim_in_place(char *s) {
    size_t len = strlen(s);
    size_t start = 0;
    while (start < len && is_ws(s[start])) {
        start++;
    }
    size_t end = len;
    while (end > start && is_ws(s[end - 1])) {
        end--;
    }
    size_t new_len = end - start;
    if (start > 0) {
        memmove(s, s + start, new_len);
    }
    s[new_len] = '\0';
}

void rtrim_in_place(char *s) {
    size_t len = strlen(s);
    while (len > 0 && is_ws(s[len - 1])) {
        len--;
    }
    s[len] = '\0';
}

char **split_lines(const char *input, size_t *out_count) {
    size_t cap = 16, count = 0;
    char **lines = xmalloc(cap * sizeof(char *));

    const char *p = input;
    while (*p) {
        const char *start = p;
        while (*p && *p != '\n') {
            p++;
        }
        size_t line_len = (size_t)(p - start);
        if (line_len > 0 && start[line_len - 1] == '\r') {
            line_len--;
        }
        if (count == cap) {
            cap *= 2;
            lines = xrealloc(lines, cap * sizeof(char *));
        }
        lines[count++] = xstrndup(start, line_len);
        if (*p == '\n') {
            p++;
        }
    }
    *out_count = count;
    return lines;
}

void free_lines(char **lines, size_t count) {
    for (size_t i = 0; i < count; i++) {
        free(lines[i]);
    }
    free(lines);
}
