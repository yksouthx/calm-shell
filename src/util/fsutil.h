/* Small filesystem/path helpers shared across the codebase. */
#ifndef CALM_UTIL_FSUTIL_H
#define CALM_UTIL_FSUTIL_H

#include <stdbool.h>

/* Reads an entire file into a newly allocated, NUL-terminated string.
 * Returns NULL on failure; errno_snapshot() then reports why. */
char *read_file_to_string(const char *path);

/* errno right after the last failing call inside this translation
 * unit's helpers -- lets callers build an error message without racing
 * further libc calls of their own against errno. */
int errno_snapshot(void);

/* Returns the parent directory of `path` (or "." if there is none), as
 * a newly allocated string. */
char *dirname_of(const char *path);
/* Joins `dir` and `file` with exactly one '/' between them. */
char *join_path(const char *dir, const char *file);
/* Returns $HOME (or NULL if unset/empty), as a newly allocated string. */
char *home_dir(void);
/* Returns $XDG_CONFIG_HOME, or ~/.config if unset, as a newly
 * allocated string. Returns NULL only if neither is resolvable. */
char *xdg_config_dir(void);
/* Returns $XDG_DATA_HOME, or ~/.local/share if unset, as a newly
 * allocated string. Returns NULL only if neither is resolvable. */
char *xdg_data_dir(void);

/* True if `path` exists and is a directory / a regular file, without
 * the caller needing its own <sys/stat.h> boilerplate at every call
 * site. */
bool path_is_dir(const char *path);
bool path_is_file(const char *path);
bool path_exists(const char *path);

/* Creates `path` and every missing parent directory (mkdir -p),
 * mode 0755. Returns true if the directory exists on return. */
bool mkdir_recursive(const char *path);

/* Writes `contents` to `path`, creating or truncating it -- unlike
 * scaffold.c's first-run-only write, this always overwrites, for
 * generated files a resync is meant to refresh every time. */
bool overwrite_file(const char *path, const char *contents);
/* Appends `contents` to `path` (creating it if missing) without
 * touching any existing content. */
bool append_to_file(const char *path, const char *contents);
/* True if `path` exists, is readable, and its contents contain
 * `needle` verbatim -- used to make one-time "add an include line"
 * edits to a user's own config idempotent. False (not an error) if
 * the file doesn't exist yet. */
bool file_contains(const char *path, const char *needle);

/* Returns the current working directory as a newly allocated string,
 * or NULL on failure (path too long, deleted cwd, ...). */
char *current_working_dir(void);

#endif /* CALM_UTIL_FSUTIL_H */
