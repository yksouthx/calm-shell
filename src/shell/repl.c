#include "shell/repl.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config/calmconf.h"
#include "config/paths.h"
#include "config/scaffold.h"
#include "exec/process.h"
#include "shell/alias_table.h"
#include "shell/builtins.h"
#include "shell/execute.h"
#include "term/history.h"
#include "term/known_commands.h"
#include "term/line_editor.h"
#include "term/prompt.h"
#include "theme/fastfetch_sync.h"
#include "theme/sync.h"
#include "theme/theme.h"
#include "util/fsutil.h"
#include "util/strutil.h"
#include "util/xalloc.h"
#include "version.h"

static void apply_environment_section(const CalmDocument *cfg) {
    const CalmSection *env = calm_document_section(cfg, "environment");
    if (!env) {
        return;
    }
    for (size_t i = 0; i < env->count; i++) {
        if (env->entries[i].value.type != CALM_STRING) {
            continue;
        }
        char *expanded = calm_expand(env->entries[i].value.as.string);
        setenv(env->entries[i].key, expanded, 1);
        free(expanded);
    }
}

static void print_greeting(const Theme *theme) {
    char *icon = theme_paint(theme, "arrow", theme->icon);
    char *name = theme_paint(theme, "calm_purple", theme->display_name);
    printf("%s Calm-shell %s -- theme: %s\n", icon, CALM_VERSION, name);
    free(icon);
    free(name);
}

static void set_terminal_title(const char *display_cwd) {
    printf("\x1b]0;calm -- %s\x07", display_cwd);
    fflush(stdout);
}

/* Chooses the prompt icon: the main config's own `[prompt] icon`
 * overrides the active theme's default, so switching themes doesn't
 * silently discard an icon the user picked deliberately. */
static const char *resolve_prompt_icon(const CalmDocument *cfg, const Theme *theme) {
    const char *icon = calm_document_get_str(cfg, "prompt", "icon");
    return icon ? icon : theme->icon;
}

