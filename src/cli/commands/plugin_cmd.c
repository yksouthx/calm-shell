#include "cli/commands/plugin_cmd.h"

#include <stdlib.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>

#include "config/calmconf.h"
#include "config/paths.h"
#include "config/scaffold.h"
#include "exec/process.h"
#include "util/fsutil.h"
#include "util/strutil.h"
#include "util/xalloc.h"

static void print_usage(void) {
    fprintf(stderr,
            "usage: calm plugin install <local-directory-or-git-url>\n"
            "       calm plugin list\n");
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

    /* Stage into a temporary working directory first, so a manifest
     * read failure or an interrupted git clone never leaves a
     * half-installed plugin sitting under the final name. */
    char *staging = join_path(plugins, ".staging");
    char *rm_argv_dummy[] = {(char *)"rm", (char *)"-rf", staging, NULL};
    process_run_foreground(rm_argv_dummy, &(int){0});

    int spawn_status = 0;
    bool spawned;
    if (looks_like_git_url(source)) {
        if (!process_command_exists("git")) {
            fprintf(stderr, "calm: `git` is required to install a plugin from a URL\n");
            free(staging);
            free(plugins);
            return 1;
        }
        char *argv[] = {(char *)"git", (char *)"clone", (char *)"--depth", (char *)"1", (char *)source, staging, NULL};
        spawned = process_run_foreground(argv, &spawn_status);
    } else {
        if (!path_is_dir(source)) {
            fprintf(stderr, "calm: `%s` is not a local directory or a recognizable git URL\n", source);
            free(staging);
            free(plugins);
            return 1;
        }
        char *argv[] = {(char *)"cp", (char *)"-r", (char *)source, staging, NULL};
        spawned = process_run_foreground(argv, &spawn_status);
    }
    if (!spawned || spawn_status != 0) {
        fprintf(stderr, "calm: failed to fetch plugin from `%s`\n", source);
        char *cleanup[] = {(char *)"rm", (char *)"-rf", staging, NULL};
        process_run_foreground(cleanup, &(int){0});
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
        printf("Installed plugin `%s`.\n", name);
    } else {
        fprintf(stderr, "calm: could not finalize plugin install at %s\n", dest);
    }

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
    struct dirent *entry;
    int count = 0;
    while ((entry = readdir(d)) != NULL) {
        if (entry->d_name[0] == '.') {
            continue;
        }
        char *plugin_dir = join_path(dir, entry->d_name);
        if (path_is_dir(plugin_dir)) {
            printf("%s\n", entry->d_name);
            count++;
        }
        free(plugin_dir);
    }
    closedir(d);
    free(dir);
    if (count == 0) {
        printf("(no plugins installed)\n");
    }
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
    print_usage();
    return 1;
}
