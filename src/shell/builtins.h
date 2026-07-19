/* Builtins that must run in the shell's own process rather than a
 * forked child: cd/pushd/popd/dirs (they change *this* process's
 * working directory) and exit/quit (they end the REPL loop). */
#ifndef CALM_SHELL_BUILTINS_H
#define CALM_SHELL_BUILTINS_H

#include <stdbool.h>

#include "shell/dir_stack.h"

typedef enum {
    BUILTIN_NOT_HANDLED,
    BUILTIN_HANDLED,
    BUILTIN_REQUEST_EXIT,
} BuiltinOutcome;

typedef struct {
    DirStack dir_stack;
    char *previous_dir; /* OLDPWD equivalent for `cd -`; NULL until the first cd */
    /* Set whenever the most recent builtins_dispatch() call printed an
     * error of its own (bad cd/pushd/popd target, ...) -- distinct
     * from a forked command's nonzero exit status, which is ordinary
     * and shouldn't ring the terminal bell the way a calm-shell-level
     * mistake should. */
    bool last_error;
} ShellNavState;

void shell_nav_init(ShellNavState *nav, size_t dir_stack_size);
void shell_nav_free(ShellNavState *nav);

/* Brings this process's own cwd in sync with `new_cwd` (chdir() plus
 * $OLDPWD/$PWD/`previous_dir` bookkeeping) -- used both by the cd
 * builtin itself and by shell/execute.h after a forked command that
 * changed directory (e.g. `cd foo && ls`, which runs in a subshell
 * whose own cwd change would otherwise be invisible back here).
 * Prints a `cd: ...` error and returns false on failure. */
bool shell_nav_sync_cwd(ShellNavState *nav, const char *new_cwd);

/* Dispatches `line` if its first word is a builtin (cd, pushd, popd,
 * dirs, exit, quit); prints anything the builtin itself needs to
 * (a `dirs` listing, a `cd: no such directory` error) and updates
 * process state (chdir(), $PWD/$OLDPWD, the directory stack).
 * On BUILTIN_REQUEST_EXIT, *exit_status is set to the shell's own
 * requested exit code (0 for bare exit/quit, or `exit N`'s N). */
BuiltinOutcome builtins_dispatch(ShellNavState *nav, const char *line, int *exit_status);

/* True if `line` is exactly one word naming an existing directory --
 * the auto_cd feature's trigger for treating a bare directory name as
 * an implicit `cd`. */
bool builtins_is_auto_cd_target(const char *line);

#endif /* CALM_SHELL_BUILTINS_H */
