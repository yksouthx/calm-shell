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
char *profiles_dir(void);

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

/* The generated overlay `config.calm` includes last (see
 * config/scaffold.c): `calm profile <name>` regenerates this file from
 * the profile's pass-through sections, so a profile's settings win
 * over the topic files without calm-shell needing to edit the user's
 * own config.calm at activation time. */
char *profile_active_file(void);
/* The file recording which profile name is currently active (a single
 * line; empty/missing means no profile is active). */
char *active_profile_marker_file(void);

/* Directory `calm repair` copies a file's original content into,
 * timestamped per repair run, before overwriting or replacing it. */
char *backups_dir(void);

/* The file recording which plugins are enabled (a top-level
 * `enabled = [...]` list of plugin names) -- edited by `calm plugin
 * enable`/`disable`, or directly by hand. */
char *plugins_state_file(void);
/* The generated overlay `config.calm` includes (right before
 * profile_active.calm, so a profile's own settings still win over a
 * plugin's -- see config/scaffold.c): `calm plugin enable`/`disable`
 * and every shell startup regenerate this from every *enabled*
 * plugin's manifest sections. */
char *plugins_active_file(void);

#endif /* CALM_CONFIG_PATHS_H */
