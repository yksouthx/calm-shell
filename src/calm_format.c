#include "calm_format.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"

#define CALM_MAX_INCLUDE_DEPTH 8

static void calm_value_free(CalmValue *v) {
    if (!v) {
        return;
    }
    if (v->type == CALM_STR) {
        free(v->str);
    } else if (v->type == CALM_LIST) {
        for (size_t i = 0; i < v->item_count; i++) {
            calm_value_free(&v->items[i]);
        }
        free(v->items);
    }
}

void calm_document_init(CalmDocument *doc) {
    doc->sections = NULL;
    doc->count = 0;
    doc->cap = 0;
}

void calm_document_free(CalmDocument *doc) {
    for (size_t i = 0; i < doc->count; i++) {
        CalmSection *s = &doc->sections[i];
        for (size_t j = 0; j < s->count; j++) {
            free(s->entries[j].key);
            calm_value_free(&s->entries[j].value);
        }
        free(s->entries);
        free(s->name);
    }
    free(doc->sections);
    doc->sections = NULL;
    doc->count = 0;
    doc->cap = 0;
}

/* Finds (or creates) the section by name, mirroring
 * `doc.sections.entry(name).or_default()` in the Rust original. */
static CalmSection *section_entry(CalmDocument *doc, const char *name) {
    for (size_t i = 0; i < doc->count; i++) {
        if (strcmp(doc->sections[i].name, name) == 0) {
            return &doc->sections[i];
        }
    }
    if (doc->count == doc->cap) {
        size_t new_cap = doc->cap == 0 ? 4 : doc->cap * 2;
        doc->sections = xrealloc(doc->sections, new_cap * sizeof(CalmSection));
        doc->cap = new_cap;
    }
    CalmSection *s = &doc->sections[doc->count++];
    s->name = xstrdup(name);
    s->entries = NULL;
    s->count = 0;
    s->cap = 0;
    return s;
}

/* Inserts or overwrites `key` in `section` -- later value wins, matching
 * the Rust HashMap's `.insert` overwrite semantics. */
static void section_insert(CalmSection *s, const char *key, CalmValue value) {
    for (size_t i = 0; i < s->count; i++) {
        if (strcmp(s->entries[i].key, key) == 0) {
            calm_value_free(&s->entries[i].value);
            s->entries[i].value = value;
            return;
        }
    }
    if (s->count == s->cap) {
        size_t new_cap = s->cap == 0 ? 4 : s->cap * 2;
        s->entries = xrealloc(s->entries, new_cap * sizeof(CalmEntry));
        s->cap = new_cap;
    }
    s->entries[s->count].key = xstrdup(key);
    s->entries[s->count].value = value;
    s->count++;
}

const CalmSection *calm_document_section(const CalmDocument *doc, const char *name) {
    for (size_t i = 0; i < doc->count; i++) {
        if (strcmp(doc->sections[i].name, name) == 0) {
            return &doc->sections[i];
        }
    }
    return NULL;
}

const CalmValue *calm_document_get(const CalmDocument *doc, const char *section, const char *key) {
    const CalmSection *s = calm_document_section(doc, section);
    if (!s) {
        return NULL;
    }
    for (size_t i = 0; i < s->count; i++) {
        if (strcmp(s->entries[i].key, key) == 0) {
            return &s->entries[i].value;
        }
    }
    return NULL;
}

const char *calm_document_get_str(const CalmDocument *doc, const char *section, const char *key) {
    const CalmValue *v = calm_document_get(doc, section, key);
    if (v && v->type == CALM_STR) {
        return v->str;
    }
    return NULL;
}

bool calm_document_get_bool(const CalmDocument *doc, const char *section, const char *key, bool fallback) {
    const CalmValue *v = calm_document_get(doc, section, key);
    if (v && v->type == CALM_BOOL) {
        return v->boolean;
    }
    return fallback;
}

long long calm_document_get_int(const CalmDocument *doc, const char *section, const char *key, long long fallback) {
    const CalmValue *v = calm_document_get(doc, section, key);
    if (v && v->type == CALM_INT) {
        return v->integer;
    }
    return fallback;
}

/* Merges `other` into `doc`, consuming `other` (its heap storage is
 * either transferred or freed; the struct must not be used afterward). */
