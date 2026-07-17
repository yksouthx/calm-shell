#include "config.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "util.h"

char *config_dir(void) {
    char *base = xdg_config_dir();
    if (!base) {
        return NULL;
    }
    char *out = join_path(base, "calm-shell");
    free(base);
    return out;
}

static char *config_dir_join(const char *name) {
    char *dir = config_dir();
    if (!dir) {
        return NULL;
    }
    char *out = join_path(dir, name);
    free(dir);
    return out;
}

char *themes_dir(void) {
    return config_dir_join("themes");
}
char *plugins_dir(void) {
    return config_dir_join("plugins");
}
char *history_dir(void) {
    return config_dir_join("history");
}
char *config_file(void) {
    return config_dir_join("config.calm");
}
char *aliases_file(void) {
    return config_dir_join("aliases.calm");
}
char *functions_file(void) {
    return config_dir_join("functions.calm");
}
char *environment_file(void) {
    return config_dir_join("environment.calm");
}
char *directory_file(void) {
    return config_dir_join("directory.calm");
}
char *history_settings_file(void) {
    return config_dir_join("history.calm");
}
char *keyboard_file(void) {
    return config_dir_join("keyboard.calm");
}
char *terminal_file(void) {
    return config_dir_join("terminal.calm");
}

static bool mkdir_p(const char *path) {
    if (mkdir(path, 0755) == 0 || errno == EEXIST) {
        return true;
    }
    if (errno != ENOENT) {
        return false;
    }
    char *parent = dirname_of(path);
    bool ok = strcmp(parent, path) != 0 && mkdir_p(parent);
    free(parent);
    if (!ok) {
        return false;
    }
    return mkdir(path, 0755) == 0 || errno == EEXIST;
}

static bool write_file_if_missing(const char *path, const char *contents) {
    struct stat st;
    if (stat(path, &st) == 0) {
        return true; /* already exists, leave user edits alone */
    }
    FILE *f = fopen(path, "w");
    if (!f) {
        return false;
    }
    size_t len = strlen(contents);
    bool ok = fwrite(contents, 1, len, f) == len;
    fclose(f);
    return ok;
}

static const char *default_config_calm(void) {
    return
        "# Calm-shell configuration\n"
        "# See: https://github.com/s0uth09/calm-shell for the full .calm format guide\n"
        "#\n"
        "# This file is the entry point only -- each concern lives in its own\n"
        "# topic file (environment, directory, history, keyboard, terminal,\n"
        "# aliases, functions), included below. Layout inspired by the classic\n"
        "# oh-my-zsh split: github.com/mattjj/my-oh-my-zsh\n"
        "\n"
        "[shell]\n"
        "theme = \"calm-lavender\"\n"
        "greeting = true\n"
        "\n"
        "[prompt]\n"
        "show_git = true\n"
        "icon = \"\xF0\x9F\x8C\x99\"\n"
        "\n"
        "include \"environment.calm\"\n"
        "include \"directory.calm\"\n"
        "include \"history.calm\"\n"
        "include \"keyboard.calm\"\n"
        "include \"terminal.calm\"\n"
        "include \"aliases.calm\"\n"
        "include \"functions.calm\"\n";
}

static const char *default_environment_calm(void) {
    return
        "# Environment variables Calm-shell exports on launch\n"
        "\n"
        "[environment]\n"
        "EDITOR = \"nvim\"\n"
        "PAGER = \"less\"\n"
        "LANG = \"en_US.UTF-8\"\n";
}

static const char *default_directory_calm(void) {
    return
        "# Directory navigation behavior and shortcuts\n"
        "\n"
        "[directory]\n"
        "# Typing a bare directory name (no spaces, no other command) cds into it.\n"
        "auto_cd = true\n"
        "# How many entries `pushd`/`popd`/`dirs` remember; oldest is dropped first.\n"
        "dir_stack_size = 20\n"
        "\n"
        "[directory.aliases]\n"
        "\"..\" = \"cd ..\"\n"
        "\"...\" = \"cd ../..\"\n"
        "\"....\" = \"cd ../../..\"\n"
        "\"-\" = \"cd -\"\n";
}

static const char *default_history_calm(void) {
    return
        "# Shell history behavior\n"
        "\n"
        "[history]\n"
        "size = 10000\n"
        "# `save` also feeds history capacity (the larger of the two wins); kept\n"
        "# distinct from `size` for familiarity if you're used to zsh's HISTSIZE\n"
        "# vs SAVEHIST split.\n"
        "save = 10000\n"
        "# Not wired yet -- every entry is currently kept, duplicates included.\n"
        "ignore_dups = true\n"
        "# Not wired yet -- history is per-session (one file, but not merged live\n"
        "# across concurrently open calm sessions).\n"
        "share_across_sessions = true\n";
}

