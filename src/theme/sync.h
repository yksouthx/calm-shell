/* The single entry point every piece of Calm-shell that changes or
 * loads the active theme calls through: `theme theme set`, `calm
 * sync`, and the REPL's own startup all funnel here, so "propagate
 * the active theme everywhere" has exactly one implementation rather
 * than being reimplemented (and drifting) at each call site. */
#ifndef CALM_THEME_SYNC_H
#define CALM_THEME_SYNC_H

#include <stdbool.h>

#include "config/calmconf.h"
#include "theme/theme.h"

typedef struct {
    bool terminal_enabled; /* [sync] sync_terminal */
    bool terminal_synced;  /* emulator_sync_write's return value, if it ran */
    char *terminal_detail; /* human-readable summary; NULL if terminal_enabled is false */

    bool fastfetch_enabled; /* [sync] sync_fastfetch */
    bool fastfetch_synced;
    char *fastfetch_path; /* generated config path; NULL if fastfetch_enabled is false */
} ThemeSyncResult;

void theme_sync_result_free(ThemeSyncResult *r);

/* Runs the sync pipeline for `theme` against terminal.calm's
 * `[appearance]`/`[sync]` settings in `cfg`: detects (or uses
 * `[sync] terminal_override`) the host terminal emulator and writes
 * its theme file, and/or regenerates the Fastfetch config -- each
 * independently gated by its own `[sync]` flag (both on by default).
 * Always fills `*out`; never fails outright (individual steps report
 * their own success through it instead). */
void theme_sync_apply(const Theme *theme, const CalmDocument *cfg, ThemeSyncResult *out);

#endif /* CALM_THEME_SYNC_H */
