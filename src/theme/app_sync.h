/* Extends theme sync beyond the hosting terminal emulator + Fastfetch
 * (term/emulator_sync.h, theme/fastfetch_sync.h) to the rest of a
 * themed desktop: a system monitor, an audio visualizer, a file
 * manager, two editors, a compositor, a status bar, and two app
 * launchers. Every writer here follows the same two rules
 * emulator_sync.c established:
 *
 *   1. The *generated* file is always named `calm-shell-theme.*` (or,
 *      where an app's theme mechanism IS a whole separate named file
 *      -- Yazi's flavor, btop's/Helix's theme file -- a `calm-shell`
 *      themed asset under that app's own themes directory) and is
 *      fully owned by calm-shell, overwritten on every sync.
 *   2. The user's own main config is never overwritten wholesale.
 *      Where the app supports an include/import directive, one
 *      clearly-commented line is added once. Where it doesn't (Cava,
 *      Fuzzel: plain flat INI with no include), only the specific
 *      color keys calm-shell owns are patched in place -- every other
 *      line in the user's file is left exactly as it was.
 *
 * Every writer is a no-op-but-successful if it can't find anywhere to
 * write (an unresolvable config dir): "app probably isn't installed"
 * isn't a sync failure, so results are reported informationally, not
 * as errors, the same way `emulator_sync_write` treats
 * TERM_EMU_UNKNOWN. */
#ifndef CALM_THEME_APP_SYNC_H
#define CALM_THEME_APP_SYNC_H

#include <stdbool.h>
#include <stddef.h>

#include "theme/theme.h"

typedef struct {
    char *app;    /* e.g. "waybar" -- matches the name used in [sync] app toggles */
    bool synced;
    char *detail; /* human-readable one-line summary, always set */
} AppSyncEntry;

typedef struct {
    AppSyncEntry *items;
    size_t count;
} AppSyncResult;

void app_sync_result_free(AppSyncResult *r);

/* Runs every app writer below against `theme` and appends one
 * AppSyncEntry per app to `out` (which must already be
 * zero-initialized, e.g. by declaring it with `= {0}` or memset).
 * Never fails outright -- see the no-op-but-successful note above. */
void app_sync_write_all(const Theme *theme, AppSyncResult *out);

#endif /* CALM_THEME_APP_SYNC_H */
