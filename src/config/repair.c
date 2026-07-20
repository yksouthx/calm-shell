#include "config/repair.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "config/calmconf.h"
#include "config/paths.h"
#include "config/plugin_loader.h"
#include "config/scaffold.h"
#include "theme/sync.h"
#include "theme/theme.h"
#include "util/fsutil.h"
#include "util/xalloc.h"

static void add_check(RepairReport *r, RepairStatus status, const char *label, const char *detail) {
    if (r->count == r->cap) {
        r->cap = r->cap == 0 ? 8 : r->cap * 2;
        r->items = xrealloc(r->items, r->cap * sizeof(RepairCheck));
    }
    r->items[r->count].status = status;
    r->items[r->count].label = xstrdup(label);
    r->items[r->count].detail = detail ? xstrdup(detail) : NULL;
    r->count++;
}

void repair_report_free(RepairReport *r) {
    for (size_t i = 0; i < r->count; i++) {
        free(r->items[i].label);
        free(r->items[i].detail);
    }
    free(r->items);
    free(r->backup_path);
    r->items = NULL;
    r->count = r->cap = 0;
    r->backup_path = NULL;
}

/* Lazily creates (once per repair_scan run) a fresh, timestamped
 * directory under backups_dir() the first time something actually
 * needs backing up -- a run that finds nothing corrupt never creates
 * an empty backup folder. */
static bool ensure_backup_dir(RepairReport *r) {
    if (r->backup_path) {
        return path_is_dir(r->backup_path);
    }
    char *base = backups_dir();
    if (!base) {
        return false;
    }
    time_t now = time(NULL);
    struct tm tm_buf;
    localtime_r(&now, &tm_buf);
    char stamp[32];
    strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", &tm_buf);
    char *dir = join_path(base, stamp);
    free(base);
    bool ok = mkdir_recursive(dir);
    if (ok) {
        r->backup_path = dir;
    } else {
        free(dir);
    }
    return ok;
}

/* Copies whatever currently exists at `original_path` into this run's
 * backup directory under `backup_name` before it gets overwritten or
 * replaced -- a no-op that still reports success if `apply_fixes` is
 * false (a scan changes nothing, so there's nothing to preserve) or
 * the original file doesn't exist yet (recreating a missing file
 * isn't overwriting anyone's content). */
static bool backup_file(RepairReport *r, bool apply_fixes, const char *original_path, const char *backup_name) {
    if (!apply_fixes || !path_is_file(original_path)) {
        return true;
    }
    if (!ensure_backup_dir(r)) {
        return false;
    }
    char *content = read_file_to_string(original_path);
    if (!content) {
        return false;
    }
    char *dest = join_path(r->backup_path, backup_name);
    bool ok = overwrite_file(dest, content);
    free(dest);
    free(content);
    if (ok) {
        r->backups_made++;
    }
    return ok;
}

static void check_dir(RepairReport *r, bool apply_fixes, char *(*dir_fn)(void), const char *label) {
    char *path = dir_fn();
    if (!path) {
        add_check(r, REPAIR_BROKEN, label, "could not resolve path");
        return;
    }
    bool existed = path_is_dir(path);
    if (!existed && apply_fixes) {
        mkdir_recursive(path);
    }
    bool ok = path_is_dir(path);
    RepairStatus status = ok ? (existed ? REPAIR_OK : REPAIR_FIXED) : REPAIR_BROKEN;
    add_check(r, status, label,
              ok ? NULL : "could not create directory -- check permissions on ~/.config/calm-shell");
    free(path);
}

/* Restores one scaffold-known topic file: recreates it if missing,
 * or -- if it exists but fails to parse -- backs it up and restores
 * it to the same default a fresh install would get. This is the
 * module's whole "repair syntax errors where possible" strategy: not
 * a patch, a restore to known-good, which is the only thing that can
 * be done safely without guessing at what the user meant. */
static void check_topic_file(RepairReport *r, bool apply_fixes, const ScaffoldDefault *def) {
    char *path = def->path_fn();
    char label[160];
    snprintf(label, sizeof(label), "%s is present and parses", def->label);

    if (!path_is_file(path)) {
        if (apply_fixes) {
            bool ok = overwrite_file(path, def->content_fn());
            add_check(r, ok ? REPAIR_FIXED : REPAIR_BROKEN, label,
                      ok ? "was missing -- recreated from defaults" : "missing, and could not be created");
        } else {
            add_check(r, REPAIR_BROKEN, label, "missing (run `calm repair` to recreate from defaults)");
        }
        free(path);
        return;
    }

    CalmDocument doc;
    char *err = NULL;
    if (calm_parse_file(path, &doc, &err)) {
        calm_document_free(&doc);
        add_check(r, REPAIR_OK, label, NULL);
        free(err);
        free(path);
        return;
    }

    if (apply_fixes) {
        bool backed_up = backup_file(r, apply_fixes, path, def->label);
        bool restored = overwrite_file(path, def->content_fn());
        char detail[320];
        snprintf(detail, sizeof(detail), "parse error (%s) -- %s%s restored to defaults", err ? err : "unknown",
                  backed_up ? "backed up and " : "(backup failed) ", restored ? "" : "NOT");
        add_check(r, restored ? REPAIR_FIXED : REPAIR_BROKEN, label, detail);
    } else {
        char detail[280];
        snprintf(detail, sizeof(detail), "parse error: %s", err ? err : "unknown");
        add_check(r, REPAIR_BROKEN, label, detail);
    }
    free(err);
    free(path);
}

