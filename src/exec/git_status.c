#include "exec/git_status.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "exec/process.h"
#include "util/strutil.h"
#include "util/xalloc.h"

/* Takes everything before the first "..." (upstream separator) or
 * whitespace (ahead/behind suffix), whichever comes first. */
static char *first_field(const char *s) {
    const char *dots = strstr(s, "...");
    size_t len = dots ? (size_t)(dots - s) : strlen(s);
    size_t ws = 0;
    while (ws < len && !isspace((unsigned char)s[ws])) {
        ws++;
    }
    if (ws == 0) {
        return xstrdup("unknown");
    }
    return xstrndup(s, ws);
}

/* Parses git's `## <info>` porcelain branch header, covering the
 * shapes git actually emits: a normal branch (optionally with
 * `...upstream` and an `[ahead N]`/`[behind N]` suffix), a brand new
 * repo with no commits yet, and detached HEAD. */
static char *parse_branch_header(const char *line) {
    const char *rest = starts_with(line, "## ") ? line + 3 : line;
    char *trimmed = xstrdup(rest);
    trim_in_place(trimmed);

    const char *no_commits_prefix = "No commits yet on ";
    if (starts_with(trimmed, no_commits_prefix)) {
        char *result = first_field(trimmed + strlen(no_commits_prefix));
        free(trimmed);
        return result;
    }
    if (starts_with(trimmed, "HEAD (no branch)")) {
        free(trimmed);
        return xstrdup("detached");
    }

    char *result = first_field(trimmed);
    free(trimmed);
    return result;
}

bool git_status(GitStatus *out) {
    char *const argv[] = {(char *)"git", (char *)"status", (char *)"--porcelain", (char *)"--branch", NULL};
    char *raw = process_capture_stdout(argv);
    if (!raw) {
        return false;
    }

    size_t line_count = 0;
    char **lines = split_lines(raw, &line_count);
    free(raw);

    out->branch = parse_branch_header(line_count > 0 ? lines[0] : "");
    out->clean = line_count <= 1;

    free_lines(lines, line_count);
    return true;
}
