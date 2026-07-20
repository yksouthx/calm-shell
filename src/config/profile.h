/* Profiles -- named bundles of configuration a user can switch between
 * in one command (`calm profile gaming`). A profile is a `.calm` file
 * under `~/.config/calm-shell/profiles/`, structured like a small
 * `config.calm`: an optional `[profile]` section (`theme`, plus a
 * freeform `description`) and any of the same topic sections
 * (`[shell]`, `[prompt]`, `[environment]`, `[aliases]`, `[directory]`,
 * `[directory.aliases]`, `[history]`, `[keyboard]`, `[keyboard.bindings]`,
 * `[terminal]`, `[appearance]`, `[sync]`) a topic file would use, plus
 * `[plugins] enabled = [...]` (handled specially -- see profile_apply()
 * below) and `[startup] commands = [...]`.
 *
 * Activation doesn't rewrite the user's topic files -- it regenerates
 * `profile_active.calm`, the one file `config.calm` always includes
 * last (see config/scaffold.c), from the profile's own sections. That
 * keeps profile switching non-destructive (the topic files a profile
 * doesn't mention are untouched, and simply show back through once a
 * different profile is applied) and reuses the exact "later include
 * wins" merge behavior calmconf already has, rather than adding a
 * second overlay mechanism.
 *
 * `[profile] theme` (if set) is applied the same way `calm theme set`
 * applies one: written to the active-theme marker and pushed through
 * theme_sync_apply(), immediately re-syncing the terminal emulator and
 * Fastfetch. `[plugins] enabled = [...]` (if present) replaces the
 * plugin loader's enabled set outright and resyncs it (see
 * config/plugin_loader.h) -- unlike the other sections, this isn't
 * inert pass-through text, since "which plugins are enabled" has to
 * actually reach the plugin loader to mean anything. `[startup]
 * commands` (a list of shell one-liners) run once, right after
 * activation, each via `/bin/sh -c`. None of theme/plugins/startup are
 * written into profile_active.calm itself -- there's nothing in the
 * `.calm` format that would replay a command on every future shell
 * launch, and plugin state already has its own persisted file
 * (plugins.calm), so duplicating it here would just be a second
 * source of truth to drift out of sync with the first.
 */
#ifndef CALM_CONFIG_PROFILE_H
#define CALM_CONFIG_PROFILE_H

#include <stdbool.h>

#include "config/calmconf.h"

typedef struct {
    char *name; /* the profile's file name, without ".calm" */
    CalmDocument doc;
} Profile;

/* Loads `~/.config/calm-shell/profiles/<name>.calm`. Returns false
 * (doc left unpopulated) if the file doesn't exist or fails to parse;
 * *err_msg (if non-NULL) then holds a newly allocated message the
 * caller must free(). */
bool profile_load(const char *name, Profile *out, char **err_msg);
void profile_free(Profile *p);

/* Applies `p`: regenerates profile_active.calm from its pass-through
 * sections, applies+syncs its theme (if any), records it as the
 * active profile, and runs its startup commands in order. Returns
 * true if every step that was attempted succeeded; a startup command
 * exiting non-zero does not by itself cause a false return (matching
 * shell semantics: a startup line failing shouldn't hide that the
 * profile itself activated), but is reported through *warnings if
 * non-NULL, one line per failed command, for the caller to print.
 * `warnings` is a newly allocated string the caller must free(), or
 * NULL if there was nothing to report. */
bool profile_apply(const Profile *p, char **warnings);

/* Returns the currently active profile's name (newly allocated), or
 * NULL if none is active. */
char *profile_active_name(void);

#endif /* CALM_CONFIG_PROFILE_H */
