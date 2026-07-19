/* Command-line alias expansion, sourced from `[aliases]` and
 * `[directory.aliases]`. */
#ifndef CALM_SHELL_ALIAS_TABLE_H
#define CALM_SHELL_ALIAS_TABLE_H

#include "config/calmconf.h"

typedef struct {
    char *key;
    char *value;
} AliasEntry;

typedef struct {
    AliasEntry *aliases;
    size_t count;
    size_t cap;
} AliasTable;

/* Builds an alias table from `[aliases]` and `[directory.aliases]`. */
AliasTable alias_table_load(const CalmDocument *cfg);
void alias_table_free(AliasTable *table);

/* Expands a typed line against the alias table. A whole-line match
 * takes priority over a first-word match, mirroring how interactive
 * shell aliases behave (`ll` alone vs. `ll -a`). Returns a newly
 * allocated string -- the original line, unchanged, if nothing
 * matched. */
char *alias_table_expand(const AliasTable *table, const char *line);

#endif /* CALM_SHELL_ALIAS_TABLE_H */
