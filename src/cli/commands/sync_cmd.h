/* `calm sync` -- manually re-runs the theme-sync pipeline (terminal
 * emulator theme file + Fastfetch config) for the active theme,
 * without switching themes. Mainly useful right after installing or
 * reconfiguring a terminal emulator, so it picks up the theme
 * immediately instead of waiting for the next `theme set`. */
#ifndef CALM_CLI_SYNC_CMD_H
#define CALM_CLI_SYNC_CMD_H

int sync_cmd_run(void);

#endif /* CALM_CLI_SYNC_CMD_H */