static void check_bundled_theme_present(RepairReport *r, bool apply_fixes) {
    char *dir = themes_dir();
    if (!dir) {
        return;
    }
    char *path = join_path(dir, "calm-lavender.calm");
    free(dir);
    if (path_is_file(path)) {
        free(path);
        return; /* check_themes() will validate its content */
    }
    if (apply_fixes) {
        bool ok = overwrite_file(path, default_lavender_theme());
        add_check(r, ok ? REPAIR_FIXED : REPAIR_BROKEN, "calm-lavender bundled theme is present",
                   ok ? "was missing -- recreated from the bundled default" : "missing, and could not be created");
    } else {
        add_check(r, REPAIR_BROKEN, "calm-lavender bundled theme is present",
                   "missing (run `calm repair` to recreate from the bundled default)");
    }
    free(path);
}

static void check_themes(RepairReport *r, bool apply_fixes) {
    char *dir = themes_dir();
    if (!dir) {
        return;
    }
    DIR *d = opendir(dir);
    if (!d) {
        free(dir);
        return;
    }
    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        size_t name_len = strlen(entry->d_name);
        if (name_len <= 5 || strcmp(entry->d_name + name_len - 5, ".calm") != 0) {
            continue;
        }
        char *theme_name = xstrndup(entry->d_name, name_len - 5);
        char label[288];
        snprintf(label, sizeof(label), "theme `%s` loads", theme_name);

        Theme t;
        if (theme_load(theme_name, &t)) {
            theme_free(&t);
            add_check(r, REPAIR_OK, label, NULL);
        } else if (strcmp(theme_name, "calm-lavender") == 0) {
            /* The one theme with a known default -- every other theme
             * is either bundled community content or user-authored,
             * and calm-shell has no basis to guess what it should
             * contain. */
            char *theme_path = join_path(dir, entry->d_name);
            if (apply_fixes) {
                bool backed_up = backup_file(r, apply_fixes, theme_path, "calm-lavender.calm");
                bool restored = overwrite_file(theme_path, default_lavender_theme());
                char detail[200];
                snprintf(detail, sizeof(detail), "%srestored to the bundled default",
                          backed_up ? "backed up and " : "(backup failed) ");
                add_check(r, restored ? REPAIR_FIXED : REPAIR_BROKEN, label, detail);
            } else {
                add_check(r, REPAIR_BROKEN, label, "broken (run `calm repair` to restore the bundled default)");
            }
            free(theme_path);
        } else {
            add_check(r, REPAIR_BROKEN, label,
                      "custom theme -- no safe default to restore automatically; fix or remove it by hand");
        }
        free(theme_name);
    }
    closedir(d);
    free(dir);
}

static void check_plugins(RepairReport *r) {
    char *dir = plugins_dir();
    if (!dir) {
        return;
    }
    DIR *d = opendir(dir);
    if (!d) {
        free(dir);
        return;
    }
    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (entry->d_name[0] == '.') {
            continue;
        }
        char *plugin_dir = join_path(dir, entry->d_name);
        if (!path_is_dir(plugin_dir)) {
            free(plugin_dir);
            continue;
        }
        char label[288];
        snprintf(label, sizeof(label), "plugin `%s` manifest is valid", entry->d_name);

        char *manifest_path = join_path(plugin_dir, "plugin.calm");
        if (!path_is_file(manifest_path)) {
            add_check(r, REPAIR_BROKEN, label, "missing plugin.calm -- reinstall with `calm plugin install`");
        } else {
            CalmDocument manifest;
            char *err = NULL;
            if (!calm_parse_file(manifest_path, &manifest, &err)) {
                char detail[280];
                snprintf(detail, sizeof(detail), "plugin.calm parse error: %s", err ? err : "unknown");
                add_check(r, REPAIR_BROKEN, label, detail);
            } else {
                const char *name = calm_document_get_str(&manifest, "", "name");
                bool has_name = name && name[0] != '\0';
                add_check(r, has_name ? REPAIR_OK : REPAIR_BROKEN, label,
                          has_name ? NULL : "plugin.calm is missing its required `name` field");
                calm_document_free(&manifest);
            }
            free(err);
        }
        free(manifest_path);
        free(plugin_dir);
    }
    closedir(d);
    free(dir);
}

