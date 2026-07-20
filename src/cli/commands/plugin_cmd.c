/* `calm plugin install|list|search|remove|update|enable|disable` --
 * the plugin manager. Installing fetches a plugin (an official
 * bundled one by name, a local directory, or a git repository, each
 * containing a `plugin.calm` manifest) into
 * ~/.config/calm-shell/plugins/<name>/; enable/disable actually
 * drives whether its aliases/environment/functions take effect (see
 * config/plugin_loader.h) -- installing alone never does, matching
 * "loaded only when necessary". */
#include "cli/commands/plugin_cmd.h"

#include <stdlib.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>

#include "config/calmconf.h"
#include "config/official_plugins.h"
#include "config/paths.h"
#include "config/plugin_loader.h"
#include "config/scaffold.h"
#include "exec/process.h"
#include "util/fsutil.h"
#include "util/strutil.h"
#include "util/xalloc.h"

static void print_usage(void) {
    fprintf(stderr,
            "usage: calm plugin install <name-or-git-url-or-local-dir>\n"
            "       calm plugin search [query]\n"
            "       calm plugin list\n"
            "       calm plugin enable <name>\n"
            "       calm plugin disable <name>\n"
            "       calm plugin remove <name>\n"
            "       calm plugin update [<name>|--all]\n");
}

static bool looks_like_git_url(const char *source) {
    return starts_with(source, "http://") || starts_with(source, "https://") || starts_with(source, "git@") ||
           ends_with(source, ".git");
}

/* Derives the plugin's install directory name from its manifest's
 * `name` field when present, falling back to the last path component
 * of `source` (with a trailing ".git" stripped for URLs). */
static char *derive_plugin_name(const char *source, const char *staged_dir) {
    char *manifest_path = join_path(staged_dir, "plugin.calm");
    CalmDocument manifest;
    char *err = NULL;
    if (calm_parse_file(manifest_path, &manifest, &err)) {
        const char *name = calm_document_get_str(&manifest, "", "name");
        if (name && name[0] != '\0') {
            char *result = xstrdup(name);
            calm_document_free(&manifest);
            free(manifest_path);
            free(err);
            return result;
        }
        calm_document_free(&manifest);
    }
    free(err);
    free(manifest_path);

    const char *base = strrchr(source, '/');
    base = base ? base + 1 : source;
    size_t len = strlen(base);
    if (ends_with(base, ".git")) {
        len -= strlen(".git");
    }
    return xstrndup(base, len);
}

/* Records where an installed plugin came from, so `calm plugin
 * update` later knows how to refresh it without the user having to
 * remember or re-type the original source. Not needed for a git
 * install -- the installed directory already *is* a git checkout, so
 * `git -C <dir> pull` alone is enough -- but a local-directory or
 * official install leaves no such trail on its own. */
static void write_source_marker(const char *dest, const char *marker_value) {
    char *marker_path = join_path(dest, ".calm-shell-source");
    overwrite_file(marker_path, marker_value);
    free(marker_path);
}

