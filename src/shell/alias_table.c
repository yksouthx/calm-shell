#include "shell/alias_table.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "util/xalloc.h"

static void alias_table_add(AliasTable *table, const char *key, const char *value) {
    if (table->count == table->cap) {
        table->cap = table->cap == 0 ? 8 : table->cap * 2;
        table->aliases = xrealloc(table->aliases, table->cap * sizeof(AliasEntry));
    }
    table->aliases[table->count].key = xstrdup(key);
    table->aliases[table->count].value = xstrdup(value);
    table->count++;
}

static void load_section(AliasTable *table, const CalmDocument *cfg, const char *section_name) {
    const CalmSection *section = calm_document_section(cfg, section_name);
    if (!section) {
        return;
    }
    for (size_t i = 0; i < section->count; i++) {
        if (section->entries[i].value.type == CALM_STRING) {
            alias_table_add(table, section->entries[i].key, section->entries[i].value.as.string);
        }
    }
}

AliasTable alias_table_load(const CalmDocument *cfg) {
    AliasTable table = {0};
    /* Directory shortcuts (`..`, `-`, ...) first, then general
     * aliases -- both land in the same flat table; load order here
     * only matters if a user defines the same key in both sections, in
     * which case [aliases] intentionally wins. */
    load_section(&table, cfg, "directory.aliases");
    load_section(&table, cfg, "aliases");
    return table;
}

void alias_table_free(AliasTable *table) {
    for (size_t i = 0; i < table->count; i++) {
        free(table->aliases[i].key);
        free(table->aliases[i].value);
    }
    free(table->aliases);
    table->aliases = NULL;
    table->count = 0;
    table->cap = 0;
}

char *alias_table_expand(const AliasTable *table, const char *line) {
    for (size_t i = 0; i < table->count; i++) {
        if (strcmp(table->aliases[i].key, line) == 0) {
            return xstrdup(table->aliases[i].value);
        }
    }

    size_t first_word_len = 0;
    while (line[first_word_len] && !isspace((unsigned char)line[first_word_len])) {
        first_word_len++;
    }
    if (first_word_len == 0) {
        return xstrdup(line);
    }

    for (size_t i = 0; i < table->count; i++) {
        size_t key_len = strlen(table->aliases[i].key);
        if (key_len == first_word_len && strncmp(table->aliases[i].key, line, key_len) == 0) {
            return xsprintf("%s%s", table->aliases[i].value, line + first_word_len);
        }
    }
    return xstrdup(line);
}
