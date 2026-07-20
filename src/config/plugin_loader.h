/* Drives which installed plugins are active. Two files, two jobs:
 *
 *   - plugins.calm (config/paths.h:plugins_state_file()) is the
 *     source of truth for *which* plugins are enabled: a top-level
 *     `enabled = [...]` list of plugin names, edited by `calm plugin
 *     enable`/`disable` (or by hand).
 *   - plugins_active.calm (plugins_active_file()) is the *generated*
 *     overlay config.calm actually includes: every enabled plugin's
 *     [environment]/[aliases]/[functions] sections, concatenated.
 *     Regenerating it from plugins.calm is what plugin_loader_sync()
 *     does, and is what makes an enabled plugin's aliases/env
 *     actually take effect -- the same "generated overlay a real
 *     config include pulls in" shape config/profile.h's
 *     profile_active.calm uses, and for the same reason: it reuses
 *     calmconf's own merge behavior instead of a second mechanism.
 *
 * On startup cost: a plugin's *manifest* (a small `.calm` file) is
 * read once, at sync time, to extract its alias/env/function
 * definitions -- there's no plugin "code" to execute and nothing a
 * plugin does keeps running in the background, so there's no
 * meaningful sense in which an enabled-but-unused plugin costs
 * anything beyond that one small file read. This is what "plugins
 * are loaded only when necessary to minimize startup time" comes down
 * to for a plugin shaped like a bundle of aliases rather than a
 * program: nothing runs, and nothing shows up as available (as a
 * completion, as an alias) for a plugin that isn't enabled. */
#ifndef CALM_CONFIG_PLUGIN_LOADER_H
#define CALM_CONFIG_PLUGIN_LOADER_H

#include <stdbool.h>

#include "util/strlist.h"

/* The plugin names in plugins.calm's `enabled` list, in file order.
 * Caller must strlist_free(). Empty (not an error) if the file is
 * missing, unparsable, or the key isn't set. */
StrList plugin_loader_enabled_names(void);

bool plugin_loader_is_enabled(const char *name);

/* Overwrites plugins.calm's `enabled` list with exactly `names`
 * (order preserved, exact duplicates within `names` collapsed). Does
 * NOT itself regenerate plugins_active.calm -- call
 * plugin_loader_sync() after, the same two-step shape
 * config/profile.c's profile_apply() already uses for its own
 * generated files (so a caller that's about to make several state
 * changes can batch them into one resync). */
bool plugin_loader_set_enabled(const StrList *names);

/* Enables/disables one plugin (adds/removes it from the enabled list,
 * a no-op if it's already in the requested state) and immediately
 * resyncs plugins_active.calm. plugin_loader_enable() additionally
 * requires the plugin to actually be installed (a directory with a
 * plugin.calm under plugins_dir()) -- enabling a name that isn't
 * installed is almost always a typo, not something to accept
 * silently. Returns false if that check fails or if the
 * write/resync failed. */
bool plugin_loader_enable(const char *name);
bool plugin_loader_disable(const char *name);

/* Regenerates plugins_active.calm from every currently-enabled
 * plugin's manifest, in enabled-list order (so where two enabled
 * plugins define the same alias, the one enabled more recently --
 * i.e. later in the list -- wins, matching calmconf's own
 * later-definition-wins rule). A plugin that's enabled but whose
 * directory or manifest is now missing or broken is skipped, not
 * fatal -- the rest of the enabled set still loads -- and reported
 * through *warnings (newly allocated, one line per skipped plugin,
 * caller must free(); NULL if nothing needed reporting). Called
 * after every enable/disable, from `calm repair`, and once at the
 * start of every shell session before config.calm is parsed (see
 * shell/repl.c), so a hand-edited plugins.calm or a plugin directory
 * removed with `rm -rf` is always picked up rather than silently
 * left stale. Returns false only if plugins_active.calm itself
 * couldn't be written. */
bool plugin_loader_sync(char **warnings);

#endif /* CALM_CONFIG_PLUGIN_LOADER_H */
