/* Generic process-spawning primitives (fork/exec/pipe), independent of
 * anything shell-specific. Higher-level "run a shell command line and
 * track its cwd" logic lives in shell/execute.h, built on top of this. */
#ifndef CALM_EXEC_PROCESS_H
#define CALM_EXEC_PROCESS_H

#include <stdbool.h>

/* Runs argv[0] with the given NULL-terminated argument vector in the
 * foreground, sharing this process's stdio, and waits for it to
 * finish. Resets SIGINT/SIGQUIT to their default disposition in the
 * child, so an interactive program (an editor, a spawned shell
 * command) responds to Ctrl+C normally even while the parent shell
 * itself ignores it during the wait.
 *
 * Returns true if the process could be forked at all; *exit_status
 * receives its exit code (127 if the executable couldn't be found or
 * exec'd). */
bool process_run_foreground(char *const argv[], int *exit_status);

/* Runs argv[0] with stdout captured via a pipe and stderr discarded
 * (redirected to /dev/null), waits for it, and returns the captured
 * stdout as a newly allocated, NUL-terminated string -- or NULL if the
 * process couldn't be spawned, or it exited with a non-zero status. */
char *process_capture_stdout(char *const argv[]);

/* True if `name` resolves to an executable file somewhere on $PATH (or
 * is itself a path, absolute or relative, to one). A direct PATH scan
 * -- no `which`/`command -v` subprocess spawn needed for what is, in
 * the end, one access() call per PATH entry. */
bool process_command_exists(const char *name);

#endif /* CALM_EXEC_PROCESS_H */
