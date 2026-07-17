/* Config directory layout and first-run scaffolding.
 *
 * Root config dir: $XDG_CONFIG_HOME/calm-shell (or ~/.config/calm-shell).
 * Every path getter here returns a newly allocated string the caller
 * must free(). They return NULL only if $HOME can't be resolved at all. */
#ifndef CALM_CONFIG_H
#define CALM_CONFIG_H

#include <stdbool.h>

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

/* Ensures the full ~/.config/calm-shell/ tree exists, creating the
 * default calm-lavender theme and the topic-file scaffold on first
 * run. Returns true on success. */
bool ensure_scaffold(void);

/* The bundled calm-lavender theme JSON, exposed so `calm doctor` and the
 * theme engine's own self-tests can validate the actual shipped content
 * instead of a second, hand-copied reference that could drift. */
const char *default_lavender_theme(void);

#endif /* CALM_CONFIG_H */
