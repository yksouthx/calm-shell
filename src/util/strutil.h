/* Small, dependency-free string helpers shared across the codebase. */
#ifndef CALM_UTIL_STRUTIL_H
#define CALM_UTIL_STRUTIL_H

#include <stdbool.h>
#include <stddef.h>

bool starts_with(const char *s, const char *prefix);
bool ends_with(const char *s, const char *suffix);
bool is_blank(const char *s);

/* Trims leading+trailing ASCII whitespace in place. */
void trim_in_place(char *s);
/* Trims only trailing ASCII whitespace in place. */
void rtrim_in_place(char *s);

/* Splits `input` into lines (newline characters stripped, and a
 * trailing '\r' before them, so CRLF-authored files behave the same
 * as LF ones). The returned array and every element must be freed
 * with free_lines(). */
char **split_lines(const char *input, size_t *out_count);
void free_lines(char **lines, size_t count);

#endif /* CALM_UTIL_STRUTIL_H */
