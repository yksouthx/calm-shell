#include "cli/cli.h"

#include <stdio.h>
#include <string.h>

#include "cli/commands/config_cmd.h"
#include "cli/commands/doctor_cmd.h"
#include "cli/commands/plugin_cmd.h"
#include "cli/commands/profile_cmd.h"
#include "cli/commands/repair_cmd.h"
#include "cli/commands/shell_cmd.h"
#include "cli/commands/sync_cmd.h"
#include "cli/commands/theme_cmd.h"
#include "version.h"

static void print_help(void) {
    printf(
        "calm-shell " CALM_VERSION "\n"
        "\n"
        "usage: calm [subcommand] [args...]\n"
        "\n"
        "With no subcommand, starts the interactive shell.\n"
        "\n"
        "subcommands:\n"
        "  shell                 start the interactive shell (the default)\n"
        "  theme list            list installed themes\n"
        "  theme set <name>      switch the active theme (also re-syncs terminal + fastfetch)\n"
        "  sync                  re-sync the active theme to your terminal emulator + fastfetch\n"
        "  profile list          list available profiles\n"
        "  profile current       print the active profile, if any\n"
        "  profile <name>        activate a profile (theme, env, aliases, plugins, startup commands)\n"
        "  plugin install <src>  install a plugin (official name, local directory, or git URL)\n"
        "  plugin search [query] search official bundled plugins\n"
        "  plugin list           list installed plugins (enabled/disabled)\n"
        "  plugin enable <name>  activate an installed plugin's aliases/environment/functions\n"
        "  plugin disable <name> deactivate a plugin without uninstalling it\n"
        "  plugin remove <name>  uninstall a plugin\n"
        "  plugin update [name]  refresh an installed plugin (or --all) from its original source\n"
        "  config edit           open config.calm in $EDITOR\n"
        "  config path           print the config directory path\n"
        "  config check          validate config.calm\n"
        "  doctor                check the environment for common issues\n"
        "  repair                validate + restore config, themes, plugins, and re-sync terminal/fastfetch\n"
        "  version               print the version\n"
        "  help                  print this message\n");
}

int cli_dispatch(int argc, char **argv) {
    if (argc < 2) {
        return shell_cmd_run();
    }
    const char *sub = argv[1];

    if (strcmp(sub, "shell") == 0) {
        return shell_cmd_run();
    }
    if (strcmp(sub, "theme") == 0) {
        return theme_cmd_run(argc, argv);
    }
    if (strcmp(sub, "sync") == 0) {
        return sync_cmd_run();
    }
    if (strcmp(sub, "plugin") == 0) {
        return plugin_cmd_run(argc, argv);
    }
    if (strcmp(sub, "profile") == 0) {
        return profile_cmd_run(argc, argv);
    }
    if (strcmp(sub, "config") == 0) {
        return config_cmd_run(argc, argv);
    }
    if (strcmp(sub, "doctor") == 0) {
        return doctor_cmd_run();
    }
    if (strcmp(sub, "repair") == 0) {
        return repair_cmd_run();
    }
    if (strcmp(sub, "version") == 0 || strcmp(sub, "--version") == 0 || strcmp(sub, "-v") == 0) {
        printf("calm-shell %s\n", CALM_VERSION);
        return 0;
    }
    if (strcmp(sub, "help") == 0 || strcmp(sub, "--help") == 0 || strcmp(sub, "-h") == 0) {
        print_help();
        return 0;
    }

    fprintf(stderr, "calm: unknown subcommand `%s` (see `calm help`)\n", sub);
    return 1;
}
