/* Loads a `.calm` theme file and resolves semantic prompt-role colors
 * (e.g. "border", "arrow") to actual RGB, through the theme's
 * `[prompt]` section indirection -- or passes a direct palette key
 * (e.g. "calm_purple") through unchanged. Themes share the exact same
 * parser as every other piece of Calm-shell configuration; there is no
 * separate theme file format to learn. */
#ifndef CALM_THEME_THEME_H
#define CALM_THEME_THEME_H

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    char *key;
    unsigned char r, g, b;
} PaletteEntry;

typedef struct {
    char *name;
    char *display_name;
    char *icon;
    char *border_color;
    char *path_color;
    char *git_clean_color;
    char *git_dirty_color;
    char *arrow_color;

    /* Whole-terminal colors (as opposed to the prompt-role colors
     * above): what a synced terminal emulator's background,
     * foreground, and cursor should be. Resolved through the same
     * `[terminal]` indirection as everything else -- a palette key
     * name, or a literal "#RRGGBB" for a color that doesn't otherwise
     * need a name in `[colors]`. */
    char *background_color;
    char *foreground_color;
    char *cursor_color;

    PaletteEntry *palette;
    size_t palette_count;

    /* When false, theme_paint() always returns plain, unstyled text --
     * driven by terminal.calm's `truecolor` setting and the NO_COLOR
     * convention (https://no-color.org). Defaults to true; callers
     * that care set it right after a successful load. */
    bool colors_enabled;
} Theme;

/* Loads whichever theme is currently active (falls back to
 * calm-lavender if none is set or the active one is missing/corrupt). */
bool theme_load_active(Theme *out);
bool theme_load(const char *name, Theme *out);
void theme_free(Theme *t);

/* Resolves `semantic_key` ("border", "path", "git_clean", "git_dirty",
 * "arrow", or a direct palette key like "calm_purple") to RGB. Returns
 * false if the key or its target color isn't in the palette. */
bool theme_rgb(const Theme *t, const char *semantic_key, unsigned char *r, unsigned char *g, unsigned char *b);

/* Wraps `text` in a truecolor ANSI escape resolved from `semantic_key`,
 * returning a newly allocated string. Falls back to plain, unstyled
 * text if the key/color is missing or `colors_enabled` is false. */
char *theme_paint(const Theme *t, const char *semantic_key, const char *text);

#endif /* CALM_THEME_THEME_H */
