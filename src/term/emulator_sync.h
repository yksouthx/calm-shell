/* Writes each supported terminal emulator's own native theme
 * representation from a Calm-shell Theme + TermAppearance, and wires
 * it into that emulator's real config -- via an include/import
 * directive for the emulators that support one (Kitty, Alacritty,
 * WezTerm, Foot, Ghostty), by patching the relevant profile file for
 * Konsole, or via `gsettings` for GNOME Terminal, which has no config
 * file at all.
 *
 * Design principle throughout: the *generated* file (always named
 * `calm-shell-theme.*`) is fully owned by calm-shell and overwritten
 * on every sync. The user's *own* config file is never overwritten --
 * at most, one clearly-commented line is appended to it once, to pull
 * the generated file in. */
#ifndef CALM_TERM_EMULATOR_SYNC_H
#define CALM_TERM_EMULATOR_SYNC_H

#include <stdbool.h>

#include "term/emulator.h"
#include "theme/appearance.h"
#include "theme/theme.h"

/* Syncs `kind`'s theme from `theme`/`appearance`. Returns false only
 * on an actual failure to write (permissions, unresolvable config
 * dir, or a missing required external tool like `gsettings`);
 * TERM_EMU_UNKNOWN is a no-op that still returns true, since "nothing
 * to sync against" isn't an error.
 *
 * *out_detail receives a newly allocated, human-readable one-line
 * summary (what was written, or why nothing was) for `calm sync`/
 * `calm doctor` output. Caller must free() it; may be NULL if the
 * caller doesn't need it. */
bool emulator_sync_write(TermEmulatorKind kind, const Theme *theme, const TermAppearance *appearance,
                          char **out_detail);

#endif /* CALM_TERM_EMULATOR_SYNC_H */
