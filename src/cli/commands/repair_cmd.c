#include "cli/commands/repair_cmd.h"

#include <stdio.h>

#include "config/repair.h"

static const char *status_label(RepairStatus s) {
    switch (s) {
        case REPAIR_OK:
            return "ok";
        case REPAIR_FIXED:
            return "fixed";
        case REPAIR_BROKEN:
        default:
            return "!!";
    }
}

int repair_cmd_run(void) {
    printf("Calm-shell repair\n\n");

    RepairReport report;
    bool scanned = repair_scan(/*apply_fixes=*/true, &report);
    if (!scanned) {
        fprintf(stderr, "calm: could not create or access ~/.config/calm-shell -- check permissions\n");
        repair_report_free(&report);
        return 1;
    }

    int fixed = 0, broken = 0;
    for (size_t i = 0; i < report.count; i++) {
        const RepairCheck *c = &report.items[i];
        printf("  [%s] %s\n", status_label(c->status), c->label);
        if (c->detail) {
            printf("        %s\n", c->detail);
        }
        if (c->status == REPAIR_FIXED) {
            fixed++;
        } else if (c->status == REPAIR_BROKEN) {
            broken++;
        }
    }

    printf("\n");
    if (report.backups_made > 0) {
        printf("Backed up %d file%s to %s before changing them.\n", report.backups_made,
               report.backups_made == 1 ? "" : "s", report.backup_path);
    }
    if (fixed == 0 && broken == 0) {
        printf("Everything was already healthy -- nothing to repair.\n");
    } else {
        if (fixed > 0) {
            printf("Fixed %d item%s.\n", fixed, fixed == 1 ? "" : "s");
        }
        if (broken > 0) {
            printf("%d item%s could not be fixed automatically -- see details above.\n", broken,
                   broken == 1 ? "" : "s");
        }
    }

    int exit_code = broken == 0 ? 0 : 1;
    repair_report_free(&report);
    return exit_code;
}