static const char *default_keyboard_calm(void) {
    return
        "# Keybindings\n"
        "\n"
        "[keyboard]\n"
        "# \"emacs\" or \"vi\" -- this one is real and does switch the line editor's mode.\n"
        "edit_mode = \"emacs\"\n"
        "\n"
        "# Not wired yet -- these four happen to already match the line editor's\n"
        "# built-in emacs defaults, so they're accurate as documentation, but\n"
        "# changing a value here won't actually remap anything yet.\n"
        "[keyboard.bindings]\n"
        "\"ctrl+r\" = \"history-search-backward\"\n"
        "\"ctrl+a\" = \"beginning-of-line\"\n"
        "\"ctrl+e\" = \"end-of-line\"\n"
        "\"alt+backspace\" = \"backward-kill-word\"\n";
}

static const char *default_terminal_calm(void) {
    return
        "# Terminal integration behavior\n"
        "\n"
        "[terminal]\n"
        "# Sets the terminal tab/window title to \"calm -- <cwd>\" each prompt.\n"
        "set_title = true\n"
        "# Rings the terminal bell on calm's own errors (bad cd/pushd/popd target,\n"
        "# a command that couldn't be spawned at all) -- not on every failing\n"
        "# command's nonzero exit status.\n"
        "bell = false\n"
        "# Not wired yet -- `calm doctor` checks $COLORTERM independently, but\n"
        "# nothing currently degrades the palette for terminals without it.\n"
        "truecolor = true\n";
}

static const char *default_aliases_calm(void) {
    return
        "# Calm-shell aliases -- key = command\n"
        "\n"
        "[aliases]\n"
        "ll = \"ls -la\"\n"
        "gs = \"git status\"\n"
        "gc = \"git commit\"\n"
        "gp = \"git push\"\n";
}

static const char *default_functions_calm(void) {
    return
        "# Calm-shell functions -- triple-quoted blocks hold raw shell bodies\n"
        "\n"
        "[functions]\n"
        "mkcd = \"\"\"\n"
        "mkdir -p \"$1\" && cd \"$1\"\n"
        "\"\"\"\n";
}

const char *default_lavender_theme(void) {
    return
        "{\n"
        "  \"name\": \"calm-lavender\",\n"
        "  \"display_name\": \"Calm Lavender\",\n"
        "  \"colors\": {\n"
        "    \"calm_purple\": \"#CBA6F7\",\n"
        "    \"soft_blue\": \"#89B4FA\",\n"
        "    \"gentle_pink\": \"#F5C2E7\",\n"
        "    \"mint_green\": \"#A6E3A1\",\n"
        "    \"warm_yellow\": \"#F9E2AF\",\n"
        "    \"soft_red\": \"#F38BA8\",\n"
        "    \"cloud_gray\": \"#BAC2DE\"\n"
        "  },\n"
        "  \"prompt\": {\n"
        "    \"icon\": \"\xF0\x9F\x8C\x99\",\n"
        "    \"border_color\": \"cloud_gray\",\n"
        "    \"path_color\": \"soft_blue\",\n"
        "    \"git_clean_color\": \"mint_green\",\n"
        "    \"git_dirty_color\": \"soft_red\",\n"
        "    \"arrow_color\": \"calm_purple\"\n"
        "  }\n"
        "}\n";
}

bool ensure_scaffold(void) {
    char *dirs[4] = {config_dir(), themes_dir(), plugins_dir(), history_dir()};
    bool ok = true;
    for (int i = 0; i < 4; i++) {
        if (!dirs[i] || !mkdir_p(dirs[i])) {
            ok = false;
        }
    }
    if (ok) {
        struct {
            char *(*path_fn)(void);
            const char *(*content_fn)(void);
        } defaults[8] = {
            {config_file, default_config_calm},
            {environment_file, default_environment_calm},
            {directory_file, default_directory_calm},
            {history_settings_file, default_history_calm},
            {keyboard_file, default_keyboard_calm},
            {terminal_file, default_terminal_calm},
            {aliases_file, default_aliases_calm},
            {functions_file, default_functions_calm},
        };
        for (int i = 0; i < 8 && ok; i++) {
            char *path = defaults[i].path_fn();
            if (!path || !write_file_if_missing(path, defaults[i].content_fn())) {
                ok = false;
            }
            free(path);
        }

        char *lavender = themes_dir();
        if (lavender) {
            char *lavender_json = join_path(lavender, "calm-lavender.json");
            write_file_if_missing(lavender_json, default_lavender_theme());
            free(lavender_json);
            free(lavender);
        }
    }
    for (int i = 0; i < 4; i++) {
        free(dirs[i]);
    }
    return ok;
}
