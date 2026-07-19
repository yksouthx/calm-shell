/* Derives one normalized terminal color scheme (background,
 * foreground, cursor, and a full 16-slot ANSI palette) from a Theme,
 * shared by every consumer that needs a whole-terminal color set --
 * the terminal emulator theme writers and the Fastfetch config
 * generator alike. This is the one place that turns "however many
 * colors a `.calm` theme happens to define" into "exactly 16 ANSI
 * slots", so every synced application derives its palette the same
 * way instead of five slightly-different reimplementations. */
#ifndef CALM_THEME_COLOR_SCHEME_H
#define CALM_THEME_COLOR_SCHEME_H

#include "theme/theme.h"

typedef struct {
    char *background; /* "#RRGGBB", owned */
    char *foreground;
    char *cursor;
    /* Standard ANSI slots: 0-7 normal (black, red, green, yellow,
     * blue, magenta, cyan, white), 8-15 the bright variants. */
    char *ansi[16];
} TermColorScheme;

/* Builds `out` from `theme`. Always succeeds -- every slot gets a
 * color, falling back to blended/derived shades when the theme's own
 * `[colors]` palette has fewer entries than ANSI wants. */
void color_scheme_build(const Theme *theme, TermColorScheme *out);
void color_scheme_free(TermColorScheme *cs);

#endif /* CALM_THEME_COLOR_SCHEME_H */
