/* calmconf -- the `.calm` configuration format.
 *
 * Design goals: read like a Fish/Git config, not a build-a-language
 * ambition. One native, dependency-free recursive-descent-free (this
 * grammar is regular enough for a single-pass line scanner) parser
 * backs every piece of Calm-shell configuration -- shell behavior,
 * aliases, keybindings, environment variables, plugin manifests, and
 * themes all share this one format, so there is exactly one parser to
 * know, test, and optimize.
 *
 *   # comment                        <- to end of line, anywhere unquoted
 *   [shell]                          <- section
 *   theme = "calm-lavender"          <- string
 *   greeting = true                  <- bool
 *   retries = 3                      <- int
 *   timeout = 1.5                    <- float
 *   tags = ["fast", "quiet"]         <- list (homogeneous or not)
 *
 *   [directory.aliases]              <- dotted section name, just a
 *   ".." = "cd .."                      flat namespace convention, not
 *                                        a nested-table tree
 *
 *   [functions]
 *   mkcd = """
 *   mkdir -p "$1" && cd "$1"
 *   """                              <- triple-quoted block: raw text,
 *                                        copied verbatim (no comment
 *                                        stripping, no escaping) so a
 *                                        shell function body's own `#`
 *                                        comments and quotes survive
 *
 *   include "aliases.calm"           <- pulls in another file, resolved
 *                                        relative to this file's directory
 *
 * A bare, unquoted word (`theme = calm-lavender`) is accepted as a
 * plain string for ergonomics -- quotes are only required when the
 * value contains whitespace, a `#`, or looks like another type.
 */
#ifndef CALM_CONFIG_CALMCONF_H
#define CALM_CONFIG_CALMCONF_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    CALM_STRING,
    CALM_BOOL,
    CALM_INT,
    CALM_FLOAT,
    CALM_LIST,
} CalmType;

typedef struct CalmValue CalmValue;

/* A tagged union: exactly one member of `as` is valid, selected by
 * `type`. Kept as a plain struct (not opaque) since callers throughout
 * the codebase read values directly -- this is an internal config
 * format, not a public library boundary. */
struct CalmValue {
    CalmType type;
    union {
        char *string; /* CALM_STRING, owned */
        bool boolean; /* CALM_BOOL */
        long long integer; /* CALM_INT */
        double real; /* CALM_FLOAT */
        struct {
            CalmValue *items; /* CALM_LIST, owned */
            size_t count;
        } list;
    } as;
};

typedef struct {
    char *key;
    CalmValue value;
} CalmEntry;

/* A named group of key/value entries. Section names are a flat string
 * (e.g. "directory.aliases") rather than a nested tree -- every
 * consumer in this codebase looks sections up by their exact dotted
 * name, and a flat namespace is both simpler to implement correctly
 * and faster to look up than walking a tree for the handful of
 * sections a config file actually has. */
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

/* Initializes an empty document (always contains an anonymous ""
 * section for top-level, pre-[section] keys). */
void calm_document_init(CalmDocument *doc);

/* Frees every string/list owned by the document and its sections. */
void calm_document_free(CalmDocument *doc);

/* Returns the section by name, or NULL if it doesn't exist. */
const CalmSection *calm_document_section(const CalmDocument *doc, const char *name);

/* Returns the value for section/key, or NULL if either is missing. */
const CalmValue *calm_document_get(const CalmDocument *doc, const char *section, const char *key);

/* Convenience typed accessors. Return NULL/the given fallback when the
 * key is missing or isn't of the requested type -- callers never need
 * to check "is this key even present" separately from "is it the
 * right shape". */
const char *calm_document_get_str(const CalmDocument *doc, const char *section, const char *key);
bool calm_document_get_bool(const CalmDocument *doc, const char *section, const char *key, bool fallback);
long long calm_document_get_int(const CalmDocument *doc, const char *section, const char *key, long long fallback);
double calm_document_get_float(const CalmDocument *doc, const char *section, const char *key, double fallback);

/* Parses `.calm` source text (without resolving includes -- use
 * calm_parse_file for that). Returns true on success and fills *out;
 * on failure returns false and fills *err_msg with a newly allocated
 * message the caller must free(). */
bool calm_parse(const char *input, CalmDocument *out, char **err_msg);

/* Parses a `.calm` file, following `include "..."` directives
 * (resolved relative to the including file's directory). Each file is
 * canonicalized and included at most once even if referenced from
 * multiple places, which also makes an include cycle a no-op rather
 * than a crash; a depth bound is kept as a defense-in-depth backstop. */
bool calm_parse_file(const char *path, CalmDocument *out, char **err_msg);

/* Expands a leading `~` to $HOME and `$VAR` / `${VAR}` references
 * anywhere in the string. Returns a newly allocated string the caller
 * must free(). Intentionally not applied automatically during
 * parsing: a `[functions]` body should keep its `$1`/`$@` literal for
 * the shell to expand, not have calmconf expand it first. */
char *calm_expand(const char *value);

#endif /* CALM_CONFIG_CALMCONF_H */
