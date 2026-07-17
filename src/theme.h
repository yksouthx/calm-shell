/* Loads a theme.json file and resolves semantic prompt-role colors
 * (e.g. "border", "arrow") to actual RGB, through the theme's `prompt`
 * section indirection, or passes direct palette keys (e.g.
 * "calm_purple") through unchanged. */
#ifndef CALM_THEME_H
#define CALM_THEME_H

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
    PaletteEntry *palette;
    size_t palette_count;
} Theme;

/* Loads whichever theme is currently active (falls back to
 * calm-lavender if none is set or the active one is missing/corrupt). */
bool theme_load_active(Theme *out);
bool theme_load(const char *name, Theme *out, char **err_msg);
void theme_free(Theme *t);

/* Resolves `semantic_key` ("border", "path", "git_clean", "git_dirty",
 * "arrow", or a direct palette key like "calm_purple") to RGB. Returns
 * false if the key or its target color isn't in the palette. */
bool theme_rgb(const Theme *t, const char *semantic_key, unsigned char *r, unsigned char *g, unsigned char *b);

/* Wraps `text` in a truecolor ANSI escape resolved from `semantic_key`,
 * returning a newly allocated string. Falls back to plain, unstyled
 * text if the key or color is missing. */
char *theme_paint(const Theme *t, const char *semantic_key, const char *text);

#endif /* CALM_THEME_H */
