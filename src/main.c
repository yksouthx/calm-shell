#include <stdio.h>

#include "cli.h"
#include "commands/config_cmd.h"
#include "commands/doctor.h"
#include "commands/plugin.h"
#include "commands/shell.h"
#include "commands/theme_cmd.h"

int main(int argc, char **argv) {
    Cli cli = cli_parse(argc, argv);

    switch (cli.command) {
        case CMD_SHELL:
            return shell_cmd_launch();
        case CMD_THEME_LIST:
            return theme_cmd_list();
        case CMD_THEME_SET:
            return theme_cmd_set(cli.arg);
        case CMD_PLUGIN_INSTALL:
            return plugin_cmd_install(cli.arg);
        case CMD_CONFIG:
            return config_cmd_open();
        case CMD_DOCTOR:
            return doctor_cmd_run();
        case CMD_HELP:
            cli_print_help();
            return 0;
        case CMD_VERSION:
            cli_print_version();
            return 0;
        case CMD_UNKNOWN:
        default:
            fprintf(stderr, "\x1b[38;2;243;139;168m\x1b[1merror:\x1b[0m unrecognized command\n\n");
            cli_print_help();
            return 1;
    }
}