static void check_permissions(RepairReport *r) {
    struct {
        char *(*fn)(void);
        const char *label;
    } targets[] = {
        {config_dir, "~/.config/calm-shell is readable and writable"},
        {config_file, "config.calm is readable and writable"},
        {themes_dir, "themes directory is readable and writable"},
        {plugins_dir, "plugins directory is readable and writable"},
        {profiles_dir, "profiles directory is readable and writable"},
    };
    for (size_t i = 0; i < sizeof(targets) / sizeof(targets[0]); i++) {
        char *path = targets[i].fn();
        if (!path) {
            continue;
        }
        /* Only checked if the path actually exists -- a missing file
         * is already reported by check_topic_file()/check_dir(); a
         * permission check on something that isn't there yet would
         * just be noise. */
        if (path_exists(path)) {
            bool ok = access(path, R_OK | W_OK) == 0;
            add_check(r, ok ? REPAIR_OK : REPAIR_BROKEN, targets[i].label,
                      ok ? NULL
                         : "calm-shell won't chmod/chown another user's files automatically -- fix ownership "
                           "or permissions by hand");
        }
        free(path);
    }
}

static void check_sync(RepairReport *r, bool apply_fixes) {
    char *cfg_path = config_file();
    CalmDocument cfg;
    char *cfg_err = NULL;
    bool cfg_ok = cfg_path && calm_parse_file(cfg_path, &cfg, &cfg_err);
    free(cfg_err);
    free(cfg_path);
    if (!cfg_ok) {
        add_check(r, REPAIR_BROKEN, "terminal + Fastfetch + app sync", "config.calm still doesn't parse -- fix that first");
        return;
    }

    Theme theme;
    bool theme_ok = theme_load_active(&theme);
    if (!theme_ok) {
        add_check(r, REPAIR_BROKEN, "terminal + Fastfetch + app sync", "no usable theme found");
        calm_document_free(&cfg);
        return;
    }

    if (apply_fixes) {
        ThemeSyncResult result;
        theme_sync_apply(&theme, &cfg, &result);
        bool all_ok = (!result.terminal_enabled || result.terminal_synced) &&
                       (!result.fastfetch_enabled || result.fastfetch_synced);
        int apps_synced = 0;
        for (size_t i = 0; i < result.apps.count; i++) {
            if (result.apps.items[i].synced) {
                apps_synced++;
            } else {
                all_ok = false;
            }
        }
        char detail[320];
        snprintf(detail, sizeof(detail), "terminal: %s | fastfetch: %s | apps: %d/%zu",
                  result.terminal_enabled ? (result.terminal_synced ? "regenerated" : "failed to write") : "disabled",
                  result.fastfetch_enabled ? (result.fastfetch_synced ? "regenerated" : "failed to write")
                                            : "disabled",
                  apps_synced, result.apps.count);
        add_check(r, all_ok ? REPAIR_FIXED : REPAIR_BROKEN, "terminal + Fastfetch + app config regenerated", detail);
        theme_sync_result_free(&result);
    } else {
        add_check(r, REPAIR_OK, "terminal + Fastfetch + app sync reachable",
                   "run `calm repair` to actively regenerate all of them from the active theme");
    }

    theme_free(&theme);
    calm_document_free(&cfg);
}

static void check_plugin_load(RepairReport *r, bool apply_fixes) {
    if (!apply_fixes) {
        add_check(r, REPAIR_OK, "enabled plugins load reachable",
                   "run `calm repair` to actively regenerate plugins_active.calm");
        return;
    }
    char *warnings = NULL;
    bool ok = plugin_loader_sync(&warnings);
    add_check(r, ok ? REPAIR_FIXED : REPAIR_BROKEN, "enabled plugins load (plugins_active.calm regenerated)",
               warnings ? warnings : (ok ? NULL : "could not write plugins_active.calm"));
    free(warnings);
}

bool repair_scan(bool apply_fixes, RepairReport *report) {
    report->items = NULL;
    report->count = 0;
    report->cap = 0;
    report->backups_made = 0;
    report->backup_path = NULL;

    check_dir(report, apply_fixes, config_dir, "config directory exists");
    char *cdir = config_dir();
    bool have_config_dir = cdir && path_is_dir(cdir);
    free(cdir);
    if (!have_config_dir) {
        return false; /* nothing else can be meaningfully checked */
    }

    check_dir(report, apply_fixes, themes_dir, "themes directory exists");
    check_dir(report, apply_fixes, plugins_dir, "plugins directory exists");
    check_dir(report, apply_fixes, history_dir, "history directory exists");
    check_dir(report, apply_fixes, profiles_dir, "profiles directory exists");

    size_t defaults_count = 0;
    const ScaffoldDefault *defaults = scaffold_defaults(&defaults_count);
    for (size_t i = 0; i < defaults_count; i++) {
        check_topic_file(report, apply_fixes, &defaults[i]);
    }

    check_bundled_theme_present(report, apply_fixes);
    check_themes(report, apply_fixes);
    check_plugins(report);
    check_plugin_load(report, apply_fixes);
    check_permissions(report);
    check_sync(report, apply_fixes);

    return true;
}
