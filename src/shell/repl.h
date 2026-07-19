/* The interactive read-eval-print loop: ties config, theme, the line
 * editor, aliases, builtins, and command execution together. */
#ifndef CALM_SHELL_REPL_H
#define CALM_SHELL_REPL_H

/* Runs the shell until the user exits (Ctrl+D, `exit`, or `quit`).
 * Returns the process exit code. */
int repl_run(void);

#endif /* CALM_SHELL_REPL_H */
