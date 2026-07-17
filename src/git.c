#include "git.h"

#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "util.h"

/* Runs `git status --porcelain --branch` in the current directory and
 * captures its stdout. Returns NULL if git couldn't be spawned or
 * exited non-zero (not inside a repo, `git` missing, etc). Caller must
 * free() the result. */
static char *capture_git_status(void) {
    int fds[2];
    if (pipe(fds) != 0) {
        return NULL;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(fds[0]);
        close(fds[1]);
        return NULL;
    }
    if (pid == 0) {
        /* Child: stdout -> pipe, stderr discarded so a "not a git
         * repository" message never leaks into the prompt. */
        close(fds[0]);
        dup2(fds[1], STDOUT_FILENO);
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) {
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }
        close(fds[1]);
        execlp("git", "git", "status", "--porcelain", "--branch", (char *)NULL);
        _exit(127);
    }

    close(fds[1]);
    StrBuilder sb;
    sb_init(&sb);
    char buf[4096];
    ssize_t n;
    while ((n = read(fds[0], buf, sizeof(buf))) > 0) {
        for (ssize_t i = 0; i < n; i++) {
            sb_append_char(&sb, buf[i]);
        }
    }
    close(fds[0]);

    int status = 0;
    waitpid(pid, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        sb_free(&sb);
        return NULL;
    }
    return sb_take(&sb);
}

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

/* Parses git's `## <info>` porcelain branch header, covering the shapes
 * git actually emits: a normal branch (optionally with `...upstream`
 * and an `[ahead N]`/`[behind N]` suffix), a brand new repo with no
 * commits yet, and detached HEAD. */
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
    char *raw = capture_git_status();
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