static void calm_document_merge(CalmDocument *doc, CalmDocument *other) {
    for (size_t i = 0; i < other->count; i++) {
        CalmSection *src = &other->sections[i];
        CalmSection *dst = section_entry(doc, src->name);
        for (size_t j = 0; j < src->count; j++) {
            section_insert(dst, src->entries[j].key, src->entries[j].value);
            free(src->entries[j].key);
        }
        free(src->entries);
        free(src->name);
    }
    free(other->sections);
    other->sections = NULL;
    other->count = 0;
    other->cap = 0;
}

/* --- scalar/value parsing ------------------------------------------- */

static char *unescape(const char *s) {
    /* Mirrors `s.replace("\\\"", "\"").replace("\\\\", "\\")`: a
     * straightforward first-pass unescape for the two sequences the
     * format actually uses (not a full escape grammar). */
    size_t len = strlen(s);
    char *out = xmalloc(len + 1);
    size_t oi = 0;
    for (size_t i = 0; i < len; i++) {
        if (s[i] == '\\' && i + 1 < len && (s[i + 1] == '"' || s[i + 1] == '\\')) {
            out[oi++] = s[i + 1];
            i++;
        } else {
            out[oi++] = s[i];
        }
    }
    out[oi] = '\0';
    return out;
}

static bool parse_scalar(const char *raw_in, CalmValue *out, char **err_msg);

/* Splits `s` on top-level commas: respects `"..."` quoting and `[...]`
 * nesting, so `["a,b", [1, 2], 3` (sans the trailing bracket, which the
 * caller has already stripped) splits into three items, not five. */
static char **split_top_level_commas(const char *s, size_t *out_count) {
    size_t cap = 4, count = 0;
    char **items = xmalloc(cap * sizeof(char *));
    size_t len = strlen(s);
    char *current = xmalloc(len + 1);
    size_t ci = 0;
    int depth = 0;
    bool in_quotes = false;

    for (size_t i = 0; i < len; i++) {
        char c = s[i];
        if (c == '"') {
            in_quotes = !in_quotes;
            current[ci++] = c;
        } else if (c == '[' && !in_quotes) {
            depth++;
            current[ci++] = c;
        } else if (c == ']' && !in_quotes) {
            depth--;
            current[ci++] = c;
        } else if (c == ',' && !in_quotes && depth == 0) {
            current[ci] = '\0';
            if (count == cap) {
                cap *= 2;
                items = xrealloc(items, cap * sizeof(char *));
            }
            items[count++] = xstrdup(current);
            ci = 0;
        } else {
            current[ci++] = c;
        }
    }
    current[ci] = '\0';
    if (!is_blank(current)) {
        if (count == cap) {
            cap *= 2;
            items = xrealloc(items, cap * sizeof(char *));
        }
        items[count++] = xstrdup(current);
    }
    free(current);
    *out_count = count;
    return items;
}

static bool parse_scalar(const char *raw_in, CalmValue *out, char **err_msg) {
    char *raw = xstrdup(raw_in);
    trim_in_place(raw);

    size_t len = strlen(raw);

    if (len >= 2 && raw[0] == '"' && raw[len - 1] == '"') {
        raw[len - 1] = '\0';
        out->type = CALM_STR;
        out->str = unescape(raw + 1);
        free(raw);
        return true;
    }
    if (strcmp(raw, "true") == 0) {
        out->type = CALM_BOOL;
        out->boolean = true;
        free(raw);
        return true;
    }
    if (strcmp(raw, "false") == 0) {
        out->type = CALM_BOOL;
        out->boolean = false;
        free(raw);
        return true;
    }
    if (len > 0) {
        char *endptr = NULL;
        long long n = strtoll(raw, &endptr, 10);
        if (endptr && *endptr == '\0' && endptr != raw) {
            out->type = CALM_INT;
            out->integer = n;
            free(raw);
            return true;
        }
    }
    if (len >= 2 && raw[0] == '[' && raw[len - 1] == ']') {
        raw[len - 1] = '\0';
        size_t item_count = 0;
        char **raw_items = split_top_level_commas(raw + 1, &item_count);

        CalmValue *items = item_count > 0 ? xmalloc(item_count * sizeof(CalmValue)) : NULL;
        size_t parsed = 0;
        bool ok = true;
        for (size_t i = 0; i < item_count; i++) {
            char *trimmed = xstrdup(raw_items[i]);
            trim_in_place(trimmed);
            if (!is_blank(trimmed)) {
                if (!parse_scalar(trimmed, &items[parsed], err_msg)) {
                    ok = false;
                    free(trimmed);
                    break;
                }
                parsed++;
            }
            free(trimmed);
        }
        for (size_t i = 0; i < item_count; i++) {
            free(raw_items[i]);
        }
        free(raw_items);
        free(raw);
        if (!ok) {
            for (size_t i = 0; i < parsed; i++) {
                calm_value_free(&items[i]);
            }
            free(items);
            return false;
        }
        out->type = CALM_LIST;
        out->items = items;
        out->item_count = parsed;
        return true;
    }
    if (len == 0) {
        if (err_msg) {
            *err_msg = xstrdup("empty value");
        }
        free(raw);
        return false;
    }
    /* Bare, unquoted word -- accept as a plain string for ergonomics
     * (e.g. `theme = calm-lavender` without quotes). */
    out->type = CALM_STR;
    out->str = raw;
    return true;
}

