#include "cli/commands/doctor_cmd.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "config/calmconf.h"
#include "config/paths.h"
#include "exec/process.h"
#include "theme/theme.h"
#include "util/fsutil.h"

static int g_problems = 0;

static void check(bool ok, const char *label, const char *detail_if_bad) {
    printf("  [%s] %s\n", ok ? "ok" : "!!", label);
    if (!ok) {
        if (detail_if_bad) {
            printf("        %s\n", detail_if_bad);
        }
        g_problems++;
    }
}

int doctor_cmd_run(void) {
    printf("Calm-shell doctor\n\n");

    char *home = home_dir();
    check(home != NULL, "$HOME is set", "cd/auto_cd/theme lookups need $HOME to resolve ~");
    free(home);

    char *dir = config_dir();
    bool dir_ok = dir && path_is_dir(dir);
    check(dir_ok, "config directory exists", "run `calm` once to create it, or `calm config path` to see where");

    char *cfg_path = config_file();
    if (cfg_path && path_is_file(cfg_path)) {
        CalmDocument doc;
        char *err = NULL;
        bool parses = calm_parse_file(cfg_path, &doc, &err);
        check(parses, "config.calm parses", err);
        if (parses) {
            calm_document_free(&doc);
        }
        free(err);
    } else {
        check(false, "config.calm parses", "file doesn't exist yet");
    }
    free(cfg_path);
    free(dir);

    Theme theme;
    bool theme_ok = theme_load_active(&theme);
    check(theme_ok, "active theme loads", "falls back to calm-lavender if this is unset or broken");
    if (theme_ok) {
        theme_free(&theme);
    }

    check(process_command_exists("/bin/sh"), "/bin/sh is available", "external commands can't run without it");
    check(process_command_exists("git"), "git is available", "the prompt's git status segment will just stay hidden");

    const char *editor = getenv("EDITOR");
    check(editor != NULL && editor[0] != '\0', "$EDITOR is set", "`calm config edit` falls back to `vi`");

    const char *colorterm = getenv("COLORTERM");
    bool truecolor_hint = colorterm && (strstr(colorterm, "truecolor") || strstr(colorterm, "24bit"));
    check(truecolor_hint, "terminal advertises truecolor ($COLORTERM)",
          "colors may still work -- many terminals support 24-bit color without setting this");

    printf("\n");
    if (g_problems == 0) {
        printf("Everything looks good.\n");
    } else {
        printf("%d item%s could use attention.\n", g_problems, g_problems == 1 ? "" : "s");
    }
    return g_problems == 0 ? 0 : 1;
}
