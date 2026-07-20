#include "cli/commands/doctor_cmd.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "config/paths.h"
#include "config/repair.h"
#include "exec/process.h"
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

    /* Everything about config files, themes, plugins, permissions,
     * and terminal/Fastfetch sync is the same scan `calm repair` runs
     * -- just without apply_fixes, so nothing here is ever written.
     * A REPAIR_FIXED status can't appear in scan mode. */
    RepairReport report;
    bool scanned = repair_scan(false, &report);
    if (!scanned) {
        check(false, "config directory exists",
              "run `calm` once to create it, or `calm config path` to see where");
    } else {
        for (size_t i = 0; i < report.count; i++) {
            check(report.items[i].status == REPAIR_OK, report.items[i].label, report.items[i].detail);
        }
    }
    repair_report_free(&report);

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
        printf("%d item%s could use attention. Run `calm repair` to fix what it safely can.\n", g_problems,
               g_problems == 1 ? "" : "s");
    }
    return g_problems == 0 ? 0 : 1;
}