/* --- top-level parse -------------------------------------------------- */

bool calm_parse(const char *input, CalmDocument *out, char **err_msg) {
    calm_document_init(out);
    char *current_section = xstrdup("");
    section_entry(out, current_section);

    size_t line_count = 0;
    char **lines = split_lines(input, &line_count);

    size_t i = 0;
    bool ok = true;
    while (i < line_count) {
        char *raw_line = lines[i];
        char *line = xstrdup(raw_line);
        trim_in_place(line);

        if (is_blank(line) || line[0] == '#' || starts_with(line, "include ")) {
            free(line);
            i++;
            continue;
        }

        if (line[0] == '[' && line[strlen(line) - 1] == ']') {
            free(current_section);
            char *inner = xstrndup(line + 1, strlen(line) - 2);
            trim_in_place(inner);
            current_section = inner;
            section_entry(out, current_section);
            free(line);
            i++;
            continue;
        }

        char *eq = strchr(line, '=');
        if (!eq) {
            if (err_msg) {
                *err_msg = xsprintf("line %zu: expected `key = value`, got: %s", i + 1, line);
            }
            free(line);
            ok = false;
            break;
        }
        *eq = '\0';
        char *key_part = line;
        char *value_part = eq + 1;
        trim_in_place(key_part);
        trim_in_place(value_part);

        char *key;
        size_t kp_len = strlen(key_part);
        if (kp_len >= 2 && key_part[0] == '"' && key_part[kp_len - 1] == '"') {
            char *inner = xstrndup(key_part + 1, kp_len - 2);
            key = unescape(inner);
            free(inner);
        } else {
            key = xstrdup(key_part);
        }

        if (starts_with(value_part, "\"\"\"")) {
            char *after_open = value_part + 3;
            size_t ao_len = strlen(after_open);
            if (ao_len >= 3 && strcmp(after_open + ao_len - 3, "\"\"\"") == 0) {
                char *closed = xstrndup(after_open, ao_len - 3);
                CalmValue v = {.type = CALM_STR, .str = closed};
                section_insert(section_entry(out, current_section), key, v);
                free(key);
                free(line);
                i++;
                continue;
            }

            StrBuilder body;
            sb_init(&body);
            if (ao_len > 0) {
                sb_append(&body, after_open);
            }
            i++;
            bool closed = false;
            while (i < line_count) {
                char *trimmed_end = xstrdup(lines[i]);
                rtrim_in_place(trimmed_end);
                if (strcmp(trimmed_end, "\"\"\"") == 0) {
                    free(trimmed_end);
                    closed = true;
                    i++;
                    break;
                }
                free(trimmed_end);
                if (body.len > 0 || ao_len > 0) {
                    sb_append(&body, "\n");
                }
                sb_append(&body, lines[i]);
                i++;
            }
            if (!closed) {
                if (err_msg) {
                    *err_msg = xsprintf(
                        "unterminated triple-quoted block for key `%s` starting near line %zu", key, i);
                }
                free(key);
                free(line);
                sb_free(&body);
                ok = false;
                break;
            }
            CalmValue v = {.type = CALM_STR, .str = sb_take(&body)};
            section_insert(section_entry(out, current_section), key, v);
            free(key);
            free(line);
            continue;
        }

        CalmValue value;
        char *scalar_err = NULL;
        if (!parse_scalar(value_part, &value, &scalar_err)) {
            if (err_msg) {
                *err_msg = xsprintf("line %zu: could not parse value `%s`: %s", i + 1, value_part,
                                     scalar_err ? scalar_err : "invalid value");
            }
            free(scalar_err);
            free(key);
            free(line);
            ok = false;
            break;
        }
        section_insert(section_entry(out, current_section), key, value);
        free(key);
        free(line);
        i++;
    }

    free(current_section);
    free_lines(lines, line_count);

    if (!ok) {
        calm_document_free(out);
        return false;
    }
    return true;
}

