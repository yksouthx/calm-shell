/* `calm plugin install|list` -- installs a plugin (a local directory
 * or a git repository containing a `plugin.calm` manifest) into
 * ~/.config/calm-shell/plugins/, and lists what's installed. */
#ifndef CALM_CLI_PLUGIN_CMD_H
#define CALM_CLI_PLUGIN_CMD_H

int plugin_cmd_run(int argc, char **argv);

#endif /* CALM_CLI_PLUGIN_CMD_H */
