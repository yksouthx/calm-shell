/* Lightweight single-pass syntax highlighting for the line editor.
 *
 * Scope, honestly: this is a tokenizer (command word / quoted strings
 * / flags), not a full shell grammar -- no pipe/operator/subshell
 * awareness. Good enough to color the line a shell prompt actually
 * types without carrying a real parser for it. */
#ifndef CALM_TERM_HIGHLIGHT_H
#define CALM_TERM_HIGHLIGHT_H

#include "theme/theme.h"
#include "util/strlist.h"

/* Renders `line` with highlighting applied: the first word (green if
 * it resolves to a known command, red if not), quoted strings
 * (yellow), and `-flag`-shaped words (blue) -- everything else passes
 * through unstyled. Returns a newly allocated, ANSI-wrapped string. */
char *highlight_line(const Theme *theme, const StrList *known, const char *line);

#endif /* CALM_TERM_HIGHLIGHT_H */