/* --- includes ---------------------------------------------------------- */

static char **extract_includes(const char *raw, size_t *out_count) {
    size_t cap = 4, count = 0;
    char **out = xmalloc(cap * sizeof(char *));

    size_t line_count = 0;
    char **lines = split_lines(raw, &line_count);
    for (size_t i = 0; i < line_count; i++) {
        char *line = xstrdup(lines[i]);
        trim_in_place(line);
        if (starts_with(line, "include ")) {
            char *rest = line + strlen("include ");
            while (*rest == ' ' || *rest == '\t') {
                rest++;
            }
            size_t rlen = strlen(rest);
            /* strip trailing whitespace already done by trim_in_place on
             * `line`, so `rest`'s end is already the true end. */
            if (rlen >= 2 && rest[0] == '"' && rest[rlen - 1] == '"') {
                char *inc = xstrndup(rest + 1, rlen - 2);
                if (count == cap) {
                    cap *= 2;
                    out = xrealloc(out, cap * sizeof(char *));
                }
                out[count++] = inc;
            }
        }
        free(line);
    }
    free_lines(lines, line_count);
    *out_count = count;
    return out;
}

static bool calm_parse_file_inner(const char *path, int depth, CalmDocument *out, char **err_msg) {
    if (depth > CALM_MAX_INCLUDE_DEPTH) {
        if (err_msg) {
            *err_msg = xsprintf("include depth exceeded %d while parsing %s", CALM_MAX_INCLUDE_DEPTH, path);
        }
        return false;
    }

    char *raw = read_file_to_string(path);
    if (!raw) {
        if (err_msg) {
            *err_msg = xsprintf("reading calm config %s: %s", path, strerror(errno_snapshot()));
        }
        return false;
    }

    char *parse_err = NULL;
    if (!calm_parse(raw, out, &parse_err)) {
        if (err_msg) {
            *err_msg = xsprintf("parsing %s: %s", path, parse_err ? parse_err : "unknown error");
        }
        free(parse_err);
        free(raw);
        return false;
    }

    size_t include_count = 0;
    char **includes = extract_includes(raw, &include_count);
    free(raw);

    char *dir = dirname_of(path);
    bool ok = true;
    for (size_t i = 0; i < include_count && ok; i++) {
        char *resolved = join_path(dir, includes[i]);
        CalmDocument included;
        char *inc_err = NULL;
        if (!calm_parse_file_inner(resolved, depth + 1, &included, &inc_err)) {
            if (err_msg) {
                *err_msg = xsprintf("including %s from %s: %s", resolved, path, inc_err ? inc_err : "unknown error");
            }
            free(inc_err);
            ok = false;
        } else {
            calm_document_merge(out, &included);
        }
        free(resolved);
    }
    free(dir);
    for (size_t i = 0; i < include_count; i++) {
        free(includes[i]);
    }
    free(includes);

    if (!ok) {
        calm_document_free(out);
        return false;
    }
    return true;
}

bool calm_parse_file(const char *path, CalmDocument *out, char **err_msg) {
    return calm_parse_file_inner(path, 0, out, err_msg);
}

/* --- expand -------------------------------------------------------------- */

char *calm_expand(const char *value) {
    StrBuilder out;
    sb_init(&out);

    const char *p = value;
    if (*p == '~') {
        char *home = home_dir();
        if (home) {
            sb_append(&out, home);
            free(home);
        }
        p++;
    }

    while (*p) {
        if (*p == '$') {
            p++;
            if (*p == '{') {
                p++;
                const char *start = p;
                while (*p && *p != '}') {
                    p++;
                }
                char *name = xstrndup(start, (size_t)(p - start));
                if (*p == '}') {
                    p++;
                }
                const char *val = getenv(name);
                if (val) {
                    sb_append(&out, val);
                }
                free(name);
            } else {
                const char *start = p;
                while (*p && (isalnum((unsigned char)*p) || *p == '_')) {
                    p++;
                }
                if (p == start) {
                    sb_append(&out, "$");
                } else {
                    char *name = xstrndup(start, (size_t)(p - start));
                    const char *val = getenv(name);
                    if (val) {
                        sb_append(&out, val);
                    }
                    free(name);
                }
            }
        } else {
            sb_append_char(&out, *p);
            p++;
        }
    }

    return sb_take(&out);
}
