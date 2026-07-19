/* Tab completion: commands at the first word, filesystem paths
 * everywhere else. Scope, honestly: completion inherits the
 * highlighter's word-boundary-only view of the line -- no flag-aware
 * or per-command completion specs. */
#ifndef CALM_TERM_COMPLETION_H
#define CALM_TERM_COMPLETION_H

#include "util/strlist.h"

/* Returns every known command (builtins + `known`) whose name starts
 * with `word`, sorted and de-duplicated. Caller must strlist_free(). */
StrList complete_commands(const StrList *known, const char *word);

/* Returns every directory entry under `word`'s directory portion whose
 * name starts with `word`'s filename portion, sorted and
 * de-duplicated, with a trailing '/' appended to directory matches.
 * Hidden entries (leading '.') are excluded unless `word` itself
 * starts with '.'. Caller must strlist_free(). */
StrList complete_paths(const char *word);

#endif /* CALM_TERM_COMPLETION_H */
