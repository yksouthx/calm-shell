/* The `calm <subcommand>` dispatcher -- everything calm-shell can do
 * outside of actually being an interactive shell (theme management,
 * plugin installation, config inspection, environment diagnostics). */
#ifndef CALM_CLI_CLI_H
#define CALM_CLI_CLI_H

/* Dispatches argv[1] (the subcommand) to its handler. Returns the
 * process exit code. `argc`/`argv` are the full, unmodified process
 * arguments (argv[0] is the program name, argv[1] the subcommand). */
int cli_dispatch(int argc, char **argv);

#endif /* CALM_CLI_CLI_H */
