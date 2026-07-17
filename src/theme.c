#include "theme.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "commands/theme_cmd.h"
#include "config.h"
#include "json.h"
#include "util.h"

static bool parse_hex(const char *hex, unsigned char *r, unsigned char *g, unsigned char *b) {
    if (hex[0] == '#') {
        hex++;
    }
    size_t len = strlen(hex);
    /* Guard against a hand-edited theme.json containing a stray
     * non-ASCII character that happens to leave the string at 6
     * *bytes* while not actually being 6 hex digits. */
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

static char *dup_or_default(const JsonValue *obj, const char *key, const char *fallback) {
    const char *s = json_as_string(json_object_get(obj, key));
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
    for (size_t i = 0; i < t->palette_count; i++) {
        free(t->palette[i].key);
    }
    free(t->palette);
    memset(t, 0, sizeof(*t));
}

bool theme_load(const char *name, Theme *out, char **err_msg_unused) {
    (void)err_msg_unused;
    char *dir = themes_dir();
    if (!dir) {
        return false;
    }
    char *filename = xsprintf("%s.json", name);
    char *path = join_path(dir, filename);
    free(filename);
    free(dir);

    char *raw = read_file_to_string(path);
    free(path);
    if (!raw) {
        return false;
    }

    char *json_err = NULL;
    JsonValue *root = json_parse(raw, &json_err);
    free(raw);
    free(json_err);
    if (!root) {
        return false;
    }

    memset(out, 0, sizeof(*out));
    out->name = dup_or_default(root, "name", name);
    out->display_name = dup_or_default(root, "display_name", name);

    const JsonValue *prompt = json_object_get(root, "prompt");
    out->icon = dup_or_default(prompt, "icon", "*");
    out->border_color = dup_or_default(prompt, "border_color", "cloud_gray");
    out->path_color = dup_or_default(prompt, "path_color", "soft_blue");
    out->git_clean_color = dup_or_default(prompt, "git_clean_color", "mint_green");
    out->git_dirty_color = dup_or_default(prompt, "git_dirty_color", "soft_red");
    out->arrow_color = dup_or_default(prompt, "arrow_color", "calm_purple");

    const JsonValue *colors = json_object_get(root, "colors");
    if (colors && colors->type == JSON_OBJECT) {
        out->palette = xmalloc(colors->member_count * sizeof(PaletteEntry));
        out->palette_count = 0;
        for (size_t i = 0; i < colors->member_count; i++) {
            const char *hex = json_as_string(colors->members[i].value);
            unsigned char r, g, b;
            if (hex && parse_hex(hex, &r, &g, &b)) {
                PaletteEntry *e = &out->palette[out->palette_count++];
                e->key = xstrdup(colors->members[i].key);
                e->r = r;
                e->g = g;
                e->b = b;
            }
        }
    }

    json_free(root);
    return true;
}

bool theme_load_active(Theme *out) {
    char *name = current_theme();
    const char *use_name = name ? name : "calm-lavender";
    bool ok = theme_load(use_name, out, NULL);
    free(name);
    if (!ok) {
        ok = theme_load("calm-lavender", out, NULL);
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
    return semantic_key;
}

bool theme_rgb(const Theme *t, const char *semantic_key, unsigned char *r, unsigned char *g, unsigned char *b) {
    const char *color_name = resolve_color_name(t, semantic_key);
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
    if (theme_rgb(t, semantic_key, &r, &g, &b)) {
        return xsprintf("\x1b[38;2;%u;%u;%um%s\x1b[0m", r, g, b, text);
    }
    return xstrdup(text);
}
