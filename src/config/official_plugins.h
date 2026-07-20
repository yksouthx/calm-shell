/* The 10 official plugins bundled with calm-shell itself, so `calm
 * plugin install git` (a bare name, no URL or local path) works out
 * of the box with no network access. Each entry's manifest_fn()
 * returns the exact `plugin.calm` content that gets written to
 * ~/.config/calm-shell/plugins/<name>/plugin.calm on install -- the
 * same bundled-content-as-a-C-string approach config/scaffold.h's
 * default_lavender_theme() already uses for the one bundled theme,
 * for the same reason: this project has no runtime data directory or
 * install-time asset step (see docs/ARCHITECTURE.md's single-binary,
 * single-language stance), so "shipped with the binary" means "a
 * string constant compiled into it". */
#ifndef CALM_CONFIG_OFFICIAL_PLUGINS_H
#define CALM_CONFIG_OFFICIAL_PLUGINS_H

#include <stddef.h>

typedef struct {
    const char *name;
    const char *description; /* one line, for `calm plugin search` */
    const char *(*manifest_fn)(void);
} OfficialPlugin;

/* Returns the fixed table of official plugins and its length. */
const OfficialPlugin *official_plugins(size_t *out_count);

/* Looks up one entry by name, or NULL if `name` isn't an official
 * plugin (e.g. it's a URL, a local path, or a third-party plugin
 * already installed under a name that happens not to collide). */
const OfficialPlugin *official_plugin_find(const char *name);

#endif /* CALM_CONFIG_OFFICIAL_PLUGINS_H */
