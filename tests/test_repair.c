#include "config/repair.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config/paths.h"
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
    char tmpdir_template[] = "/tmp/calm_repair_test_XXXXXX";
    char *tmpdir = mkdtemp(tmpdir_template);
    if (!tmpdir) {
        return NULL;
    }
    char *owned = xstrdup_local(tmpdir);
    setenv("XDG_CONFIG_HOME", owned, 1);
    return owned;
}

static const RepairCheck *find_check(const RepairReport *r, const char *label_substring) {
    for (size_t i = 0; i < r->count; i++) {
        if (strstr(r->items[i].label, label_substring)) {
            return &r->items[i];
        }
    }
    return NULL;
}

static void test_scan_on_missing_config_creates_everything(int *run, int *failed) {
    char *home = isolate_config_home();
    CHECK(run, failed, home != NULL);
    if (!home) {
        return;
    }

    RepairReport report;
    bool scanned = repair_scan(/*apply_fixes=*/true, &report);
    CHECK(run, failed, scanned);
    CHECK(run, failed, report.count > 0);

    const RepairCheck *config_check = find_check(&report, "config.calm is present");
    CHECK(run, failed, config_check != NULL);
    if (config_check) {
        CHECK(run, failed, config_check->status == REPAIR_FIXED);
    }

    char *cfg_path = config_file();
    CHECK(run, failed, path_is_file(cfg_path));
    free(cfg_path);

    /* A completely empty config directory has nothing to back up --
     * every file is *missing*, not corrupt, so nothing to preserve. */
    CHECK(run, failed, report.backups_made == 0);

    repair_report_free(&report);
    free(home);
}

/* Regression test for a real bug caught while building this: `calm
 * repair` must not treat config.calm as broken (and reset it to
 * defaults) just because a file config.calm *includes* is corrupt --
 * calm_parse_file() follows includes, so a naive per-file check
 * ordering makes the includer look broken for someone else's mistake. */
static void test_corrupt_include_does_not_reset_includer(int *run, int *failed) {
    char *home = isolate_config_home();
    CHECK(run, failed, home != NULL);
    if (!home) {
        return;
    }

    RepairReport setup_report;
    CHECK(run, failed, repair_scan(true, &setup_report));
    repair_report_free(&setup_report);

    char *cfg_path = config_file();
    char *original_config = read_file_to_string(cfg_path);
    CHECK(run, failed, original_config != NULL);

    char *env_path = environment_file();
    FILE *f = fopen(env_path, "w");
    CHECK(run, failed, f != NULL);
    if (f) {
        fputs("[shell\n", f); /* unterminated section header -- a parse error */
        fclose(f);
    }

    RepairReport report;
    CHECK(run, failed, repair_scan(/*apply_fixes=*/true, &report));

    const RepairCheck *env_check = find_check(&report, "environment.calm is present");
    CHECK(run, failed, env_check != NULL && env_check->status == REPAIR_FIXED);

    const RepairCheck *config_check = find_check(&report, "config.calm is present");
    CHECK(run, failed, config_check != NULL);
    if (config_check) {
        /* config.calm's own content was never broken -- once its
         * corrupt include is fixed first, config.calm should parse
         * fine on its own and be reported OK, not FIXED. */
        CHECK(run, failed, config_check->status == REPAIR_OK);
    }

    char *config_after = read_file_to_string(cfg_path);
    CHECK(run, failed, config_after != NULL);
    if (original_config && config_after) {
        CHECK(run, failed, strcmp(original_config, config_after) == 0);
    }

    repair_report_free(&report);
    free(config_after);
    free(original_config);
    free(env_path);
    free(cfg_path);
    free(home);
}

