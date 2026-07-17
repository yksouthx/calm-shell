#include "theme_cmd.h"

#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "../config.h"
#include "../json.h"
#include "../util.h"

static char *active_theme_marker_path(void) {
    char *dir = config_dir();
    if (!dir) {
        return NULL;
    }
    char *path = join_path(dir, ".active_theme");
    free(dir);
    return path;
}

char *current_theme(void) {
    char *marker = active_theme_marker_path();
    if (marker) {
        char *contents = read_file_to_string(marker);
        free(marker);
        if (contents) {
            trim_in_place(contents);
            return contents;
        }
    }
    return xstrdup("calm-lavender");
}

/* Strips a trailing ".json" from `filename`, e.g. for the file-stem
 * fallback when a theme's JSON doesn't set its own "name" field. */
static char *strip_json_ext(const char *filename) {
    size_t len = strlen(filename);
    if (ends_with(filename, ".json")) {
        return xstrndup(filename, len - 5);
    }
    return xstrdup(filename);
}

int theme_cmd_list(void) {
    if (!ensure_scaffold()) {
        fprintf(stderr, "error: could not set up config directory\n");
        return 1;
    }
    char *dir = themes_dir();
    if (!dir) {
        fprintf(stderr, "error: could not resolve themes directory\n");
        return 1;
    }
    char *active = current_theme();

    DIR *d = opendir(dir);
    if (!d) {
        fprintf(stderr, "error: reading %s: %s\n", dir, strerror(errno));
        free(dir);
        free(active);
        return 1;
    }

    bool found = false;
    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (!ends_with(entry->d_name, ".json")) {
            continue;
        }
        found = true;

        char *path = join_path(dir, entry->d_name);
        char *raw = read_file_to_string(path);
        free(path);

        char *fallback_name = strip_json_ext(entry->d_name);
        char *name = xstrdup(fallback_name);
        char *display = xstrdup(fallback_name);

        if (raw) {
            JsonValue *root = json_parse(raw, NULL);
            if (root) {
                const char *n = json_as_string(json_object_get(root, "name"));
                const char *disp = json_as_string(json_object_get(root, "display_name"));
                if (n) {
                    free(name);
                    name = xstrdup(n);
                }
                if (disp) {
                    free(display);
                    display = xstrdup(disp);
                } else {
                    free(display);
                    display = xstrdup(name);
                }
                json_free(root);
            }
            free(raw);
        }
        free(fallback_name);

        if (strcmp(name, active) == 0) {
            printf("  \x1b[38;2;203;166;247m\xE2\x97\x8F\x1b[0m \x1b[1m%s\x1b[0m  (%s)\n", display, name);
        } else {
            printf("  \x1b[2m\xE2\x97\x8B %s\x1b[0m  (%s)\n", display, name);
        }
        free(name);
        free(display);
    }
    closedir(d);

    if (!found) {
        printf("\x1b[38;2;243;139;168mNo themes installed.\x1b[0m\n");
    }

    free(dir);
    free(active);
    return 0;
}

/* Rewrites config.calm's `theme = "..."` line under [shell] in place,
 * or appends a [shell] section with it if one doesn't exist yet. */
static bool sync_config_theme(const char *name) {
    char *path = config_file();
    if (!path) {
        return false;
    }
    char *existing = read_file_to_string(path);
    if (!existing) {
        existing = xstrdup("");
    }

    size_t line_count = 0;
    char **lines = split_lines(existing, &line_count);
    free(existing);

    StrBuilder out;
    sb_init(&out);
    bool found = false;
    bool has_shell_section = false;
    for (size_t i = 0; i < line_count; i++) {
        char *trimmed = xstrdup(lines[i]);
        trim_in_place(trimmed);
        if (strcmp(trimmed, "[shell]") == 0) {
            has_shell_section = true;
        }
        if (!found && starts_with(trimmed, "theme") && strchr(trimmed, '=') != NULL) {
            found = true;
            char *replacement = xsprintf("theme = \"%s\"", name);
            sb_append(&out, replacement);
            free(replacement);
        } else {
            sb_append(&out, lines[i]);
        }
        sb_append(&out, "\n");
        free(trimmed);
    }
    free_lines(lines, line_count);

    if (!found) {
        if (!has_shell_section) {
            sb_append(&out, "[shell]\n");
        }
        char *line = xsprintf("theme = \"%s\"\n", name);
        sb_append(&out, line);
        free(line);
    }

    char *final_contents = sb_take(&out);
    FILE *f = fopen(path, "w");
    bool ok = f != NULL;
    if (f) {
        size_t len = strlen(final_contents);
        ok = fwrite(final_contents, 1, len, f) == len;
        fclose(f);
    }
    free(final_contents);
    free(path);
    return ok;
}

int theme_cmd_set(const char *name) {
    if (!ensure_scaffold()) {
        fprintf(stderr, "error: could not set up config directory\n");
        return 1;
    }

    char *dir = themes_dir();
    char *filename = xsprintf("%s.json", name);
    char *theme_file = join_path(dir, filename);
    free(filename);

    struct stat st;
    if (stat(theme_file, &st) != 0) {
        fprintf(stderr, "error: theme '%s' not found in %s. Run `calm theme list` to see installed themes.\n", name,
                dir);
        free(dir);
        free(theme_file);
        return 1;
    }
    free(theme_file);
    free(dir);

    char *marker = active_theme_marker_path();
    if (marker) {
        FILE *f = fopen(marker, "w");
        if (f) {
            fputs(name, f);
            fclose(f);
        }
        free(marker);
    }

    sync_config_theme(name);

    printf("\x1b[38;2;166;227;161m\xE2\x9C\x93\x1b[0m Theme set to \x1b[38;2;203;166;247m\x1b[1m%s\x1b[0m\n", name);
    return 0;
}
