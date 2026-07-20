#include "theme/app_sync.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config/scaffold.h"
#include "test_framework.h"
#include "util/fsutil.h"

static char *xstrdup_local(const char *s) {
    size_t len = 0;
    while (s[len]) {
        len++;
    }
    char *out = malloc(len + 1);
    for (size_t i = 0; i <= len; i++) {
        out[i] = s[i];
    }
    return out;
}

static char *isolate_config_home(void) {
    char tmpdir_template[] = "/tmp/calm_appsync_test_XXXXXX";
    char *tmpdir = mkdtemp(tmpdir_template);
    if (!tmpdir) {
        return NULL;
    }
    char *owned = xstrdup_local(tmpdir);
    setenv("XDG_CONFIG_HOME", owned, 1);
    return owned;
}

static bool load_bundled_lavender(Theme *out) {
    char *themes_dir_path = join_path(getenv("XDG_CONFIG_HOME"), "calm-shell");
    mkdir_recursive(themes_dir_path);
    char *themes_subdir = join_path(themes_dir_path, "themes");
    mkdir_recursive(themes_subdir);
    char *theme_path = join_path(themes_subdir, "calm-lavender.calm");
    FILE *f = fopen(theme_path, "w");
    if (f) {
        fputs(default_lavender_theme(), f);
        fclose(f);
    }
    free(theme_path);
    free(themes_subdir);
    free(themes_dir_path);
    return theme_load("calm-lavender", out);
}

static const AppSyncEntry *find_entry(const AppSyncResult *r, const char *app) {
    for (size_t i = 0; i < r->count; i++) {
        if (strcmp(r->items[i].app, app) == 0) {
            return &r->items[i];
        }
    }
    return NULL;
}

static void test_all_apps_sync_on_fresh_config(int *run, int *failed) {
    char *home = isolate_config_home();
    CHECK(run, failed, home != NULL);
    if (!home) {
        return;
    }
    Theme t;
    CHECK(run, failed, load_bundled_lavender(&t));

    AppSyncResult result = {0};
    app_sync_write_all(&t, &result);
    CHECK(run, failed, result.count == 9);
    for (size_t i = 0; i < result.count; i++) {
        CHECK(run, failed, result.items[i].synced);
        CHECK(run, failed, result.items[i].detail != NULL);
    }

    app_sync_result_free(&result);
    theme_free(&t);
    free(home);
}

/* Regression test: a top-level (no [section] headers) config file
 * like btop.conf or Helix's config.toml must never get a spurious
 * "[]" line -- ini_set()'s empty-section case is genuinely different
 * from "the [foo] section wasn't found yet". */
static void test_top_level_config_gets_no_bogus_section_header(int *run, int *failed) {
    char *home = isolate_config_home();
    CHECK(run, failed, home != NULL);
    if (!home) {
        return;
    }
    Theme t;
    CHECK(run, failed, load_bundled_lavender(&t));

    AppSyncResult result = {0};
    app_sync_write_all(&t, &result);

    char *btop_conf_path = join_path(home, "btop/btop.conf");
    char *btop_conf = read_file_to_string(btop_conf_path);
    CHECK(run, failed, btop_conf != NULL);
    if (btop_conf) {
        CHECK(run, failed, strstr(btop_conf, "[]") == NULL);
        CHECK(run, failed, strstr(btop_conf, "color_theme = \"calm-shell\"") != NULL);
    }
    free(btop_conf);
    free(btop_conf_path);

    char *helix_conf_path = join_path(home, "helix/config.toml");
    char *helix_conf = read_file_to_string(helix_conf_path);
    CHECK(run, failed, helix_conf != NULL);
    if (helix_conf) {
        CHECK(run, failed, strstr(helix_conf, "[]") == NULL);
        CHECK(run, failed, strstr(helix_conf, "theme = \"calm-shell\"") != NULL);
    }
    free(helix_conf);
    free(helix_conf_path);

    app_sync_result_free(&result);
    theme_free(&t);
    free(home);
}

/* Apps with no include/import mechanism (Cava, Fuzzel) must only
 * patch their own owned keys -- every other line the user already
 * had in the file must survive untouched. */
static void test_key_patch_apps_preserve_unrelated_user_settings(int *run, int *failed) {
    char *home = isolate_config_home();
    CHECK(run, failed, home != NULL);
    if (!home) {
        return;
    }
    Theme t;
    CHECK(run, failed, load_bundled_lavender(&t));

    char *fuzzel_dir = join_path(home, "fuzzel");
    mkdir_recursive(fuzzel_dir);
    char *fuzzel_path = join_path(fuzzel_dir, "fuzzel.ini");
    FILE *f = fopen(fuzzel_path, "w");
    CHECK(run, failed, f != NULL);
    if (f) {
        fputs("[main]\nterminal=kitty\nfont=JetBrainsMono:size=14\n", f);
        fclose(f);
    }

    AppSyncResult result = {0};
    app_sync_write_all(&t, &result);

    char *after = read_file_to_string(fuzzel_path);
    CHECK(run, failed, after != NULL);
    if (after) {
        CHECK(run, failed, strstr(after, "terminal=kitty") != NULL);
        CHECK(run, failed, strstr(after, "font=JetBrainsMono:size=14") != NULL);
        CHECK(run, failed, strstr(after, "[colors]") != NULL);
    }

    app_sync_result_free(&result);
    free(after);
    free(fuzzel_path);
    free(fuzzel_dir);
    theme_free(&t);
    free(home);
}

/* Include-based apps (Waybar here) must preserve the user's existing
 * stylesheet content and only ever add the @import line once, even
 * across repeated syncs. */
static void test_include_apps_are_idempotent_and_non_destructive(int *run, int *failed) {
    char *home = isolate_config_home();
    CHECK(run, failed, home != NULL);
    if (!home) {
        return;
    }
    Theme t;
    CHECK(run, failed, load_bundled_lavender(&t));

    char *waybar_dir = join_path(home, "waybar");
    mkdir_recursive(waybar_dir);
    char *style_path = join_path(waybar_dir, "style.css");
    FILE *f = fopen(style_path, "w");
    CHECK(run, failed, f != NULL);
    if (f) {
        fputs("#workspaces { padding: 4px; }\n", f);
        fclose(f);
    }

    AppSyncResult r1 = {0};
    app_sync_write_all(&t, &r1);
    AppSyncResult r2 = {0};
    app_sync_write_all(&t, &r2);

    char *after = read_file_to_string(style_path);
    CHECK(run, failed, after != NULL);
    if (after) {
        CHECK(run, failed, strstr(after, "#workspaces { padding: 4px; }") != NULL);
        /* Exactly one @import line, not two, after two sync runs. */
        const char *first = strstr(after, "@import \"calm-shell-theme.css\";");
        CHECK(run, failed, first != NULL);
        if (first) {
            const char *second = strstr(first + 1, "@import \"calm-shell-theme.css\";");
            CHECK(run, failed, second == NULL);
        }
    }

    app_sync_result_free(&r1);
    app_sync_result_free(&r2);
    free(after);
    free(style_path);
    free(waybar_dir);
    theme_free(&t);
    free(home);
}

void run_app_sync_tests(int *run, int *failed) {
    test_all_apps_sync_on_fresh_config(run, failed);
    test_top_level_config_gets_no_bogus_section_header(run, failed);
    test_key_patch_apps_preserve_unrelated_user_settings(run, failed);
    test_include_apps_are_idempotent_and_non_destructive(run, failed);
}
