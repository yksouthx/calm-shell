#include "theme/theme.h"

#include <stdio.h>
#include <stdlib.h>

#include "config/scaffold.h"
#include "test_framework.h"
#include "util/fsutil.h"

static void test_bundled_lavender_theme_parses(int *run, int *failed) {
    char tmpdir_template[] = "/tmp/calm_theme_test_XXXXXX";
    char *tmpdir = mkdtemp(tmpdir_template);
    CHECK(run, failed, tmpdir != NULL);
    if (!tmpdir) {
        return;
    }
    /* theme_load() always resolves against themes_dir(), which is
     * derived from $XDG_CONFIG_HOME; point that at our scratch
     * directory so the theme we write here is exactly what it finds,
     * without ever touching the real ~/.config. */
    setenv("XDG_CONFIG_HOME", tmpdir, 1);
    char *themes_subdir = join_path(tmpdir, "calm-shell");
    mkdir_recursive(themes_subdir);
    char *themes_subsubdir = join_path(themes_subdir, "themes");
    mkdir_recursive(themes_subsubdir);
    char *final_path = join_path(themes_subsubdir, "calm-lavender.calm");
    FILE *f2 = fopen(final_path, "w");
    fputs(default_lavender_theme(), f2);
    fclose(f2);

    Theme t;
    bool ok = theme_load("calm-lavender", &t);
    CHECK(run, failed, ok);
    if (ok) {
        CHECK_STR_EQ(run, failed, t.name, "calm-lavender");
        CHECK_STR_EQ(run, failed, t.display_name, "Calm Lavender");
        CHECK(run, failed, t.palette_count == 7);

        unsigned char r, g, b;
        bool resolved = theme_rgb(&t, "border", &r, &g, &b);
        CHECK(run, failed, resolved);
        if (resolved) {
            /* border_color = "cloud_gray" = #BAC2DE */
            CHECK(run, failed, r == 0xBA && g == 0xC2 && b == 0xDE);
        }

        bool resolved_direct = theme_rgb(&t, "calm_purple", &r, &g, &b);
        CHECK(run, failed, resolved_direct);
        if (resolved_direct) {
            CHECK(run, failed, r == 0xCB && g == 0xA6 && b == 0xF7);
        }

        theme_free(&t);
    }

    free(themes_subdir);
    free(themes_subsubdir);
    free(final_path);
}

static void test_colors_disabled_paints_plain_text(int *run, int *failed) {
    Theme t = {0};
    t.colors_enabled = false;
    t.border_color = NULL; /* would crash if theme_paint tried to resolve it while disabled */
    char *painted = theme_paint(&t, "border", "hello");
    CHECK_STR_EQ(run, failed, painted, "hello");
    free(painted);
}

void run_theme_tests(int *run, int *failed) {
    test_bundled_lavender_theme_parses(run, failed);
    test_colors_disabled_paints_plain_text(run, failed);
}
