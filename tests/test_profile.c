#include "config/profile.h"

#include <stdio.h>
#include <stdlib.h>

#include "config/calmconf.h"
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

/* Points $XDG_CONFIG_HOME at a fresh scratch directory so every path
 * getter in config/paths.c resolves under it, the same isolation
 * trick test_theme.c uses -- never touches the real ~/.config. */
static char *isolate_config_home(void) {
    char tmpdir_template[] = "/tmp/calm_profile_test_XXXXXX";
    char *tmpdir = mkdtemp(tmpdir_template);
    if (!tmpdir) {
        return NULL;
    }
    char *owned = xstrdup_local(tmpdir);
    setenv("XDG_CONFIG_HOME", owned, 1);
    return owned;
}

static void write_file(const char *path, const char *contents) {
    FILE *f = fopen(path, "w");
    fputs(contents, f);
    fclose(f);
}

static void test_profile_load_and_apply_overrides_config(int *run, int *failed) {
    char *home = isolate_config_home();
    CHECK(run, failed, home != NULL);
    if (!home) {
        return;
    }

    CHECK(run, failed, ensure_scaffold());

    char *profiles = profiles_dir();
    CHECK(run, failed, profiles != NULL && mkdir_recursive(profiles));
    char *gaming_path = join_path(profiles, "gaming.calm");
    write_file(gaming_path,
               "[profile]\n"
               "theme = \"calm-lavender\"\n"
               "\n"
               "[shell]\n"
               "greeting = false\n"
               "\n"
               "[environment]\n"
               "GAME_MODE = \"1\"\n"
               "\n"
               "[plugins]\n"
               "enabled = [\"discord\", \"gamemode\"]\n");

    Profile p;
    char *err = NULL;
    bool loaded = profile_load("gaming", &p, &err);
    CHECK(run, failed, loaded);
    CHECK(run, failed, err == NULL);

    if (loaded) {
        char *warnings = NULL;
        bool applied = profile_apply(&p, &warnings);
        CHECK(run, failed, applied);
        CHECK(run, failed, warnings == NULL);
        free(warnings);
        profile_free(&p);

        char *active_name = profile_active_name();
        CHECK_STR_EQ(run, failed, active_name, "gaming");
        free(active_name);

        /* config.calm includes profile_active.calm last, so parsing
         * config.calm itself must show the override -- not just the
         * generated file in isolation -- proving the merge actually
         * takes effect where the rest of calm-shell reads from. */
        char *cfg_path = config_file();
        CalmDocument cfg;
        char *cfg_err = NULL;
        bool parsed = calm_parse_file(cfg_path, &cfg, &cfg_err);
        CHECK(run, failed, parsed);
        if (parsed) {
            CHECK(run, failed, calm_document_get_bool(&cfg, "shell", "greeting", true) == false);
            CHECK_STR_EQ(run, failed, calm_document_get_str(&cfg, "environment", "GAME_MODE"), "1");
            calm_document_free(&cfg);
        }
        free(cfg_err);
        free(cfg_path);
    }
    free(err);
    free(gaming_path);
    free(profiles);
    free(home);
}

static void test_profile_load_missing_fails(int *run, int *failed) {
    char *home = isolate_config_home();
    CHECK(run, failed, home != NULL);
    if (!home) {
        return;
    }
    CHECK(run, failed, ensure_scaffold());

    Profile p;
    char *err = NULL;
    bool loaded = profile_load("does-not-exist", &p, &err);
    CHECK(run, failed, !loaded);
    CHECK(run, failed, err != NULL);
    free(err);
    free(home);
}

void run_profile_tests(int *run, int *failed) {
    test_profile_load_and_apply_overrides_config(run, failed);
    test_profile_load_missing_fails(run, failed);
}
