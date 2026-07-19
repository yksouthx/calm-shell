/* Cross-terminal appearance settings: the non-color knobs
 * (font, cursor, transparency, padding, decorations) that
 * `[appearance]` in terminal.calm exposes once, for every synced
 * terminal emulator to pick up -- so switching aesthetics doesn't mean
 * editing five different config languages by hand. Colors themselves
 * come from the active Theme, not from here. */
#ifndef CALM_THEME_APPEARANCE_H
#define CALM_THEME_APPEARANCE_H

#include <stdbool.h>

#include "config/calmconf.h"

typedef enum {
    CURSOR_STYLE_BEAM,
    CURSOR_STYLE_BLOCK,
    CURSOR_STYLE_UNDERLINE,
} CursorStyle;

typedef struct {
    char *font_family;
    double font_size;
    CursorStyle cursor_style;
    bool cursor_blink;
    /* 1.0 = fully opaque, 0.0 = fully transparent. Values below 1.0
     * are what "transparency" in terminal.calm actually toggles --
     * there's no separate on/off flag, just an opacity a synced
     * emulator either honors or (Konsole, GNOME Terminal) ignores. */
    double opacity;
    /* Uniform cell padding in pixels, applied to all four sides --
     * every synced emulator that supports padding takes one number,
     * not independent per-side settings. */
    long long padding;
    bool window_decorations;
} TermAppearance;

/* Reads terminal.calm's [appearance] section, falling back to
 * reasonable defaults for anything missing or malformed. Never fails
 * -- an absent section just means "use the defaults". */
void appearance_load(const CalmDocument *cfg, TermAppearance *out);
void appearance_free(TermAppearance *a);

/* Canonical lowercase name for a CursorStyle ("beam"/"block"/"underline"). */
const char *appearance_cursor_style_name(CursorStyle s);

#endif /* CALM_THEME_APPEARANCE_H */
