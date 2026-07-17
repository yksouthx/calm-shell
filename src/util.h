/* Small shared utilities: allocation wrappers, string helpers, and a
 * growable string builder. Kept dependency-free (libc only) on purpose --
 * this project's whole point is minimal dependencies. */
#ifndef CALM_UTIL_H
#define CALM_UTIL_H

#include <stdbool.h>
#include <stddef.h>

/* --- allocation wrappers ---------------------------------------------
 * Calm-shell is a small interactive tool, not a long-running server: an
 * allocation failure means the machine is critically out of memory, and
 * there's no graceful degradation to offer. These abort with a clear
 * message instead of every call site needing its own NULL check. */
void *xmalloc(size_t size);
void *xrealloc(void *ptr, size_t size);
char *xstrdup(const char *s);
char *xstrndup(const char *s, size_t n);

/* printf-style formatting into a newly allocated string. */
char *xsprintf(const char *fmt, ...);

/* --- string helpers ---------------------------------------------------- */
bool starts_with(const char *s, const char *prefix);
bool ends_with(const char *s, const char *suffix);
bool is_blank(const char *s);

/* Trims leading+trailing ASCII whitespace in place. */
void trim_in_place(char *s);
/* Trims only trailing ASCII whitespace in place. */
void rtrim_in_place(char *s);

/* Splits `input` into lines (without the newline characters). The
 * returned array and every element must be freed with free_lines(). */
char **split_lines(const char *input, size_t *out_count);
void free_lines(char **lines, size_t count);

/* --- string builder ------------------------------------------------------ */
typedef struct {
    char *data;
    size_t len;
    size_t cap;
} StrBuilder;

void sb_init(StrBuilder *sb);
void sb_append(StrBuilder *sb, const char *s);
void sb_append_char(StrBuilder *sb, char c);
/* Returns the accumulated, NUL-terminated string, transferring
 * ownership to the caller (who must free() it), and resets `sb` to a
 * fresh, empty, still-usable builder. */
char *sb_take(StrBuilder *sb);
void sb_free(StrBuilder *sb);

/* --- filesystem helpers -------------------------------------------------- */
/* Reads an entire file into a newly allocated, NUL-terminated string.
 * Returns NULL on failure (errno is left set by the failing syscall). */
char *read_file_to_string(const char *path);
/* errno right after the last failing libc call in this translation
 * unit's helpers -- used so callers can build a message without racing
 * further libc calls of their own against errno. */
int errno_snapshot(void);

/* Returns the parent directory of `path` (or "." if there is none), as
 * a newly allocated string. */
char *dirname_of(const char *path);
/* Joins `dir` and `file` with exactly one '/' between them. */
char *join_path(const char *dir, const char *file);
/* Returns $HOME (or NULL if unset), as a newly allocated string. */
char *home_dir(void);
/* Returns $XDG_CONFIG_HOME, or ~/.config if unset, as a newly allocated
 * string. Returns NULL only if neither is resolvable. */
char *xdg_config_dir(void);

#endif /* CALM_UTIL_H */
