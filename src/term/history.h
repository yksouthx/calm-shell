/* Persistent command history: on-disk file (one escaped entry per
 * line), an in-memory ring capped at a configurable size, optional
 * immediate-duplicate suppression, and a fuzzy Ctrl+R search over it. */
#ifndef CALM_TERM_HISTORY_H
#define CALM_TERM_HISTORY_H

#include <stdbool.h>
#include <stddef.h>

#include "util/strlist.h"

typedef struct {
    char **entries; /* oldest first */
    size_t count;
    size_t cap;
    char *path; /* NULL if history couldn't be located */

    /* history.calm's `size` -- the in-memory (and on-disk, since every
     * add is also trimmed on load) entry cap. Oldest entries are
     * dropped first once exceeded. 0 means unbounded. */
    size_t max_entries;
    /* history.calm's `ignore_dups` -- a command identical to the
     * immediately preceding entry is not added again. */
    bool ignore_dups;
} History;

/* Loads history from `path` (NULL means "don't persist"), applying
 * `max_entries` immediately so a long-lived file never makes startup
 * scale with its whole contents forever. */
bool history_load(const char *path, size_t max_entries, bool ignore_dups, History *out);

/* Appends `line` to both the in-memory list and the on-disk file
 * (best-effort -- a write failure doesn't stop the shell running),
 * applying `ignore_dups` and `max_entries` as configured. */
void history_add(History *hist, const char *line);

void history_free(History *hist);

#define CALM_FUZZY_HISTORY_LIMIT 50

/* Searches `hist` (most recent first, de-duplicated by command text)
 * for entries matching `query` as a case-insensitive, ordered
 * subsequence (fzf/skim-style fuzzy match), returning up to `limit`
 * results best-match-first. An empty `query` returns the `limit` most
 * recent entries. Caller must strlist_free() the result. */
StrList history_fuzzy_search(const History *hist, const char *query, size_t limit);

#endif /* CALM_TERM_HISTORY_H */
