#include "cli/commands/theme_cmd.h"

#include <stdlib.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>

#include "config/calmconf.h"
#include "config/paths.h"
#include "config/scaffold.h"
#include "theme/sync.h"
#include "theme/theme.h"
#include "util/fsutil.h"
#include "util/strutil.h"
#include "util/xalloc.h"

static void print_usage(void) {
    fprintf(stderr,
            "usage: calm theme list\n"
            "       calm theme set <name>\n");
}

static int theme_list(void) {
    char *dir = themes_dir();
    if (!dir) {
        fprintf(stderr, "calm: could not resolve ~/.config/calm-shell/themes\n");
        return 1;
    }
    DIR *d = opendir(dir);
    if (!d) {
        fprintf(stderr, "calm: no themes directory yet -- try running `calm` once first\n");
        free(dir);
        return 1;
    }

    char *marker = active_theme_marker_file();
    char *active = marker ? read_file_to_string(marker) : NULL;
    if (active) {
        char *nl = strchr(active, '\n');
        if (nl) {
            *nl = '\0';
        }
    }
    free(marker);
    const char *active_name = (active && active[0]) ? active : "calm-lavender";

    struct dirent *entry;
    int count = 0;
    while ((entry = readdir(d)) != NULL) {
        if (!ends_with(entry->d_name, ".calm")) {
            continue;
        }
        size_t name_len = strlen(entry->d_name) - strlen(".calm");
        char *name = xstrndup(entry->d_name, name_len);
        printf("%s %s\n", strcmp(name, active_name) == 0 ? "*" : " ", name);
        free(name);
        count++;
    }
    closedir(d);
    free(dir);
    free(active);
    if (count == 0) {
        printf("(no themes found)\n");
    }
    return 0;
}

static int theme_set(const char *name) {
    if (!ensure_scaffold()) {
        fprintf(stderr, "calm: could not write to ~/.config/calm-shell\n");
        return 1;
    }

    Theme t;
    if (!theme_load(name, &t)) {
        fprintf(stderr, "calm: no such theme `%s` (see `calm theme list`)\n", name);
        return 1;
    }
    theme_free(&t);

    char *marker = active_theme_marker_file();
    if (!marker) {
        fprintf(stderr, "calm: could not resolve config directory\n");
        return 1;
    }
    FILE *f = fopen(marker, "w");
    free(marker);
    if (!f) {
        fprintf(stderr, "calm: could not write active theme marker\n");
        return 1;
    }
    fprintf(f, "%s\n", name);
    fclose(f);
    printf("Theme set to `%s`.\n", name);

    /* Propagate the switch immediately -- terminal emulator theme
     * file and Fastfetch config both regenerate right now, not just
     * on the next shell startup. Best-effort: a config that can't be
     * loaded (rare -- ensure_scaffold already succeeded above) just
     * means the CLI reload skips sync, the next `calm` session still
     * picks up the new theme normally. */
    char *cfg_path = config_file();
    CalmDocument cfg;
    char *cfg_err = NULL;
    if (cfg_path && calm_parse_file(cfg_path, &cfg, &cfg_err)) {
        Theme active;
        if (theme_load(name, &active)) {
            ThemeSyncResult result;
            theme_sync_apply(&active, &cfg, &result);
            if (result.terminal_enabled && result.terminal_detail) {
                printf("%s\n", result.terminal_detail);
            }
            if (result.fastfetch_enabled) {
                printf("fastfetch: %s\n", result.fastfetch_synced ? "config synced" : "failed to sync");
            }
            if (result.apps_enabled) {
                int synced_count = 0;
                for (size_t i = 0; i < result.apps.count; i++) {
                    if (result.apps.items[i].synced) {
                        synced_count++;
                    }
                }
                printf("apps: %d/%zu synced (cava, btop, yazi, neovim, helix, hyprland, waybar, wofi, fuzzel -- "
                       "see `calm sync` for per-app detail)\n",
                       synced_count, result.apps.count);
            }
            theme_sync_result_free(&result);
            theme_free(&active);
        }
        calm_document_free(&cfg);
    }
    free(cfg_err);
    free(cfg_path);

    return 0;
}

int theme_cmd_run(int argc, char **argv) {
    if (argc < 3) {
        print_usage();
        return 1;
    }
    if (strcmp(argv[2], "list") == 0) {
        return theme_list();
    }
    if (strcmp(argv[2], "set") == 0) {
        if (argc < 4) {
            print_usage();
            return 1;
        }
        return theme_set(argv[3]);
    }
    print_usage();
    return 1;
}
