/* Calm-shell -- Fish's simplicity + Zsh's power + HyDE's beauty +
 * Arch's flexibility.
 *
 * Minimal hand-rolled argv parsing (no getopt_long dependency needed
 * for a command surface this small): `calm` alone launches the
 * interactive shell; everything else is one of a handful of
 * subcommands. */
#ifndef CALM_CLI_H
#define CALM_CLI_H

typedef enum {
    CMD_SHELL, /* bare `calm` */
    CMD_THEME_LIST,
    CMD_THEME_SET,
    CMD_PLUGIN_INSTALL,
    CMD_CONFIG,
    CMD_DOCTOR,
    CMD_HELP,
    CMD_VERSION,
    CMD_UNKNOWN,
} CliCommand;

typedef struct {
    CliCommand command;
    /* Owned by argv; not separately allocated. NULL unless the
     * command needs it (theme set / plugin install). */
    const char *arg;
} Cli;

Cli cli_parse(int argc, char **argv);
void cli_print_help(void);
void cli_print_version(void);

#endif /* CALM_CLI_H */