static int plugin_install(const char *source) {
    if (!ensure_scaffold()) {
        fprintf(stderr, "calm: could not write to ~/.config/calm-shell\n");
        return 1;
    }
    char *plugins = plugins_dir();
    if (!plugins) {
        fprintf(stderr, "calm: could not resolve plugins directory\n");
        return 1;
    }

    char *staging = join_path(plugins, ".staging");
    char *rm_argv_dummy[] = {(char *)"rm", (char *)"-rf", staging, NULL};
    process_run_foreground(rm_argv_dummy, &(int){0});

    const OfficialPlugin *official =
        (looks_like_git_url(source) || path_is_dir(source)) ? NULL : official_plugin_find(source);
    char *source_marker = NULL;

    int spawn_status = 0;
    bool spawned;
    if (official) {
        spawned = mkdir_recursive(staging);
        if (spawned) {
            char *manifest_path = join_path(staging, "plugin.calm");
            spawned = overwrite_file(manifest_path, official->manifest_fn());
            free(manifest_path);
        }
        source_marker = xsprintf("official:%s", official->name);
    } else if (looks_like_git_url(source)) {
        if (!process_command_exists("git")) {
            fprintf(stderr, "calm: `git` is required to install a plugin from a URL\n");
            free(staging);
            free(plugins);
            return 1;
        }
        char *argv[] = {(char *)"git", (char *)"clone", (char *)"--depth", (char *)"1", (char *)source, staging, NULL};
        spawned = process_run_foreground(argv, &spawn_status);
    } else if (path_is_dir(source)) {
        char *argv[] = {(char *)"cp", (char *)"-r", (char *)source, staging, NULL};
        spawned = process_run_foreground(argv, &spawn_status);
        source_marker = xsprintf("local:%s", source);
    } else {
        fprintf(stderr,
                "calm: `%s` is not an official plugin, a local directory, or a recognizable git URL\n"
                "      run `calm plugin search` to see official plugins\n",
                source);
        free(staging);
        free(plugins);
        return 1;
    }
    if (!spawned || spawn_status != 0) {
        fprintf(stderr, "calm: failed to fetch plugin from `%s`\n", source);
        char *cleanup[] = {(char *)"rm", (char *)"-rf", staging, NULL};
        process_run_foreground(cleanup, &(int){0});
        free(source_marker);
        free(staging);
        free(plugins);
        return 1;
    }

    char *manifest_path = join_path(staging, "plugin.calm");
    if (!path_is_file(manifest_path)) {
        fprintf(stderr, "calm: `%s` has no plugin.calm manifest\n", source);
        free(manifest_path);
        char *cleanup[] = {(char *)"rm", (char *)"-rf", staging, NULL};
        process_run_foreground(cleanup, &(int){0});
        free(source_marker);
        free(staging);
        free(plugins);
        return 1;
    }
    free(manifest_path);

    char *name = derive_plugin_name(source, staging);
    char *dest = join_path(plugins, name);
    char *cleanup_dest[] = {(char *)"rm", (char *)"-rf", dest, NULL};
    process_run_foreground(cleanup_dest, &(int){0});
    char *mv_argv[] = {(char *)"mv", staging, dest, NULL};
    int mv_status = 0;
    bool mv_ok = process_run_foreground(mv_argv, &mv_status) && mv_status == 0;

    if (mv_ok) {
        if (source_marker) {
            write_source_marker(dest, source_marker);
        }
        printf("Installed plugin `%s`. Enable it with `calm plugin enable %s`.\n", name, name);
    } else {
        fprintf(stderr, "calm: could not finalize plugin install at %s\n", dest);
    }

    free(source_marker);
    free(name);
    free(dest);
    free(staging);
    free(plugins);
    return mv_ok ? 0 : 1;
}

static int plugin_list(void) {
    char *dir = plugins_dir();
    if (!dir) {
        fprintf(stderr, "calm: could not resolve plugins directory\n");
        return 1;
    }
    DIR *d = opendir(dir);
    if (!d) {
        printf("(no plugins installed)\n");
        free(dir);
        return 0;
    }

    StrList enabled = plugin_loader_enabled_names();

    struct dirent *entry;
    int count = 0;
    while ((entry = readdir(d)) != NULL) {
        if (entry->d_name[0] == '.') {
            continue;
        }
        char *plugin_dir = join_path(dir, entry->d_name);
        if (!path_is_dir(plugin_dir)) {
            free(plugin_dir);
            continue;
        }

        const char *description = NULL;
        char *manifest_path = join_path(plugin_dir, "plugin.calm");
        CalmDocument manifest;
        bool parsed = calm_parse_file(manifest_path, &manifest, NULL);
        if (parsed) {
            description = calm_document_get_str(&manifest, "", "description");
        }

        bool is_enabled = strlist_contains(&enabled, entry->d_name);
        printf("%s %-16s %s\n", is_enabled ? "[enabled] " : "[disabled]", entry->d_name,
               description ? description : "");

        if (parsed) {
            calm_document_free(&manifest);
        }
        free(manifest_path);
        free(plugin_dir);
        count++;
    }
    closedir(d);
    strlist_free(&enabled);
    free(dir);
    if (count == 0) {
        printf("(no plugins installed)\n");
    }
    return 0;
}

