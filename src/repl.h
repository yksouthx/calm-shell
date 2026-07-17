/* The interactive loop behind bare `calm`.
 *
 * Line editing itself (raw-mode input, history, completion) is
 * line_editor.c. This module owns everything *around* that:
 * environment variables, `[aliases]` + `[directory.aliases]`
 * expansion, `[functions]` and installed plugins (sourced as a
 * preamble before each command), the `cd`/`pushd`/`popd`/`dirs`
 * builtins plus `auto_cd`, directory-change syncing for everything
 * else, the terminal-title OSC sequence, the error bell, and Ctrl+C
 * handling around spawned commands. */
#ifndef CALM_REPL_H
#define CALM_REPL_H

#include "calm_format.h"
#include "theme.h"

/* Runs the interactive read-eval-print loop until `exit`/`quit`/Ctrl+D.
 * Returns 0 on a clean exit. */
int repl_run(const Theme *theme, const CalmDocument *cfg, const char *icon);

#endif /* CALM_REPL_H */
