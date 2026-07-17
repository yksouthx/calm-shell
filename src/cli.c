#include "cli.h"

#include <stdio.h>
#include <string.h>

#include "version.h"

void cli_print_help(void) {
    printf(
        "calm -- Fish's simplicity + Zsh's power + HyDE's beauty + Arch's flexibility.\n"
        "\n"
        "Usage: calm [COMMAND]\n"
        "\n"
        "Commands:\n"
        "  theme list            List all installed themes\n"
        "  theme set <name>      Set the active theme\n"
        "  plugin install <name> Scaffold a new plugin\n"
        "  config                Open the Calm-shell configuration in $EDITOR\n"
        "  doctor                Diagnose system compatibility\n"
        "  help                  Print this help message\n"
        "  version               Print version information\n"
        "\n"
        "Run `calm` with no arguments to launch the interactive shell.\n");
}

void cli_print_version(void) {
    printf("calm-shell %s\n", CALM_SHELL_VERSION);
}

Cli cli_parse(int argc, char **argv) {
    Cli cli = {.command = CMD_SHELL, .arg = NULL};

    if (argc <= 1) {
        return cli;
    }

    const char *first = argv[1];

    if (strcmp(first, "-h") == 0 || strcmp(first, "--help") == 0 || strcmp(first, "help") == 0) {
        cli.command = CMD_HELP;
        return cli;
    }
    if (strcmp(first, "-V") == 0 || strcmp(first, "--version") == 0 || strcmp(first, "version") == 0) {
        cli.command = CMD_VERSION;
        return cli;
    }

    if (strcmp(first, "theme") == 0) {
        if (argc >= 3 && strcmp(argv[2], "list") == 0) {
            cli.command = CMD_THEME_LIST;
            return cli;
        }
        if (argc >= 4 && strcmp(argv[2], "set") == 0) {
            cli.command = CMD_THEME_SET;
            cli.arg = argv[3];
            return cli;
        }
        cli.command = CMD_UNKNOWN;
        return cli;
    }

    if (strcmp(first, "plugin") == 0) {
        if (argc >= 4 && strcmp(argv[2], "install") == 0) {
            cli.command = CMD_PLUGIN_INSTALL;
            cli.arg = argv[3];
            return cli;
        }
        cli.command = CMD_UNKNOWN;
        return cli;
    }

    if (strcmp(first, "config") == 0) {
        cli.command = CMD_CONFIG;
        return cli;
    }

    if (strcmp(first, "doctor") == 0) {
        cli.command = CMD_DOCTOR;
        return cli;
    }

    cli.command = CMD_UNKNOWN;
    return cli;
}
