/* The `.calm` config format.
 *
 * Design goals: read like Fish config, no build-a-config-language ambition.
 * Sections group related keys, values are typed (string / bool / int / list),
 * and triple-quoted strings let functions.calm hold real shell bodies
 * without the parser needing to understand shell syntax.
 *
 *   # comment
 *   [shell]
 *   theme = "calm-lavender"
 *   greeting = true
 *   retries = 3
 *   tags = ["fast", "quiet"]
 *
 *   [functions]
 *   mkcd = """
 *   mkdir -p "$1" && cd "$1"
 *   """
 *
 *   include "aliases.calm"
 */
#ifndef CALM_FORMAT_H
#define CALM_FORMAT_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    CALM_STR,
    CALM_BOOL,
    CALM_INT,
    CALM_LIST,
} CalmType;

typedef struct CalmValue CalmValue;

struct CalmValue {
    CalmType type;
    char *str;       /* CALM_STR */
    bool boolean;    /* CALM_BOOL */
    long long integer; /* CALM_INT */
    CalmValue *items; /* CALM_LIST */
    size_t item_count;
};

typedef struct {
    char *key;
    CalmValue value;
} CalmEntry;

typedef struct {
    char *name;
    CalmEntry *entries;
    size_t count;
    size_t cap;
} CalmSection;

typedef struct {
    CalmSection *sections;
    size_t count;
    size_t cap;
} CalmDocument;

/* Initializes an empty document (always contains an anonymous "" section). */
void calm_document_init(CalmDocument *doc);

/* Frees every string/list owned by the document and its sections. */
void calm_document_free(CalmDocument *doc);

/* Returns the section by name, or NULL if it doesn't exist. */
const CalmSection *calm_document_section(const CalmDocument *doc, const char *name);

/* Returns the value for section/key, or NULL if either is missing. */
const CalmValue *calm_document_get(const CalmDocument *doc, const char *section, const char *key);

/* Convenience accessors. Return NULL/false/0 (via out param success) when
 * the key is missing or isn't of the requested type. */
const char *calm_document_get_str(const CalmDocument *doc, const char *section, const char *key);
bool calm_document_get_bool(const CalmDocument *doc, const char *section, const char *key, bool fallback);
long long calm_document_get_int(const CalmDocument *doc, const char *section, const char *key, long long fallback);

/* Parses `.calm` source text (without resolving includes -- use
 * calm_parse_file for that). Returns true on success and fills *out;
 * on failure returns false and fills *err_msg with a newly allocated
 * message the caller must free(). */
bool calm_parse(const char *input, CalmDocument *out, char **err_msg);

/* Parses a `.calm` file, following `include "..."` directives (resolved
 * relative to the including file's directory). Include depth is bounded
 * to avoid infinite recursion from a misconfigured file. */
bool calm_parse_file(const char *path, CalmDocument *out, char **err_msg);

/* Expands a leading `~` to $HOME and `$VAR` / `${VAR}` references anywhere
 * in the string. Returns a newly allocated string the caller must free(). */
char *calm_expand(const char *value);

#endif /* CALM_FORMAT_H */
