#include "config/scaffold.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "config/paths.h"
#include "util/fsutil.h"

static bool write_file_if_missing(const char *path, const char *contents) {
    if (path_exists(path)) {
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
        "# Full format guide: docs/CONFIGURATION.md in the calm-shell repo.\n"
        "#\n"
        "# This file is the entry point only -- each concern lives in its own\n"
        "# topic file (environment, directory, history, keyboard, terminal,\n"
        "# aliases, functions), included below.\n"
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
        "# How many entries pushd/popd/dirs remember; oldest is dropped first.\n"
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
        "# Max entries kept in memory and in the history file; oldest entries\n"
        "# are dropped first once the count is exceeded.\n"
        "size = 10000\n"
        "# A command identical to the immediately preceding one is not added\n"
        "# again -- keeps repeated `ls`/`git status` runs from cluttering\n"
        "# Ctrl+R search and Up-arrow recall.\n"
        "ignore_dups = true\n"
        "# Not wired yet -- history is per-session (one file per running\n"
        "# calm, not merged live across concurrently open sessions).\n"
        "share_across_sessions = true\n";
}

static const char *default_keyboard_calm(void) {
    return
        "# Keybindings\n"
        "\n"
        "[keyboard]\n"
        "# \"emacs\" or \"vi\" -- switches the line editor's editing mode.\n"
        "edit_mode = \"emacs\"\n"
        "\n"
        "# Not wired yet -- these four happen to already match the line\n"
        "# editor's built-in emacs defaults, so they're accurate as\n"
        "# documentation, but changing a value here won't remap anything.\n"
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
        "# Rings the terminal bell on calm's own errors (bad cd/pushd/popd\n"
        "# target, a command that couldn't be spawned at all) -- not on every\n"
        "# failing command's nonzero exit status.\n"
        "bell = false\n"
        "# Whether the prompt/editor emit 24-bit ANSI color at all. Also\n"
        "# disabled automatically when $NO_COLOR is set, per no-color.org.\n"
        "truecolor = true\n"
        "\n"
        "# Non-color appearance, shared across every terminal emulator `calm\n"
        "# sync` writes a theme file for -- font, cursor, transparency,\n"
        "# padding, and window decorations. Colors themselves come from the\n"
        "# active theme's own [terminal] section, not from here.\n"
        "[appearance]\n"
        "font_family        = \"monospace\"\n"
        "font_size          = 12\n"
        "# \"beam\", \"block\", or \"underline\".\n"
        "cursor_style       = \"beam\"\n"
        "cursor_blink       = true\n"
        "# 1.0 = fully opaque, down to 0.0 = fully transparent.\n"
        "opacity            = 1.0\n"
        "# Uniform cell padding in pixels.\n"
        "padding            = 8\n"
        "window_decorations = true\n"
        "\n"
        "# Controls `calm sync` -- detecting the host terminal emulator and\n"
        "# regenerating its theme file, and regenerating Fastfetch's config,\n"
        "# every time the active theme changes and once per new session.\n"
        "[sync]\n"
        "sync_terminal        = true\n"
        "sync_fastfetch       = true\n"
        "run_fastfetch_on_start = true\n"
        "# Force detection to a specific emulator (\"kitty\", \"alacritty\",\n"
        "# \"wezterm\", \"foot\", \"ghostty\", \"konsole\", \"gnome-terminal\") --\n"
        "# useful under tmux/ssh, where the real host emulator's environment\n"
        "# variables don't survive the hop. Empty string means auto-detect.\n"
        "terminal_override    = \"\"\n";
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
        "# Calm Lavender -- the bundled default theme.\n"
        "name = \"calm-lavender\"\n"
        "display_name = \"Calm Lavender\"\n"
        "\n"
        "[colors]\n"
        "calm_purple  = \"#CBA6F7\"\n"
        "soft_blue    = \"#89B4FA\"\n"
        "gentle_pink  = \"#F5C2E7\"\n"
        "mint_green   = \"#A6E3A1\"\n"
        "warm_yellow  = \"#F9E2AF\"\n"
        "soft_red     = \"#F38BA8\"\n"
        "cloud_gray   = \"#BAC2DE\"\n"
        "\n"
        "[prompt]\n"
        "icon             = \"\xF0\x9F\x8C\x99\"\n"
        "border_color     = \"cloud_gray\"\n"
        "path_color       = \"soft_blue\"\n"
        "git_clean_color  = \"mint_green\"\n"
        "git_dirty_color  = \"soft_red\"\n"
        "arrow_color      = \"calm_purple\"\n"
        "\n"
        "# Whole-terminal colors -- picked up by `calm sync` for every synced\n"
        "# terminal emulator's background/foreground/cursor, and by Fastfetch's\n"
        "# generated color scheme. A palette key or a literal \"#RRGGBB\" both work.\n"
        "[terminal]\n"
        "background = \"#1B1823\"\n"
        "foreground = \"#E5E1F0\"\n"
        "cursor     = \"calm_purple\"\n";
}

bool ensure_scaffold(void) {
    char *dirs[4] = {config_dir(), themes_dir(), plugins_dir(), history_dir()};
    bool ok = true;
    for (int i = 0; i < 4; i++) {
        if (!dirs[i] || !mkdir_recursive(dirs[i])) {
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

        char *themes = themes_dir();
        if (themes) {
            char *lavender_path = join_path(themes, "calm-lavender.calm");
            write_file_if_missing(lavender_path, default_lavender_theme());
            free(lavender_path);
            free(themes);
        }
    }
    for (int i = 0; i < 4; i++) {
        free(dirs[i]);
    }
    return ok;
}
