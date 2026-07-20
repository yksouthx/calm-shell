#include "theme/sync.h"

#include <stdlib.h>

#include "term/emulator.h"
#include "term/emulator_sync.h"
#include "theme/app_sync.h"
#include "theme/appearance.h"
#include "theme/fastfetch_sync.h"

void theme_sync_result_free(ThemeSyncResult *r) {
    free(r->terminal_detail);
    free(r->fastfetch_path);
    r->terminal_detail = NULL;
    r->fastfetch_path = NULL;
    app_sync_result_free(&r->apps);
}

void theme_sync_apply(const Theme *theme, const CalmDocument *cfg, ThemeSyncResult *out) {
    out->terminal_enabled = calm_document_get_bool(cfg, "sync", "sync_terminal", true);
    out->terminal_synced = false;
    out->terminal_detail = NULL;
    out->fastfetch_enabled = calm_document_get_bool(cfg, "sync", "sync_fastfetch", true);
    out->fastfetch_synced = false;
    out->fastfetch_path = NULL;
    out->apps_enabled = calm_document_get_bool(cfg, "sync", "sync_apps", true);
    out->apps.items = NULL;
    out->apps.count = 0;

    if (out->terminal_enabled) {
        TermAppearance appearance;
        appearance_load(cfg, &appearance);

        const char *override_name = calm_document_get_str(cfg, "sync", "terminal_override");
        TermEmulatorKind kind = term_emulator_detect(override_name);

        out->terminal_synced = emulator_sync_write(kind, theme, &appearance, &out->terminal_detail);
        appearance_free(&appearance);
    }

    if (out->fastfetch_enabled) {
        out->fastfetch_synced = fastfetch_sync_write(theme, &out->fastfetch_path);
    }

    if (out->apps_enabled) {
        app_sync_write_all(theme, &out->apps);
    }
}
