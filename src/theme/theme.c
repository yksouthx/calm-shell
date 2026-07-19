#include "theme/theme.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config/calmconf.h"
#include "config/paths.h"
#include "util/fsutil.h"
#include "util/xalloc.h"

static bool parse_hex(const char *hex, unsigned char *r, unsigned char *g, unsigned char *b) {
    if (hex[0] == '#') {
        hex++;
    }
    size_t len = strlen(hex);
    if (len != 6) {
        return false;
    }
    for (size_t i = 0; i < 6; i++) {
        if (!isxdigit((unsigned char)hex[i])) {
            return false;
        }
    }
    unsigned int rv, gv, bv;
    if (sscanf(hex, "%2x%2x%2x", &rv, &gv, &bv) != 3) {
        return false;
    }
    *r = (unsigned char)rv;
    *g = (unsigned char)gv;
    *b = (unsigned char)bv;
    return true;
}

static char *str_or_default(const CalmDocument *doc, const char *section, const char *key, const char *fallback) {
    const char *s = calm_document_get_str(doc, section, key);
    return xstrdup(s ? s : fallback);
}

void theme_free(Theme *t) {
    if (!t) {
        return;
    }
    free(t->name);
    free(t->display_name);
    free(t->icon);
    free(t->border_color);
    free(t->path_color);
    free(t->git_clean_color);
    free(t->git_dirty_color);
    free(t->arrow_color);
    free(t->background_color);
    free(t->foreground_color);
    free(t->cursor_color);
    for (size_t i = 0; i < t->palette_count; i++) {
        free(t->palette[i].key);
    }
    free(t->palette);
    memset(t, 0, sizeof(*t));
}

bool theme_load(const char *name, Theme *out) {
    char *dir = themes_dir();
    if (!dir) {
        return false;
    }
    char *filename = xsprintf("%s.calm", name);
    char *path = join_path(dir, filename);
    free(filename);
    free(dir);

    CalmDocument doc;
    char *err = NULL;
    bool parsed = calm_parse_file(path, &doc, &err);
    free(path);
    free(err);
    if (!parsed) {
        return false;
    }

    memset(out, 0, sizeof(*out));
    out->colors_enabled = true;
    out->name = str_or_default(&doc, "", "name", name);
    out->display_name = str_or_default(&doc, "", "display_name", name);

    out->icon = str_or_default(&doc, "prompt", "icon", "*");
    out->border_color = str_or_default(&doc, "prompt", "border_color", "cloud_gray");
    out->path_color = str_or_default(&doc, "prompt", "path_color", "soft_blue");
    out->git_clean_color = str_or_default(&doc, "prompt", "git_clean_color", "mint_green");
    out->git_dirty_color = str_or_default(&doc, "prompt", "git_dirty_color", "soft_red");
    out->arrow_color = str_or_default(&doc, "prompt", "arrow_color", "calm_purple");

    out->background_color = str_or_default(&doc, "terminal", "background", "#111111");
    out->foreground_color = str_or_default(&doc, "terminal", "foreground", "#E5E5E5");
    /* No hardcoded fallback color for the cursor -- default to
     * whatever the prompt arrow already resolves to, so a theme that
     * never mentions `[terminal]` at all still gets a cursor color
     * that visibly belongs to it. */
    out->cursor_color = str_or_default(&doc, "terminal", "cursor", out->arrow_color);

    const CalmSection *colors = calm_document_section(&doc, "colors");
    if (colors) {
        out->palette = xmalloc(colors->count * sizeof(PaletteEntry));
        out->palette_count = 0;
        for (size_t i = 0; i < colors->count; i++) {
            const CalmValue *v = &colors->entries[i].value;
            unsigned char r, g, b;
            if (v->type == CALM_STRING && parse_hex(v->as.string, &r, &g, &b)) {
                PaletteEntry *e = &out->palette[out->palette_count++];
                e->key = xstrdup(colors->entries[i].key);
                e->r = r;
                e->g = g;
                e->b = b;
            }
        }
    }

    calm_document_free(&doc);
    return true;
}

bool theme_load_active(Theme *out) {
    char *marker = active_theme_marker_file();
    char *name = marker ? read_file_to_string(marker) : NULL;
    free(marker);
    if (name) {
        /* The marker file holds exactly one line naming the active
         * theme; trim any trailing newline a text editor might have
         * added. */
        char *nl = strchr(name, '\n');
        if (nl) {
            *nl = '\0';
        }
    }
    const char *use_name = (name && name[0] != '\0') ? name : "calm-lavender";
    bool ok = theme_load(use_name, out);
    free(name);
    if (!ok && strcmp(use_name, "calm-lavender") != 0) {
        ok = theme_load("calm-lavender", out);
    }
    return ok;
}

/* Maps a semantic prompt-role key (e.g. "border", "arrow") to the
 * palette color name it's configured to use, or passes through direct
 * palette keys (e.g. "calm_purple") unchanged. */
static const char *resolve_color_name(const Theme *t, const char *semantic_key) {
    if (strcmp(semantic_key, "border") == 0) {
        return t->border_color;
    }
    if (strcmp(semantic_key, "path") == 0) {
        return t->path_color;
    }
    if (strcmp(semantic_key, "git_clean") == 0) {
        return t->git_clean_color;
    }
    if (strcmp(semantic_key, "git_dirty") == 0) {
        return t->git_dirty_color;
    }
    if (strcmp(semantic_key, "arrow") == 0) {
        return t->arrow_color;
    }
    if (strcmp(semantic_key, "background") == 0) {
        return t->background_color;
    }
    if (strcmp(semantic_key, "foreground") == 0) {
        return t->foreground_color;
    }
    if (strcmp(semantic_key, "cursor") == 0) {
        return t->cursor_color;
    }
    return semantic_key;
}

bool theme_rgb(const Theme *t, const char *semantic_key, unsigned char *r, unsigned char *g, unsigned char *b) {
    const char *color_name = resolve_color_name(t, semantic_key);
    /* A literal "#RRGGBB" resolves directly, without needing a
     * matching entry in `[colors]` -- lets `[terminal]` (and, for that
     * matter, any prompt-role key) name a one-off color it doesn't
     * care to give a palette name. */
    if (color_name[0] == '#') {
        return parse_hex(color_name, r, g, b);
    }
    for (size_t i = 0; i < t->palette_count; i++) {
        if (strcmp(t->palette[i].key, color_name) == 0) {
            *r = t->palette[i].r;
            *g = t->palette[i].g;
            *b = t->palette[i].b;
            return true;
        }
    }
    return false;
}

char *theme_paint(const Theme *t, const char *semantic_key, const char *text) {
    unsigned char r, g, b;
    if (t->colors_enabled && theme_rgb(t, semantic_key, &r, &g, &b)) {
        return xsprintf("\x1b[38;2;%u;%u;%um%s\x1b[0m", r, g, b, text);
    }
    return xstrdup(text);
}
