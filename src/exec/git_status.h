/* Reads the current directory's git status for the prompt line, via
 * `git status --porcelain --branch` -- no libgit2 dependency for what
 * amounts to reading one header line and counting the rest. */
#ifndef CALM_EXEC_GIT_STATUS_H
#define CALM_EXEC_GIT_STATUS_H

#include <stdbool.h>

typedef struct {
    char *branch; /* newly allocated, caller must free() */
    bool clean;
} GitStatus;

/* Returns false (and leaves *out untouched) if the current directory
 * isn't inside a git repository, or `git` isn't available. */
bool git_status(GitStatus *out);

#endif /* CALM_EXEC_GIT_STATUS_H */