static int plugin_search(const char *query) {
    size_t count = 0;
    const OfficialPlugin *all = official_plugins(&count);
    char *plugins = plugins_dir();
    int shown = 0;
    for (size_t i = 0; i < count; i++) {
        bool matches = !query || query[0] == '\0' || strstr(all[i].name, query) != NULL ||
                       strstr(all[i].description, query) != NULL;
        if (!matches) {
            continue;
        }
        bool installed = false;
        if (plugins) {
            char *plugin_dir_path = join_path(plugins, all[i].name);
            installed = path_is_dir(plugin_dir_path);
            free(plugin_dir_path);
        }
        printf("%-12s %s%s\n", all[i].name, all[i].description, installed ? "  (installed)" : "");
        shown++;
    }
    free(plugins);
    if (shown == 0) {
        printf("(no official plugins match `%s`)\n", query ? query : "");
    } else {
        printf("\nInstall one with `calm plugin install <name>`.\n");
    }
    return 0;
}

static int plugin_enable(const char *name) {
    if (!ensure_scaffold()) {
        fprintf(stderr, "calm: could not write to ~/.config/calm-shell\n");
        return 1;
    }
    bool ok = plugin_loader_enable(name);
    if (!ok) {
        fprintf(stderr, "calm: `%s` isn't installed (see `calm plugin list`)\n", name);
        return 1;
    }
    printf("Enabled plugin `%s`.\n", name);
    return 0;
}

static int plugin_disable(const char *name) {
    if (!plugin_loader_is_enabled(name)) {
        printf("`%s` is already disabled.\n", name);
        return 0;
    }
    bool ok = plugin_loader_disable(name);
    if (!ok) {
        fprintf(stderr, "calm: could not disable `%s`\n", name);
        return 1;
    }
    printf("Disabled plugin `%s`.\n", name);
    return 0;
}

static int plugin_remove(const char *name) {
    char *plugins = plugins_dir();
    char *plugin_dir = plugins ? join_path(plugins, name) : NULL;
    free(plugins);
    if (!plugin_dir || !path_is_dir(plugin_dir)) {
        fprintf(stderr, "calm: `%s` isn't installed\n", name);
        free(plugin_dir);
        return 1;
    }

    bool was_enabled = plugin_loader_is_enabled(name);
    char *argv[] = {(char *)"rm", (char *)"-rf", plugin_dir, NULL};
    int status = 0;
    bool ok = process_run_foreground(argv, &status) && status == 0;
    free(plugin_dir);

    if (!ok) {
        fprintf(stderr, "calm: failed to remove `%s`\n", name);
        return 1;
    }
    if (was_enabled) {
        plugin_loader_disable(name);
    }
    printf("Removed plugin `%s`.\n", name);
    return 0;
}

