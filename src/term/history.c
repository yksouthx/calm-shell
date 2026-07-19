#include "term/history.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util/fsutil.h"
#include "util/strbuf.h"
#include "util/strutil.h"
#include "util/xalloc.h"

/* --- on-disk encoding ---------------------------------------------------
 * Escapes embedded newlines/backslashes so a multi-line history entry
 * round-trips as a single on-disk line: '\\' -> "\\\\", '\n' -> "\\n". */

static char *history_escape(const char *line) {
    StrBuf sb;
    strbuf_init(&sb);
    for (const char *p = line; *p; p++) {
        if (*p == '\\') {
            strbuf_append(&sb, "\\\\");
        } else if (*p == '\n') {
            strbuf_append(&sb, "\\n");
        } else {
            strbuf_append_char(&sb, *p);
        }
    }
    return strbuf_take(&sb);
}

static char *history_unescape(const char *line) {
    StrBuf sb;
    strbuf_init(&sb);
    for (const char *p = line; *p; p++) {
        if (*p == '\\' && p[1] == 'n') {
            strbuf_append_char(&sb, '\n');
            p++;
        } else if (*p == '\\' && p[1] == '\\') {
            strbuf_append_char(&sb, '\\');
            p++;
        } else {
            strbuf_append_char(&sb, *p);
        }
    }
    return strbuf_take(&sb);
}

/* --- load / add / free ---------------------------------------------------- */

static void history_push_raw(History *hist, char *owned_line) {
    if (hist->count == hist->cap) {
        hist->cap = hist->cap == 0 ? 64 : hist->cap * 2;
        hist->entries = xrealloc(hist->entries, hist->cap * sizeof(char *));
    }
    hist->entries[hist->count++] = owned_line;
}

/* Drops the oldest (count - max_entries) entries in place, keeping
 * only the most recent `max_entries`. No-op if already within budget. */
static void history_trim_to_cap(History *hist) {
    if (hist->max_entries == 0 || hist->count <= hist->max_entries) {
        return;
    }
    size_t drop = hist->count - hist->max_entries;
    for (size_t i = 0; i < drop; i++) {
        free(hist->entries[i]);
    }
    memmove(hist->entries, hist->entries + drop, (hist->count - drop) * sizeof(char *));
    hist->count -= drop;
}

bool history_load(const char *path, size_t max_entries, bool ignore_dups, History *out) {
    out->entries = NULL;
    out->count = 0;
    out->cap = 0;
    out->path = path ? xstrdup(path) : NULL;
    out->max_entries = max_entries;
    out->ignore_dups = ignore_dups;

    if (!path) {
        return true;
    }
    char *raw = read_file_to_string(path);
    if (!raw) {
        return true; /* missing file just means empty history, not an error */
    }
    size_t line_count = 0;
    char **lines = split_lines(raw, &line_count);
    free(raw);
    for (size_t i = 0; i < line_count; i++) {
        if (is_blank(lines[i])) {
            continue;
        }
        history_push_raw(out, history_unescape(lines[i]));
    }
    free_lines(lines, line_count);

    history_trim_to_cap(out);
    return true;
}

void history_add(History *hist, const char *line) {
    if (hist->ignore_dups && hist->count > 0 && strcmp(hist->entries[hist->count - 1], line) == 0) {
        return;
    }

    history_push_raw(hist, xstrdup(line));
    history_trim_to_cap(hist);

    if (!hist->path) {
        return;
    }
    FILE *f = fopen(hist->path, "a");
    if (!f) {
        return;
    }
    char *escaped = history_escape(line);
    fprintf(f, "%s\n", escaped);
    free(escaped);
    fclose(f);
}

void history_free(History *hist) {
    if (hist->path) {
        /* Rewrite the on-disk file to match the final, capped
         * in-memory contents -- a live session's appends can push the
         * file past `max_entries` between now and the last trim, so
         * this is where the file itself actually gets capped (the
         * same "trim at exit" shape bash's HISTFILESIZE uses). */
        FILE *f = fopen(hist->path, "w");
        if (f) {
            for (size_t i = 0; i < hist->count; i++) {
                char *escaped = history_escape(hist->entries[i]);
                fprintf(f, "%s\n", escaped);
                free(escaped);
            }
            fclose(f);
        }
    }
    for (size_t i = 0; i < hist->count; i++) {
        free(hist->entries[i]);
    }
    free(hist->entries);
    free(hist->path);
    hist->entries = NULL;
    hist->count = 0;
    hist->cap = 0;
    hist->path = NULL;
}

/* --- fuzzy search --------------------------------------------------------- */

/* Scores `entry` against `query` as a case-insensitive, ordered
 * subsequence match (not necessarily contiguous) -- the same shape of
 * match fzf/skim popularized for reverse history search. Returns -1 if
 * `query` isn't a subsequence of `entry`, else a score where higher is
 * better (contiguous runs, word-boundary starts, and tighter overall
 * matches score higher). */
static long fuzzy_score(const char *entry, const char *query) {
    if (query[0] == '\0') {
        return 0;
    }
    size_t ei = 0, qi = 0;
    size_t elen = strlen(entry), qlen = strlen(query);
    long score = 0;
    bool prev_matched_contiguous = false;
    while (ei < elen && qi < qlen) {
        if (tolower((unsigned char)entry[ei]) == tolower((unsigned char)query[qi])) {
            score += prev_matched_contiguous ? 3 : 1;
            if (ei == 0 || entry[ei - 1] == ' ') {
                score += 2; /* bonus for matching at a word boundary */
            }
            prev_matched_contiguous = true;
            qi++;
        } else {
            prev_matched_contiguous = false;
        }
        ei++;
    }
    if (qi < qlen) {
        return -1; /* not every query char was found, in order */
    }
    score -= (long)(elen - qlen) / 4; /* prefer shorter, tighter matches */
    return score;
}

typedef struct {
    const char *entry;
    long score;
} ScoredEntry;

static int scored_cmp(const void *a, const void *b) {
    const ScoredEntry *sa = a;
    const ScoredEntry *sb = b;
    if (sb->score != sa->score) {
        return (sb->score > sa->score) ? 1 : -1;
    }
    return 0;
}

StrList history_fuzzy_search(const History *hist, const char *query, size_t limit) {
    StrList out;
    strlist_init(&out);
    if (hist->count == 0) {
        return out;
    }

    ScoredEntry *scored = xmalloc(hist->count * sizeof(ScoredEntry));
    size_t scored_count = 0;
    StrList seen; /* de-duplicate by command text, most-recent-first */
    strlist_init(&seen);
    for (size_t i = hist->count; i > 0; i--) {
        const char *entry = hist->entries[i - 1];
        if (strlist_contains(&seen, entry)) {
            continue;
        }
        strlist_add(&seen, entry);
        long score = fuzzy_score(entry, query);
        if (score >= 0) {
            scored[scored_count].entry = entry;
            scored[scored_count].score = score;
            scored_count++;
        }
    }
    strlist_free(&seen);

    if (query[0] != '\0') {
        qsort(scored, scored_count, sizeof(ScoredEntry), scored_cmp);
    }
    size_t n = scored_count < limit ? scored_count : limit;
    for (size_t i = 0; i < n; i++) {
        strlist_add(&out, scored[i].entry);
    }
    free(scored);
    return out;
}
