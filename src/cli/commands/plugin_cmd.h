/* `calm plugin install|search|list|enable|disable|remove|update` --
 * the plugin manager: installs a plugin (an official bundled one by
 * name, a local directory, or a git repository containing a
 * `plugin.calm` manifest) into ~/.config/calm-shell/plugins/, lists
 * what's installed, and enables/disables/removes/updates them. See
 * config/plugin_loader.h for what "enabled" actually does. */
#ifndef CALM_CLI_PLUGIN_CMD_H
#define CALM_CLI_PLUGIN_CMD_H

int plugin_cmd_run(int argc, char **argv);

#endif /* CALM_CLI_PLUGIN_CMD_H */