int repl_run(void) {
    if (!ensure_scaffold()) {
        fprintf(stderr, "calm: could not create ~/.config/calm-shell -- check permissions\n");
        return 1;
    }

    char *cfg_path = config_file();
    CalmDocument cfg;
    char *err = NULL;
    if (!cfg_path || !calm_parse_file(cfg_path, &cfg, &err)) {
        fprintf(stderr, "calm: failed to load configuration: %s\n", err ? err : "unknown error");
        free(err);
        free(cfg_path);
        return 1;
    }
    free(cfg_path);

    Theme theme;
    if (!theme_load_active(&theme)) {
        fprintf(stderr, "calm: no usable theme found (not even the bundled calm-lavender)\n");
        calm_document_free(&cfg);
        return 1;
    }
    bool truecolor = calm_document_get_bool(&cfg, "terminal", "truecolor", true);
    theme.colors_enabled = truecolor && getenv("NO_COLOR") == NULL;

    apply_environment_section(&cfg);

    bool auto_cd = calm_document_get_bool(&cfg, "directory", "auto_cd", true);
    long long dir_stack_size = calm_document_get_int(&cfg, "directory", "dir_stack_size", 20);
    bool set_title = calm_document_get_bool(&cfg, "terminal", "set_title", true);
    bool bell = calm_document_get_bool(&cfg, "terminal", "bell", false);
    bool vi_mode = strcmp(calm_document_get_str(&cfg, "keyboard", "edit_mode")
                               ? calm_document_get_str(&cfg, "keyboard", "edit_mode")
                               : "emacs",
                           "vi") == 0;
    bool greeting = calm_document_get_bool(&cfg, "shell", "greeting", true);

    long long history_size = calm_document_get_int(&cfg, "history", "size", 10000);
    bool ignore_dups = calm_document_get_bool(&cfg, "history", "ignore_dups", true);

    ShellNavState nav;
    shell_nav_init(&nav, dir_stack_size > 0 ? (size_t)dir_stack_size : 0);

    AliasTable aliases = alias_table_load(&cfg);
    StrList known = known_commands_build(&cfg);
    char *prelude = execute_build_prelude(&cfg);

    char *hdir = history_dir();
    char *hist_path = hdir ? join_path(hdir, "history") : NULL;
    free(hdir);
    History hist;
    history_load(hist_path, history_size > 0 ? (size_t)history_size : 0, ignore_dups, &hist);
    free(hist_path);

    if (greeting) {
        print_greeting(&theme);
    }

    /* Keeps the host terminal emulator and Fastfetch's own display in
     * lockstep with whatever theme is active, every session -- not
     * just right after `theme set`, since the terminal emulator may
     * have been reconfigured, or this may be the first session since
     * install. Both steps are cheap (a handful of small file writes)
     * and `fastfetch_run` itself is what actually takes any
     * measurable time, so it -- and only it -- is gated behind its
     * own opt-out for anyone who wants startup to stay silent. */
    ThemeSyncResult sync_result;
    theme_sync_apply(&theme, &cfg, &sync_result);
    bool run_fastfetch = calm_document_get_bool(&cfg, "sync", "run_fastfetch_on_start", true);
    if (run_fastfetch && sync_result.fastfetch_enabled && sync_result.fastfetch_synced && sync_result.fastfetch_path) {
        fastfetch_run(sync_result.fastfetch_path);
    }
    theme_sync_result_free(&sync_result);

    const char *icon = resolve_prompt_icon(&cfg, &theme);
    int exit_status = 0;
    bool running = true;

    while (running) {
        if (set_title) {
            char *display = display_path();
            set_terminal_title(display);
            free(display);
        }
        render_prompt_box(&theme, icon);

        char *line = NULL;
        LineSignal signal = line_editor_read_line(&theme, &known, &hist, vi_mode, &line);

        if (signal == LINE_CTRLD) {
            running = false;
            free(line);
            continue;
        }
        if (signal == LINE_CTRLC || signal == LINE_ERROR) {
            free(line);
            continue;
        }

        char *trimmed = xstrdup(line);
        free(line);
        trim_in_place(trimmed);
        if (trimmed[0] == '\0') {
            free(trimmed);
            continue;
        }

        char *expanded = alias_table_expand(&aliases, trimmed);
        free(trimmed);

        bool dispatched_as_cd_target = false;
        if (!execute_needs_full_shell(expanded)) {
            char first_word[16] = {0};
            size_t fw = 0;
            while (expanded[fw] && !isspace((unsigned char)expanded[fw]) && fw < sizeof(first_word) - 1) {
                first_word[fw] = expanded[fw];
                fw++;
            }
            bool looks_like_builtin = strcmp(first_word, "cd") == 0 || strcmp(first_word, "pushd") == 0 ||
                                       strcmp(first_word, "popd") == 0 || strcmp(first_word, "dirs") == 0 ||
                                       strcmp(first_word, "exit") == 0 || strcmp(first_word, "quit") == 0;

            if (looks_like_builtin) {
                BuiltinOutcome outcome = builtins_dispatch(&nav, expanded, &exit_status);
                if (outcome == BUILTIN_REQUEST_EXIT) {
                    running = false;
                }
                if (nav.last_error && bell) {
                    fputc('\a', stdout);
                }
                dispatched_as_cd_target = true;
            } else if (auto_cd && builtins_is_auto_cd_target(expanded)) {
                char *cd_line = xsprintf("cd %s", expanded);
                builtins_dispatch(&nav, cd_line, &exit_status);
                if (nav.last_error && bell) {
                    fputc('\a', stdout);
                }
                free(cd_line);
                dispatched_as_cd_target = true;
            }
        }

        if (!dispatched_as_cd_target && running) {
            if (!process_command_exists("/bin/sh")) {
                fprintf(stderr, "calm: /bin/sh not found -- cannot run external commands\n");
                if (bell) {
                    fputc('\a', stdout);
                }
            } else {
                int status = execute_run(&nav, prelude, expanded);
                if (status < 0 && bell) {
                    fputc('\a', stdout);
                }
                if (status >= 0) {
                    exit_status = status;
                }
            }
        }

        free(expanded);
    }

    history_free(&hist);
    free(prelude);
    strlist_free(&known);
    alias_table_free(&aliases);
    shell_nav_free(&nav);
    theme_free(&theme);
    calm_document_free(&cfg);

    printf("\n");
    return exit_status;
}
