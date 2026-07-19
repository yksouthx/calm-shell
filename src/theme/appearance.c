#include "theme/appearance.h"

#include <stdlib.h>
#include <string.h>

#include "util/xalloc.h"

static CursorStyle parse_cursor_style(const char *s) {
    if (!s) {
        return CURSOR_STYLE_BEAM;
    }
    if (strcmp(s, "block") == 0) {
        return CURSOR_STYLE_BLOCK;
    }
    if (strcmp(s, "underline") == 0) {
        return CURSOR_STYLE_UNDERLINE;
    }
    return CURSOR_STYLE_BEAM;
}

const char *appearance_cursor_style_name(CursorStyle s) {
    switch (s) {
        case CURSOR_STYLE_BLOCK:
            return "block";
        case CURSOR_STYLE_UNDERLINE:
            return "underline";
        case CURSOR_STYLE_BEAM:
        default:
            return "beam";
    }
}

void appearance_load(const CalmDocument *cfg, TermAppearance *out) {
    memset(out, 0, sizeof(*out));

    const char *font = calm_document_get_str(cfg, "appearance", "font_family");
    out->font_family = xstrdup(font ? font : "monospace");

    out->font_size = calm_document_get_float(cfg, "appearance", "font_size", 12.0);
    out->cursor_style = parse_cursor_style(calm_document_get_str(cfg, "appearance", "cursor_style"));
    out->cursor_blink = calm_document_get_bool(cfg, "appearance", "cursor_blink", true);
    out->opacity = calm_document_get_float(cfg, "appearance", "opacity", 1.0);
    if (out->opacity < 0.0) {
        out->opacity = 0.0;
    } else if (out->opacity > 1.0) {
        out->opacity = 1.0;
    }
    out->padding = calm_document_get_int(cfg, "appearance", "padding", 8);
    if (out->padding < 0) {
        out->padding = 0;
    }
    out->window_decorations = calm_document_get_bool(cfg, "appearance", "window_decorations", true);
}

void appearance_free(TermAppearance *a) {
    free(a->font_family);
    a->font_family = NULL;
}
