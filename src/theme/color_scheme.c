#include "theme/color_scheme.h"

#include <stddef.h>
#include <stdlib.h>

#include "util/xalloc.h"

static char *hex_of_rgb(unsigned char r, unsigned char g, unsigned char b) {
    return xsprintf("#%02X%02X%02X", r, g, b);
}

/* Nudges a color `amount` (0..1) of the way toward white, for a
 * "bright" ANSI variant when the theme itself only defines one shade
 * per hue. amount=0 returns the color unchanged. */
static void lighten(unsigned char *r, unsigned char *g, unsigned char *b, double amount) {
    *r = (unsigned char)(*r + (255 - *r) * amount);
    *g = (unsigned char)(*g + (255 - *g) * amount);
    *b = (unsigned char)(*b + (255 - *b) * amount);
}

static char *hex_of_semantic(const Theme *t, const char *key, unsigned char fallback_r, unsigned char fallback_g,
                              unsigned char fallback_b) {
    unsigned char r, g, b;
    if (!theme_rgb(t, key, &r, &g, &b)) {
        r = fallback_r;
        g = fallback_g;
        b = fallback_b;
    }
    return hex_of_rgb(r, g, b);
}

/* Picks the `i`th palette entry (cycling if the theme defines fewer
 * than needed, and falling back to mid-gray if it defines none at
 * all -- a theme file that somehow has an empty [colors] section
 * still produces a complete, if drab, 16-color scheme rather than a
 * crash). */
static void nth_palette_rgb(const Theme *t, size_t i, unsigned char *r, unsigned char *g, unsigned char *b) {
    if (t->palette_count == 0) {
        *r = *g = *b = 0x88;
        return;
    }
    const PaletteEntry *e = &t->palette[i % t->palette_count];
    *r = e->r;
    *g = e->g;
    *b = e->b;
}

void color_scheme_build(const Theme *theme, TermColorScheme *out) {
    out->background = hex_of_semantic(theme, "background", 0x11, 0x11, 0x11);
    out->foreground = hex_of_semantic(theme, "foreground", 0xE5, 0xE5, 0xE5);
    out->cursor = hex_of_semantic(theme, "cursor", 0xCB, 0xA6, 0xF7);

    unsigned char bg_r, bg_g, bg_b, fg_r, fg_g, fg_b;
    theme_rgb(theme, "background", &bg_r, &bg_g, &bg_b);
    theme_rgb(theme, "foreground", &fg_r, &fg_g, &fg_b);

    /* Slot 0 (black) is the background itself; slot 8 (bright black)
     * is a lightened step toward it being visible against slot 0's
     * exact value on its own background. Slots 7/15 (white/bright
     * white) both resolve to the foreground -- most `.calm` themes
     * don't distinguish "dim text" from "normal text" as separate
     * colors, and collapsing them is truer to the theme than
     * inventing a shade it never specified. */
    unsigned char r0 = bg_r, g0 = bg_g, b0 = bg_b;
    lighten(&r0, &g0, &b0, 0.35);
    out->ansi[0] = hex_of_rgb(bg_r, bg_g, bg_b);
    out->ansi[8] = hex_of_rgb(r0, g0, b0);
    out->ansi[7] = hex_of_rgb(fg_r, fg_g, fg_b);
    out->ansi[15] = hex_of_rgb(fg_r, fg_g, fg_b);

    for (size_t i = 0; i < 6; i++) {
        unsigned char r, g, b;
        nth_palette_rgb(theme, i, &r, &g, &b);
        out->ansi[1 + i] = hex_of_rgb(r, g, b);

        unsigned char br = r, bg = g, bb = b;
        lighten(&br, &bg, &bb, 0.22);
        out->ansi[9 + i] = hex_of_rgb(br, bg, bb);
    }
}

void color_scheme_free(TermColorScheme *cs) {
    free(cs->background);
    free(cs->foreground);
    free(cs->cursor);
    for (size_t i = 0; i < 16; i++) {
        free(cs->ansi[i]);
    }
}
