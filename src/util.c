#include "util.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_last_errno = 0;

void *xmalloc(size_t size) {
    if (size == 0) {
        size = 1;
    }
    void *p = malloc(size);
    if (!p) {
        fprintf(stderr, "calm: out of memory\n");
        exit(1);
    }
    return p;
}

void *xrealloc(void *ptr, size_t size) {
    if (size == 0) {
        size = 1;
    }
    void *p = realloc(ptr, size);
    if (!p) {
        fprintf(stderr, "calm: out of memory\n");
        exit(1);
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
        /* Strip a trailing \r so CRLF-authored files behave the same as
         * LF ones. */
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

void sb_init(StrBuilder *sb) {
    sb->data = xmalloc(1);
    sb->data[0] = '\0';
    sb->len = 0;
    sb->cap = 1;
}

static void sb_reserve(StrBuilder *sb, size_t extra) {
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

void sb_append(StrBuilder *sb, const char *s) {
    size_t n = strlen(s);
    sb_reserve(sb, n);
    memcpy(sb->data + sb->len, s, n + 1);
    sb->len += n;
}

void sb_append_char(StrBuilder *sb, char c) {
    sb_reserve(sb, 1);
    sb->data[sb->len++] = c;
    sb->data[sb->len] = '\0';
}

char *sb_take(StrBuilder *sb) {
    char *out = sb->data;
    sb_init(sb);
    return out;
}

void sb_free(StrBuilder *sb) {
    free(sb->data);
    sb->data = NULL;
    sb->len = 0;
    sb->cap = 0;
}

char *read_file_to_string(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        g_last_errno = errno;
        return NULL;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        g_last_errno = errno;
        fclose(f);
        return NULL;
    }
    long size = ftell(f);
    if (size < 0) {
        g_last_errno = errno;
        fclose(f);
        return NULL;
    }
    rewind(f);
    char *buf = xmalloc((size_t)size + 1);
    size_t read = fread(buf, 1, (size_t)size, f);
    buf[read] = '\0';
    fclose(f);
    return buf;
}

int errno_snapshot(void) {
    return g_last_errno;
}

char *dirname_of(const char *path) {
    const char *slash = strrchr(path, '/');
    if (!slash) {
        return xstrdup(".");
    }
    if (slash == path) {
        return xstrdup("/");
    }
    return xstrndup(path, (size_t)(slash - path));
}

char *join_path(const char *dir, const char *file) {
    size_t dlen = strlen(dir);
    bool needs_slash = dlen > 0 && dir[dlen - 1] != '/';
    return xsprintf("%s%s%s", dir, needs_slash ? "/" : "", file);
}

char *home_dir(void) {
    const char *home = getenv("HOME");
    if (home && home[0] != '\0') {
        return xstrdup(home);
    }
    return NULL;
}

char *xdg_config_dir(void) {
    const char *xdg = getenv("XDG_CONFIG_HOME");
    if (xdg && xdg[0] != '\0') {
        return xstrdup(xdg);
    }
    char *home = home_dir();
    if (!home) {
        return NULL;
    }
    char *out = join_path(home, ".config");
    free(home);
    return out;
}
