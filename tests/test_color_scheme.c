#include "theme/color_scheme.h"

#include <stdlib.h>
#include <string.h>

#include "test_framework.h"
#include "util/xalloc.h"

/* Builds a minimal in-memory Theme -- no file parsing -- with a
 * 3-entry palette and explicit background/foreground/cursor, so
 * color_scheme_build's output is fully predictable. */
static void make_theme(Theme *t) {
    memset(t, 0, sizeof(*t));
    t->background_color = xstrdup("#101010");
    t->foreground_color = xstrdup("#F0F0F0");
    t->cursor_color = xstrdup("#FF00FF");
    t->arrow_color = xstrdup("#FF00FF");

    t->palette_count = 3;
    t->palette = xmalloc(sizeof(PaletteEntry) * 3);
    t->palette[0] = (PaletteEntry){.key = xstrdup("a"), .r = 0x10, .g = 0x20, .b = 0x30};
    t->palette[1] = (PaletteEntry){.key = xstrdup("b"), .r = 0x40, .g = 0x50, .b = 0x60};
    t->palette[2] = (PaletteEntry){.key = xstrdup("c"), .r = 0x70, .g = 0x80, .b = 0x90};
}

static void free_theme(Theme *t) {
    free(t->background_color);
    free(t->foreground_color);
    free(t->cursor_color);
    free(t->arrow_color);
    for (size_t i = 0; i < t->palette_count; i++) {
        free(t->palette[i].key);
    }
    free(t->palette);
}

static void test_background_foreground_cursor_passthrough(int *run, int *failed) {
    Theme t;
    make_theme(&t);
    TermColorScheme cs;
    color_scheme_build(&t, &cs);

    CHECK_STR_EQ(run, failed, cs.background, "#101010");
    CHECK_STR_EQ(run, failed, cs.foreground, "#F0F0F0");
    CHECK_STR_EQ(run, failed, cs.cursor, "#FF00FF");
    /* Slot 0 (ANSI black) is the background itself; 7 and 15 both
     * resolve to the foreground. */
    CHECK_STR_EQ(run, failed, cs.ansi[0], "#101010");
    CHECK_STR_EQ(run, failed, cs.ansi[7], "#F0F0F0");
    CHECK_STR_EQ(run, failed, cs.ansi[15], "#F0F0F0");

    color_scheme_free(&cs);
    free_theme(&t);
}

static void test_palette_cycles_when_short(int *run, int *failed) {
    Theme t;
    make_theme(&t);
    TermColorScheme cs;
    color_scheme_build(&t, &cs);

    /* Only 3 palette entries feed 6 normal-color slots (1-6): slot 1
     * and slot 4 must both land on palette[0] (index 0 and 3, 3 % 3
     * == 0). */
    CHECK_STR_EQ(run, failed, cs.ansi[1], cs.ansi[4]);
    CHECK_STR_EQ(run, failed, cs.ansi[1], "#102030");
    CHECK_STR_EQ(run, failed, cs.ansi[2], "#405060");
    CHECK_STR_EQ(run, failed, cs.ansi[3], "#708090");

    color_scheme_free(&cs);
    free_theme(&t);
}

static void test_bright_variants_differ_from_normal(int *run, int *failed) {
    Theme t;
    make_theme(&t);
    TermColorScheme cs;
    color_scheme_build(&t, &cs);

    /* The bright counterpart of a normal-color slot is a lightened
     * version of the same base color, not an identical copy. */
    CHECK(run, failed, strcmp(cs.ansi[1], cs.ansi[9]) != 0);

    color_scheme_free(&cs);
    free_theme(&t);
}

static void test_empty_palette_does_not_crash(int *run, int *failed) {
    Theme t;
    make_theme(&t);
    for (size_t i = 0; i < t.palette_count; i++) {
        free(t.palette[i].key);
    }
    free(t.palette);
    t.palette = NULL;
    t.palette_count = 0;

    TermColorScheme cs;
    color_scheme_build(&t, &cs);
    CHECK(run, failed, cs.ansi[1] != NULL);

    color_scheme_free(&cs);
    free(t.background_color);
    free(t.foreground_color);
    free(t.cursor_color);
    free(t.arrow_color);
}

void run_color_scheme_tests(int *run, int *failed) {
    test_background_foreground_cursor_passthrough(run, failed);
    test_palette_cycles_when_short(run, failed);
    test_bright_variants_differ_from_normal(run, failed);
    test_empty_palette_does_not_crash(run, failed);
}
