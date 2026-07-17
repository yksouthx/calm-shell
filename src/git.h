/* Git branch/dirty status for the prompt. */
#ifndef CALM_GIT_H
#define CALM_GIT_H

#include <stdbool.h>

typedef struct {
    char *branch; /* caller must free() */
    bool clean;
} GitStatus;

/* Returns false if the current directory isn't inside a git repo (or
 * `git` isn't runnable at all). On success, `out->branch` must be
 * free()d by the caller.
 *
 * Deliberately a single subprocess spawn (`git status --porcelain
 * --branch`) rather than three separate `git` calls (is-inside-work-tree,
 * rev-parse HEAD, status --porcelain): this runs on every prompt render,
 * so on modest hardware three fork+execs per keystroke-to-prompt cycle
 * is real, perceptible lag. `--branch` prepends a `## ...` header line
 * to the same porcelain output, so one call gives us both branch and
 * dirty state. */
bool git_status(GitStatus *out);

#endif /* CALM_GIT_H */
