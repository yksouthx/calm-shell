/* Config directory layout.
 *
 * Root config dir: $XDG_CONFIG_HOME/calm-shell (or ~/.config/calm-shell).
 * Every path getter here returns a newly allocated string the caller
 * must free(). They return NULL only if $HOME can't be resolved at all.
 */
#ifndef CALM_CONFIG_PATHS_H
#define CALM_CONFIG_PATHS_H

char *config_dir(void);
char *themes_dir(void);
char *plugins_dir(void);
char *history_dir(void);

char *config_file(void);
char *aliases_file(void);
char *functions_file(void);
char *environment_file(void);
char *directory_file(void);
char *history_settings_file(void);
char *keyboard_file(void);
char *terminal_file(void);

/* The file recording which theme name is currently active (a single
 * line, no config-format overhead needed for one value). */
char *active_theme_marker_file(void);

#endif /* CALM_CONFIG_PATHS_H */
