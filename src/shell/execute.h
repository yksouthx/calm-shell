/* Runs a command line as a real POSIX shell script (`/bin/sh -c`) --
 * required for anything beyond a single simple command: pipes,
 * redirection, `&&`/`||`/`;` chaining, globbing, command substitution.
 *
 * Every invocation is prefixed with a "prelude" of `sh`-syntax function
 * definitions assembled from `[functions]` plus each installed
 * plugin's entry script, so `mkcd foo` and any plugin-defined command
 * work the same as a real builtin from the user's point of view. */
#ifndef CALM_SHELL_EXECUTE_H
#define CALM_SHELL_EXECUTE_H

#include <stdbool.h>

#include "config/calmconf.h"
#include "shell/builtins.h"

/* Assembles the `sh`-syntax prelude: one `name() { body; }` function
 * per `[functions]` entry, followed by `. "path"` for each installed
 * plugin's entry script. Returns a newly allocated string. */
char *execute_build_prelude(const CalmDocument *cfg);

/* True if `line` contains shell syntax (a pipe, redirection, `&&`,
 * `||`, `;`, backgrounding, command substitution, or a quote) that a
 * bare builtin dispatch can't correctly interpret as "my one
 * argument" -- e.g. `cd foo && ls` must not hand pushd/popd/cd the
 * literal string "foo && ls" as a target. Lines like this always run
 * through execute_run(), never through builtins_dispatch(). */
bool execute_needs_full_shell(const char *line);

/* Runs `line` as `/bin/sh -c` with `prelude` prepended, sharing this
 * process's stdio, and waits for it to finish. If the subshell's own
 * cwd ended up different from this process's (e.g. `line` itself
 * contained a `cd`), syncs `nav` (and this process) to match, so the
 * next prompt reflects it -- the one piece of state a forked
 * subprocess can't hand back through its exit status alone. */
int execute_run(ShellNavState *nav, const char *prelude, const char *line);

#endif /* CALM_SHELL_EXECUTE_H */
