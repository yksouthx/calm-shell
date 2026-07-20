/* The scan engine behind both `calm doctor` (report only) and `calm
 * repair` (report + fix). Structured as one shared pass over the same
 * things -- topic files, themes, plugin manifests, permissions,
 * terminal/Fastfetch sync -- so the two commands can't quietly drift
 * apart on what "healthy" means; `calm doctor`'s message pointing
 * someone at `calm repair` is describing the literal same checks,
 * just run without `apply_fixes`.
 *
 * Repair strategy is deliberately narrow: a corrupt or missing file
 * is only ever restored to a *known* default (the scaffold table in
 * config/scaffold.c, or the bundled calm-lavender theme) -- never
 * pattern-patched. There's no general syntax-error corrector here,
 * because guessing at a user's intent from a broken file and writing
 * something plausible in its place is far more likely to silently
 * discard real customization than a single blunt "restore to
 * default" ever is. Anything without a known default (a custom
 * theme, a third-party plugin's manifest) is reported broken, never
 * silently regenerated -- restoring a *default* stops being safe the
 * moment there's no default to restore. Every file this module
 * overwrites is backed up first (config/paths.c's backups_dir()),
 * timestamped per repair run, so an unwanted restore is always
 * reversible by hand. */
#ifndef CALM_CONFIG_REPAIR_H
#define CALM_CONFIG_REPAIR_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    REPAIR_OK,      /* already fine, nothing to do */
    REPAIR_FIXED,   /* was broken/missing; apply_fixes restored it */
    REPAIR_BROKEN,  /* broken/missing and either not fixable, or apply_fixes was false */
} RepairStatus;

typedef struct {
    RepairStatus status;
    char *label;  /* e.g. "environment.calm parses" */
    char *detail; /* human-readable elaboration; NULL if status is REPAIR_OK */
} RepairCheck;

typedef struct {
    RepairCheck *items;
    size_t count;
    size_t cap;
    /* How many original files were copied into backups_dir() this
     * run (0 when apply_fixes is false -- a scan never writes
     * anything). */
    int backups_made;
    /* Path backups were written under this run, e.g.
     * ~/.config/calm-shell/.backups/20260719-141055 -- NULL if
     * backups_made is 0. */
    char *backup_path;
} RepairReport;

void repair_report_free(RepairReport *r);

/* Runs every check. When `apply_fixes` is true, a check that finds a
 * problem it knows how to safely resolve fixes it immediately (after
 * backing up whatever it's about to overwrite) before recording the
 * result, so later checks in the same run (e.g. "config.calm
 * parses") see the repaired state. Always fills `*report`, one
 * RepairCheck per thing checked, and never fails outright -- an
 * unresolvable problem is reflected as a REPAIR_BROKEN entry, not a
 * false return. The one case this returns false is being unable to
 * create ~/.config/calm-shell at all, which leaves nothing else to
 * meaningfully check. */
bool repair_scan(bool apply_fixes, RepairReport *report);

#endif /* CALM_CONFIG_REPAIR_H */
