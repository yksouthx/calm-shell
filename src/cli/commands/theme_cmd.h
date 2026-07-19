/* `calm theme list|set` -- lists installed themes (bundled + anything
 * dropped into ~/.config/calm-shell/themes/) and switches the active
 * one by writing its name to the active-theme marker file. */
#ifndef CALM_CLI_THEME_CMD_H
#define CALM_CLI_THEME_CMD_H

int theme_cmd_run(int argc, char **argv);

#endif /* CALM_CLI_THEME_CMD_H */
