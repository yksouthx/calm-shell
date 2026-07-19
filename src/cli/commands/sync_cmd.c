#include "cli/commands/sync_cmd.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "config/calmconf.h"
#include "config/paths.h"
#include "config/scaffold.h"
#include "term/emulator.h"
#include "theme/sync.h"
#include "theme/theme.h"

int sync_cmd_run(void) {
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
        fprintf(stderr, "calm: no usable theme found\n");
        calm_document_free(&cfg);
        return 1;
    }

    const char *override_name = calm_document_get_str(&cfg, "sync", "terminal_override");
    TermEmulatorKind kind = term_emulator_detect(override_name);
    printf("Theme: %s\nTerminal emulator: %s%s\n", theme.display_name, term_emulator_name(kind),
           (override_name && override_name[0]) ? " (forced via terminal_override)" : "");

    ThemeSyncResult result;
    theme_sync_apply(&theme, &cfg, &result);

    if (result.terminal_enabled) {
        printf("%s\n", result.terminal_detail ? result.terminal_detail : "(no detail)");
    } else {
        printf("Terminal sync disabled ([sync] sync_terminal = false)\n");
    }

    if (result.fastfetch_enabled) {
        printf("%s %s\n", result.fastfetch_synced ? "fastfetch: config synced ->" : "fastfetch: failed to write",
               result.fastfetch_path ? result.fastfetch_path : "(unknown path)");
    } else {
        printf("Fastfetch sync disabled ([sync] sync_fastfetch = false)\n");
    }

    bool all_ok = (!result.terminal_enabled || result.terminal_synced) &&
                  (!result.fastfetch_enabled || result.fastfetch_synced);

    theme_sync_result_free(&result);
    theme_free(&theme);
    calm_document_free(&cfg);
    return all_ok ? 0 : 1;
}