static char *plugin_update_one(const char *name) {
    char *plugins = plugins_dir();
    char *plugin_dir = plugins ? join_path(plugins, name) : NULL;
    free(plugins);
    if (!plugin_dir || !path_is_dir(plugin_dir)) {
        free(plugin_dir);
        return xsprintf("%s: not installed", name);
    }

    char *git_dir = join_path(plugin_dir, ".git");
    char *marker_path = join_path(plugin_dir, ".calm-shell-source");
    char *marker = read_file_to_string(marker_path);
    if (marker) {
        char *nl = strchr(marker, '\n');
        if (nl) {
            *nl = '\0';
        }
    }

    char *result;
    if (path_is_dir(git_dir)) {
        char *argv[] = {(char *)"git", (char *)"-C", plugin_dir, (char *)"pull", (char *)"--ff-only", NULL};
        int status = 0;
        bool ok = process_command_exists("git") && process_run_foreground(argv, &status) && status == 0;
        result = xsprintf("%s: %s", name, ok ? "updated from its git remote" : "git pull failed");
    } else if (marker && starts_with(marker, "official:")) {
        const OfficialPlugin *official = official_plugin_find(marker + strlen("official:"));
        if (official) {
            char *manifest_path = join_path(plugin_dir, "plugin.calm");
            overwrite_file(manifest_path, official->manifest_fn());
            free(manifest_path);
            result = xsprintf("%s: already up to date (bundled with this calm-shell build)", name);
        } else {
            result = xsprintf("%s: recorded as an official plugin calm-shell no longer bundles", name);
        }
    } else if (marker && starts_with(marker, "local:")) {
        const char *original_path = marker + strlen("local:");
        if (!path_is_dir(original_path)) {
            result = xsprintf("%s: original source `%s` no longer exists", name, original_path);
        } else {
            char *staging = xsprintf("%s.update-staging", plugin_dir);
            char *rm_argv[] = {(char *)"rm", (char *)"-rf", staging, NULL};
            process_run_foreground(rm_argv, &(int){0});
            char *cp_argv[] = {(char *)"cp", (char *)"-r", (char *)original_path, staging, NULL};
            int status = 0;
            bool copied = process_run_foreground(cp_argv, &status) && status == 0;
            bool swapped = false;
            if (copied) {
                write_source_marker(staging, marker);
                char *rm_dest[] = {(char *)"rm", (char *)"-rf", plugin_dir, NULL};
                process_run_foreground(rm_dest, &(int){0});
                char *mv_argv[] = {(char *)"mv", staging, plugin_dir, NULL};
                int mv_status = 0;
                swapped = process_run_foreground(mv_argv, &mv_status) && mv_status == 0;
            }
            result =
                xsprintf("%s: %s", name, swapped ? "re-synced from its original local source" : "update failed");
            free(staging);
        }
    } else {
        result = xsprintf("%s: no recorded source -- reinstall with `calm plugin install` to enable updates", name);
    }

    free(marker);
    free(marker_path);
    free(git_dir);
    free(plugin_dir);
    return result;
}

static int plugin_update(const char *target) {
    char *dir = plugins_dir();
    if (!dir) {
        fprintf(stderr, "calm: could not resolve plugins directory\n");
        return 1;
    }

    if (target && strcmp(target, "--all") != 0) {
        char *status = plugin_update_one(target);
        printf("%s\n", status);
        free(status);
        free(dir);
        char *warnings = NULL;
        plugin_loader_sync(&warnings);
        if (warnings) {
            fputs(warnings, stdout);
        }
        free(warnings);
        return 0;
    }

    DIR *d = opendir(dir);
    if (!d) {
        printf("(no plugins installed)\n");
        free(dir);
        return 0;
    }
    struct dirent *entry;
    int count = 0;
    while ((entry = readdir(d)) != NULL) {
        if (entry->d_name[0] == '.') {
            continue;
        }
        char *plugin_dir = join_path(dir, entry->d_name);
        if (path_is_dir(plugin_dir)) {
            char *status = plugin_update_one(entry->d_name);
            printf("%s\n", status);
            free(status);
            count++;
        }
        free(plugin_dir);
    }
    closedir(d);
    free(dir);
    if (count == 0) {
        printf("(no plugins installed)\n");
    }
    char *warnings = NULL;
    plugin_loader_sync(&warnings);
    if (warnings) {
        fputs(warnings, stdout);
    }
    free(warnings);
    return 0;
}

int plugin_cmd_run(int argc, char **argv) {
    if (argc < 3) {
        print_usage();
        return 1;
    }
    if (strcmp(argv[2], "install") == 0) {
        if (argc < 4) {
            print_usage();
            return 1;
        }
        return plugin_install(argv[3]);
    }
    if (strcmp(argv[2], "list") == 0) {
        return plugin_list();
    }
    if (strcmp(argv[2], "search") == 0) {
        return plugin_search(argc >= 4 ? argv[3] : NULL);
    }
    if (strcmp(argv[2], "enable") == 0) {
        if (argc < 4) {
            print_usage();
            return 1;
        }
        return plugin_enable(argv[3]);
    }
    if (strcmp(argv[2], "disable") == 0) {
        if (argc < 4) {
            print_usage();
            return 1;
        }
        return plugin_disable(argv[3]);
    }
    if (strcmp(argv[2], "remove") == 0 || strcmp(argv[2], "uninstall") == 0) {
        if (argc < 4) {
            print_usage();
            return 1;
        }
        return plugin_remove(argv[3]);
    }
    if (strcmp(argv[2], "update") == 0) {
        return plugin_update(argc >= 4 ? argv[3] : NULL);
    }
    print_usage();
    return 1;
}