static void test_corrupt_bundled_theme_is_restored(int *run, int *failed) {
    char *home = isolate_config_home();
    CHECK(run, failed, home != NULL);
    if (!home) {
        return;
    }

    RepairReport setup_report;
    CHECK(run, failed, repair_scan(true, &setup_report));
    repair_report_free(&setup_report);

    char *themes = themes_dir();
    char *lavender_path = join_path(themes, "calm-lavender.calm");
    free(themes);
    FILE *f = fopen(lavender_path, "w");
    CHECK(run, failed, f != NULL);
    if (f) {
        fputs("this is not valid calm syntax [[[\n", f);
        fclose(f);
    }

    RepairReport report;
    CHECK(run, failed, repair_scan(/*apply_fixes=*/true, &report));
    const RepairCheck *theme_check = find_check(&report, "calm-lavender` loads");
    CHECK(run, failed, theme_check != NULL);
    if (theme_check) {
        CHECK(run, failed, theme_check->status == REPAIR_FIXED);
    }
    CHECK(run, failed, report.backups_made >= 1);
    CHECK(run, failed, report.backup_path != NULL && path_is_dir(report.backup_path));

    repair_report_free(&report);
    free(lavender_path);
    free(home);
}

static void test_custom_theme_reported_broken_not_overwritten(int *run, int *failed) {
    char *home = isolate_config_home();
    CHECK(run, failed, home != NULL);
    if (!home) {
        return;
    }

    RepairReport setup_report;
    CHECK(run, failed, repair_scan(true, &setup_report));
    repair_report_free(&setup_report);

    char *themes = themes_dir();
    char *custom_path = join_path(themes, "my-custom.calm");
    free(themes);
    FILE *f = fopen(custom_path, "w");
    CHECK(run, failed, f != NULL);
    if (f) {
        fputs("this is not valid calm syntax [[[\n", f);
        fclose(f);
    }

    RepairReport report;
    CHECK(run, failed, repair_scan(/*apply_fixes=*/true, &report));
    const RepairCheck *theme_check = find_check(&report, "my-custom` loads");
    CHECK(run, failed, theme_check != NULL);
    if (theme_check) {
        /* No known default for a user's own theme -- must be
         * reported, never silently regenerated. */
        CHECK(run, failed, theme_check->status == REPAIR_BROKEN);
    }

    char *content_after = read_file_to_string(custom_path);
    CHECK(run, failed, content_after != NULL);
    if (content_after) {
        CHECK(run, failed, strstr(content_after, "not valid calm syntax") != NULL);
    }

    repair_report_free(&report);
    free(content_after);
    free(custom_path);
    free(home);
}

static void test_plugin_manifest_missing_name_reported_broken(int *run, int *failed) {
    char *home = isolate_config_home();
    CHECK(run, failed, home != NULL);
    if (!home) {
        return;
    }

    RepairReport setup_report;
    CHECK(run, failed, repair_scan(true, &setup_report));
    repair_report_free(&setup_report);

    char *plugins = plugins_dir();
    char *plugin_dir = join_path(plugins, "unnamed");
    free(plugins);
    CHECK(run, failed, mkdir_recursive(plugin_dir));
    char *manifest_path = join_path(plugin_dir, "plugin.calm");
    FILE *f = fopen(manifest_path, "w");
    CHECK(run, failed, f != NULL);
    if (f) {
        fputs("version = \"1.0\"\n", f); /* parses fine, but no `name` */
        fclose(f);
    }

    RepairReport report;
    CHECK(run, failed, repair_scan(/*apply_fixes=*/false, &report));
    const RepairCheck *plugin_check = find_check(&report, "unnamed` manifest");
    CHECK(run, failed, plugin_check != NULL);
    if (plugin_check) {
        CHECK(run, failed, plugin_check->status == REPAIR_BROKEN);
    }

    repair_report_free(&report);
    free(manifest_path);
    free(plugin_dir);
    free(home);
}

void run_repair_tests(int *run, int *failed) {
    test_scan_on_missing_config_creates_everything(run, failed);
    test_corrupt_include_does_not_reset_includer(run, failed);
    test_corrupt_bundled_theme_is_restored(run, failed);
    test_custom_theme_reported_broken_not_overwritten(run, failed);
    test_plugin_manifest_missing_name_reported_broken(run, failed);
}
