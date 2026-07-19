/* Generates a Fastfetch config.jsonc derived from the active Calm-shell
 * theme -- logo colors, key/title colors, and a border/separator style
 * that all track `color_scheme_build()`'s palette -- and runs
 * Fastfetch with it at the start of a new session.
 *
 * The generated file lives at `~/.config/calm-shell/fastfetch.jsonc`,
 * entirely separate from the user's own
 * `~/.config/fastfetch/config.jsonc` (if they have one): Fastfetch is
 * always invoked with `--config <that path>` explicitly, so nothing
 * about the user's own Fastfetch setup is ever touched. */
#ifndef CALM_THEME_FASTFETCH_SYNC_H
#define CALM_THEME_FASTFETCH_SYNC_H

#include <stdbool.h>

#include "theme/theme.h"

/* (Re)writes the generated config from `theme`. Returns false only on
 * an actual write failure (unresolvable config dir, permissions).
 * *out_path, if non-NULL, receives the generated file's path (newly
 * allocated, caller must free()) regardless of success -- useful for
 * `fastfetch_run` even when the caller already knows the sync
 * succeeded from a moment ago. */
bool fastfetch_sync_write(const Theme *theme, char **out_path);

/* Runs `fastfetch --config <config_path>` in the foreground, sharing
 * this process's stdio. A no-op (returns true) if fastfetch isn't
 * installed -- a missing optional dependency is never an error here,
 * just nothing to show. */
bool fastfetch_run(const char *config_path);

#endif /* CALM_THEME_FASTFETCH_SYNC_H */
