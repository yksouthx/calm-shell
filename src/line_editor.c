#include "line_editor.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

#include "config.h"
#include "git.h"
#include "util.h"

/* --- StrList ------------------------------------------------------------ */

void strlist_init(StrList *list) {
    list->items = NULL;
    list->count = 0;
    list->cap = 0;
}

void strlist_add(StrList *list, const char *item) {
    if (list->count == list->cap) {
        list->cap = list->cap == 0 ? 8 : list->cap * 2;
        list->items = xrealloc(list->items, list->cap * sizeof(char *));
    }
    list->items[list->count++] = xstrdup(item);
}

bool strlist_contains(const StrList *list, const char *item) {
    for (size_t i = 0; i < list->count; i++) {
        if (strcmp(list->items[i], item) == 0) {
            return true;
        }
    }
    return false;
}

void strlist_free(StrList *list) {
    for (size_t i = 0; i < list->count; i++) {
        free(list->items[i]);
    }
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->cap = 0;
}

static int str_cmp_qsort(const void *a, const void *b) {
    const char *sa = *(const char *const *)a;
    const char *sb = *(const char *const *)b;
    return strcmp(sa, sb);
}

static void strlist_sort_unique(StrList *list) {
    if (list->count == 0) {
        return;
    }
    qsort(list->items, list->count, sizeof(char *), str_cmp_qsort);
    size_t w = 1;
    for (size_t r = 1; r < list->count; r++) {
        if (strcmp(list->items[r], list->items[w - 1]) != 0) {
            list->items[w++] = list->items[r];
        } else {
            free(list->items[r]);
        }
    }
    list->count = w;
}

/* --- known commands ------------------------------------------------------ */

/* Builtins that aren't $PATH executables or config-defined
 * aliases/functions, but should still tab-complete and highlight like
 * any other command. */
static const char *BUILTIN_COMMANDS[] = {"cd", "pushd", "popd", "dirs", "exit", "quit"};
static const size_t BUILTIN_COMMANDS_COUNT = sizeof(BUILTIN_COMMANDS) / sizeof(BUILTIN_COMMANDS[0]);

static void scan_path_commands(StrList *out) {
    const char *path_var = getenv("PATH");
    if (!path_var) {
        return;
    }
    char *copy = xstrdup(path_var);
    char *saveptr = NULL;
    char *dir = strtok_r(copy, ":", &saveptr);
    while (dir) {
        DIR *d = opendir(dir);
        if (d) {
            struct dirent *entry;
            while ((entry = readdir(d)) != NULL) {
                if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                    continue;
                }
                strlist_add(out, entry->d_name);
            }
            closedir(d);
        }
        dir = strtok_r(NULL, ":", &saveptr);
    }
    free(copy);
}

StrList known_commands_build(const CalmDocument *cfg) {
    StrList out;
    strlist_init(&out);
    scan_path_commands(&out);

    const char *sections[] = {"aliases", "directory.aliases", "functions"};
    for (size_t s = 0; s < 3; s++) {
        const CalmSection *sec = calm_document_section(cfg, sections[s]);
        if (!sec) {
            continue;
        }
        for (size_t i = 0; i < sec->count; i++) {
            strlist_add(&out, sec->entries[i].key);
        }
    }

    strlist_sort_unique(&out);
    return out;
}

/* --- history -------------------------------------------------------------- */

/* Escapes embedded newlines/backslashes so a multi-line history entry
 * round-trips as a single on-disk line: '\\' -> "\\\\", '\n' -> "\\n". */
static char *history_escape(const char *line) {
    StrBuilder sb;
    sb_init(&sb);
    for (const char *p = line; *p; p++) {
        if (*p == '\\') {
            sb_append(&sb, "\\\\");
        } else if (*p == '\n') {
            sb_append(&sb, "\\n");
        } else {
            sb_append_char(&sb, *p);
        }
    }
    return sb_take(&sb);
}

static char *history_unescape(const char *line) {
    StrBuilder sb;
    sb_init(&sb);
    for (const char *p = line; *p; p++) {
        if (*p == '\\' && p[1] == 'n') {
            sb_append_char(&sb, '\n');
            p++;
        } else if (*p == '\\' && p[1] == '\\') {
            sb_append_char(&sb, '\\');
            p++;
        } else {
            sb_append_char(&sb, *p);
        }
    }
    return sb_take(&sb);
}

bool history_load(const char *path, History *out) {
    out->entries = NULL;
    out->count = 0;
    out->cap = 0;
    out->path = path ? xstrdup(path) : NULL;

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
        char *decoded = history_unescape(lines[i]);
        if (out->count == out->cap) {
            out->cap = out->cap == 0 ? 64 : out->cap * 2;
            out->entries = xrealloc(out->entries, out->cap * sizeof(char *));
        }
        out->entries[out->count++] = decoded;
    }
    free_lines(lines, line_count);
    return true;
}

void history_add(History *hist, const char *line) {
    if (hist->count == hist->cap) {
        hist->cap = hist->cap == 0 ? 64 : hist->cap * 2;
        hist->entries = xrealloc(hist->entries, hist->cap * sizeof(char *));
    }
    hist->entries[hist->count++] = xstrdup(line);

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

/* --- prompt --------------------------------------------------------------- */

char *display_path(void) {
    char cwd[4096];
    if (!getcwd(cwd, sizeof(cwd))) {
        return xstrdup("?");
    }
    char *home = home_dir();
    if (home) {
        size_t home_len = strlen(home);
        if (strncmp(cwd, home, home_len) == 0) {
            char *result = xsprintf("~%s", cwd + home_len);
            free(home);
            return result;
        }
        free(home);
    }
    return xstrdup(cwd);
}

static char *hostname_str(void) {
    char buf[256];
    if (gethostname(buf, sizeof(buf)) != 0 || buf[0] == '\0') {
        return xstrdup("endeavouros");
    }
    buf[sizeof(buf) - 1] = '\0';
    return xstrdup(buf);
}

static void paint_out(StrBuilder *sb, const Theme *theme, const char *key, const char *text) {
    char *painted = theme_paint(theme, key, text);
    sb_append(sb, painted);
    free(painted);
}

void render_prompt_box(const Theme *theme, const char *icon) {
    StrBuilder sb;
    sb_init(&sb);

    const char *user = getenv("USER");
    if (!user || user[0] == '\0') {
        user = "user";
    }
    char *host = hostname_str();
    char *path = display_path();
    char *user_host = xsprintf("%s@%s", user, host);

    paint_out(&sb, theme, "border", "\xE2\x95\xAD\xE2\x94\x80 ");
    paint_out(&sb, theme, "path", user_host);
    sb_append(&sb, "\n");

    paint_out(&sb, theme, "border", "\xE2\x94\x82  ");
    sb_append(&sb, icon);
    sb_append(&sb, " ");
    paint_out(&sb, theme, "calm_purple", "Calm-shell");
    sb_append(&sb, "\n");

    paint_out(&sb, theme, "border", "\xE2\x94\x82  ");
    paint_out(&sb, theme, "path", path);
    sb_append(&sb, "\n");

    GitStatus status;
    if (git_status(&status)) {
        paint_out(&sb, theme, "border", "\xE2\x94\x82  ");
        char *label = xsprintf("git:%s", status.branch);
        paint_out(&sb, theme, "gentle_pink", label);
        free(label);
        sb_append(&sb, " ");
        if (status.clean) {
            paint_out(&sb, theme, "git_clean", "\xE2\x9C\x93");
        } else {
            paint_out(&sb, theme, "git_dirty", "\xE2\x9C\x97");
        }
        sb_append(&sb, "\n");
        free(status.branch);
    }

    paint_out(&sb, theme, "border", "\xE2\x95\xB0\xE2\x94\x80 ");
    paint_out(&sb, theme, "arrow", "\xE2\x9D\xAF ");

    char *rendered = sb_take(&sb);
    fputs(rendered, stdout);
    fflush(stdout);
    free(rendered);
    free(host);
    free(path);
    free(user_host);
}

/* --- syntax highlighting -------------------------------------------------- */

static bool is_known_command(const StrList *known, const char *word) {
    if (word[0] == '\0') {
        return false;
    }
    if (strcmp(word, "cd") == 0 || strcmp(word, "exit") == 0 || strcmp(word, "quit") == 0) {
        return true;
    }
    if (strchr(word, '/') != NULL) {
        struct stat st;
        return stat(word, &st) == 0 && S_ISREG(st.st_mode);
    }
    return strlist_contains(known, word);
}

/* Renders `buf` with lightweight single-pass highlighting: the command
 * word (green if known, red if not), quoted strings (yellow), and
 * `-flag`-shaped words (blue). Not a shell parser -- no
 * pipes/subshells/operators awareness. */
static char *highlight_line(const Theme *theme, const StrList *known, const char *buf) {
    StrBuilder out;
    sb_init(&out);
    size_t len = strlen(buf);
    size_t i = 0;
    bool first_word = true;

    while (i < len) {
        if (isspace((unsigned char)buf[i])) {
            size_t start = i;
            while (i < len && isspace((unsigned char)buf[i])) {
                i++;
            }
            char *chunk = xstrndup(buf + start, i - start);
            sb_append(&out, chunk);
            free(chunk);
            continue;
        }
        if (buf[i] == '"' || buf[i] == '\'') {
            char quote = buf[i];
            size_t start = i;
            i++;
            while (i < len && buf[i] != quote) {
                i++;
            }
            if (i < len) {
                i++; /* consume closing quote */
            }
            char *chunk = xstrndup(buf + start, i - start);
            paint_out(&out, theme, "warm_yellow", chunk);
            free(chunk);
            first_word = false;
            continue;
        }
        size_t start = i;
        while (i < len && !isspace((unsigned char)buf[i]) && buf[i] != '"' && buf[i] != '\'') {
            i++;
        }
        char *word = xstrndup(buf + start, i - start);
        if (first_word) {
            paint_out(&out, theme, is_known_command(known, word) ? "git_clean" : "soft_red", word);
        } else if (word[0] == '-') {
            paint_out(&out, theme, "soft_blue", word);
        } else {
            sb_append(&out, word);
        }
        free(word);
        first_word = false;
    }

    return sb_take(&out);
}

/* --- tab completion -------------------------------------------------------- */

static StrList complete_commands(const StrList *known, const char *word) {
    StrList out;
    strlist_init(&out);
    for (size_t i = 0; i < BUILTIN_COMMANDS_COUNT; i++) {
        if (starts_with(BUILTIN_COMMANDS[i], word)) {
            strlist_add(&out, BUILTIN_COMMANDS[i]);
        }
    }
    for (size_t i = 0; i < known->count; i++) {
        if (starts_with(known->items[i], word)) {
            strlist_add(&out, known->items[i]);
        }
    }
    strlist_sort_unique(&out);
    return out;
}

static StrList complete_paths(const char *word) {
    StrList out;
    strlist_init(&out);

    const char *slash = strrchr(word, '/');
    char *typed_dir;
    const char *file_prefix;
    if (slash) {
        typed_dir = xstrndup(word, (size_t)(slash - word) + 1);
        file_prefix = slash + 1;
    } else {
        typed_dir = xstrdup("");
        file_prefix = word;
    }

    char *scan_dir;
    if (typed_dir[0] == '\0') {
        char cwd[4096];
        scan_dir = getcwd(cwd, sizeof(cwd)) ? xstrdup(cwd) : xstrdup(".");
    } else {
        scan_dir = calm_expand(typed_dir);
    }

    DIR *d = opendir(scan_dir);
    free(scan_dir);
    if (!d) {
        free(typed_dir);
        return out;
    }

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        if (!starts_with(entry->d_name, file_prefix)) {
            continue;
        }
        if (entry->d_name[0] == '.' && file_prefix[0] != '.') {
            continue;
        }
        char *value = xsprintf("%s%s", typed_dir, entry->d_name);
        strlist_add(&out, value);
        free(value);
    }
    closedir(d);
    strlist_sort_unique(&out);
    free(typed_dir);
    return out;
}

/* --- fuzzy history search --------------------------------------------------- */

/* Scores `entry` against `query` as a case-insensitive, ordered
 * subsequence match (not necessarily contiguous) -- the same shape of
 * match fzf/skim popularized for reverse history search. Returns -1 if
 * `query` isn't a subsequence of `entry`, else a score where higher is
 * better (contiguous runs and earlier matches score higher). */
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
    /* Prefer shorter, tighter matches: penalize by leftover length. */
    score -= (long)(elen - qlen) / 4;
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
        return (int)(sb->score - sa->score);
    }
    return 0;
}

/* Searches `hist` (most recent first) for entries matching `query`,
 * returning up to `limit` results best-match first. Caller must
 * strlist_free() the result. */
static StrList fuzzy_search_history(const History *hist, const char *query, size_t limit) {
    StrList out;
    strlist_init(&out);
    if (hist->count == 0) {
        return out;
    }

    ScoredEntry *scored = xmalloc(hist->count * sizeof(ScoredEntry));
    size_t scored_count = 0;
    /* Most-recent-first, de-duplicated by command text. */
    StrList seen;
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

#define FUZZY_HISTORY_LIMIT 50

/* --- raw terminal mode ------------------------------------------------------ */

static struct termios g_orig_termios;
static bool g_raw_mode_active = false;

static void disable_raw_mode(void) {
    if (g_raw_mode_active) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_orig_termios);
        g_raw_mode_active = false;
    }
}

static bool enable_raw_mode(void) {
    if (tcgetattr(STDIN_FILENO, &g_orig_termios) != 0) {
        return false;
    }
    struct termios raw = g_orig_termios;
    raw.c_iflag &= (tcflag_t) ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
    raw.c_oflag &= (tcflag_t) ~(OPOST);
    raw.c_lflag &= (tcflag_t) ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) != 0) {
        return false;
    }
    g_raw_mode_active = true;
    atexit(disable_raw_mode);
    return true;
}

/* Peeks whether more input is immediately available (used to
 * distinguish a standalone Escape keypress from the start of a `\x1b[`
 * arrow-key escape sequence, without blocking indefinitely). */
static bool input_ready_within(int millis) {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    struct timeval tv = {.tv_sec = 0, .tv_usec = millis * 1000};
    return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0;
}

typedef enum {
    KEY_NONE = 0,
    KEY_UP = 1000,
    KEY_DOWN,
    KEY_LEFT,
    KEY_RIGHT,
    KEY_HOME,
    KEY_END,
    KEY_DEL,
    KEY_ESCAPE,
} SpecialKey;

/* Reads one logical keypress: a plain byte (0-255), or one of the
 * SpecialKey constants for arrows/escape/etc. Returns -1 on read
 * error/EOF. */
static int read_key(void) {
    unsigned char c;
    ssize_t n = read(STDIN_FILENO, &c, 1);
    if (n <= 0) {
        return -1;
    }
    if (c != 27) {
        return c;
    }
    if (!input_ready_within(50)) {
        return KEY_ESCAPE;
    }
    unsigned char seq0;
    if (read(STDIN_FILENO, &seq0, 1) <= 0) {
        return KEY_ESCAPE;
    }
    if (seq0 != '[' && seq0 != 'O') {
        return KEY_ESCAPE;
    }
    unsigned char seq1;
    if (read(STDIN_FILENO, &seq1, 1) <= 0) {
        return KEY_ESCAPE;
    }
    switch (seq1) {
        case 'A':
            return KEY_UP;
        case 'B':
            return KEY_DOWN;
        case 'C':
            return KEY_RIGHT;
        case 'D':
            return KEY_LEFT;
        case 'H':
            return KEY_HOME;
        case 'F':
            return KEY_END;
        case '3': {
            /* Delete key sends ESC [ 3 ~ */
            unsigned char tilde;
            if (read(STDIN_FILENO, &tilde, 1) <= 0) {
                return KEY_ESCAPE;
            }
            return KEY_DEL;
        }
        default:
            return KEY_ESCAPE;
    }
}

/* --- line buffer editing ---------------------------------------------------- */

typedef struct {
    char *data;
    size_t len;
    size_t cap;
    size_t cursor;
} EditBuf;

static void editbuf_init(EditBuf *b) {
    b->cap = 128;
    b->data = xmalloc(b->cap);
    b->data[0] = '\0';
    b->len = 0;
    b->cursor = 0;
}

static void editbuf_set(EditBuf *b, const char *s) {
    size_t n = strlen(s);
    if (n + 1 > b->cap) {
        b->cap = n + 1;
        b->data = xrealloc(b->data, b->cap);
    }
    memcpy(b->data, s, n + 1);
    b->len = n;
    b->cursor = n;
}

static void editbuf_insert(EditBuf *b, char c) {
    if (b->len + 2 > b->cap) {
        b->cap *= 2;
        b->data = xrealloc(b->data, b->cap);
    }
    memmove(b->data + b->cursor + 1, b->data + b->cursor, b->len - b->cursor + 1);
    b->data[b->cursor] = c;
    b->len++;
    b->cursor++;
}

static void editbuf_delete_before_cursor(EditBuf *b) {
    if (b->cursor == 0) {
        return;
    }
    memmove(b->data + b->cursor - 1, b->data + b->cursor, b->len - b->cursor + 1);
    b->len--;
    b->cursor--;
}

static void editbuf_delete_at_cursor(EditBuf *b) {
    if (b->cursor >= b->len) {
        return;
    }
    memmove(b->data + b->cursor, b->data + b->cursor + 1, b->len - b->cursor);
    b->len--;
}

static void editbuf_kill_to_end(EditBuf *b) {
    b->data[b->cursor] = '\0';
    b->len = b->cursor;
}

static void editbuf_kill_whole_line(EditBuf *b) {
    b->data[0] = '\0';
    b->len = 0;
    b->cursor = 0;
}

/* Deletes the word immediately before the cursor, bash Ctrl+W style
 * (trailing whitespace, then the run of non-whitespace before it). */
static void editbuf_kill_word_before(EditBuf *b) {
    size_t end = b->cursor;
    size_t start = end;
    while (start > 0 && isspace((unsigned char)b->data[start - 1])) {
        start--;
    }
    while (start > 0 && !isspace((unsigned char)b->data[start - 1])) {
        start--;
    }
    memmove(b->data + start, b->data + end, b->len - end + 1);
    b->len -= (end - start);
    b->cursor = start;
}

static void editbuf_free(EditBuf *b) {
    free(b->data);
    b->data = NULL;
    b->len = 0;
    b->cap = 0;
    b->cursor = 0;
}

/* --- the editing loop -------------------------------------------------------- */

/* Redraws just the input line (not the whole prompt box): clears the
 * current line, reprints the arrow + highlighted buffer, and
 * repositions the terminal cursor to match `buf->cursor`. */
static void redraw_input(const Theme *theme, const StrList *known, const EditBuf *buf) {
    fputs("\r\x1b[K", stdout);
    char *arrow = theme_paint(theme, "arrow", "\xE2\x9D\xAF ");
    fputs(arrow, stdout);
    free(arrow);

    char *highlighted = highlight_line(theme, known, buf->data);
    fputs(highlighted, stdout);
    free(highlighted);

    /* Move the terminal cursor back from the end of the (plain-text)
     * buffer to `buf->cursor`, in raw byte units. Styling codes don't
     * move the cursor, so this only needs to account for the visible
     * character count, which for our ANSI-wrapped output is just
     * buf->len (styling adds zero-width escape codes around, not
     * inside, each visible run). */
    size_t back = buf->len - buf->cursor;
    if (back > 0) {
        printf("\x1b[%zuD", back);
    }
    fflush(stdout);
}

/* Runs the Ctrl+R fuzzy history search sub-mode. On accept, fills
 * `buf` with the chosen entry (cursor at end) and returns true. On
 * cancel (Esc/Ctrl+G), leaves `buf` untouched and returns false. */
static bool fuzzy_history_search(const Theme *theme, History *hist, EditBuf *buf) {
    char query[256] = {0};
    size_t qlen = 0;
    size_t selected = 0;

    while (true) {
        StrList matches = fuzzy_search_history(hist, query, FUZZY_HISTORY_LIMIT);
        if (selected >= matches.count) {
            selected = matches.count > 0 ? matches.count - 1 : 0;
        }

        fputs("\r\x1b[K", stdout);
        printf("(reverse-i-search)`%s': %s", query, matches.count > 0 ? matches.items[selected] : "");
        fflush(stdout);

        int key = read_key();
        if (key == -1 || key == KEY_ESCAPE || key == 7 /* Ctrl+G */) {
            strlist_free(&matches);
            fputs("\r\x1b[K", stdout);
            fflush(stdout);
            return false;
        }
        if (key == '\r' || key == '\n') {
            if (matches.count > 0) {
                editbuf_set(buf, matches.items[selected]);
            }
            strlist_free(&matches);
            fputs("\r\x1b[K", stdout);
            fflush(stdout);
            return matches.count > 0;
        }
        if (key == 18 /* Ctrl+R again: cycle to next match */) {
            if (matches.count > 0) {
                selected = (selected + 1) % matches.count;
            }
        } else if (key == 127 || key == 8) {
            if (qlen > 0) {
                query[--qlen] = '\0';
                selected = 0;
            }
        } else if (key >= 32 && key < 127 && qlen + 1 < sizeof(query)) {
            query[qlen++] = (char)key;
            query[qlen] = '\0';
            selected = 0;
        }
        strlist_free(&matches);
    }
}

/* Minimal vi normal-mode handling: movement (h/l/0/$), mode switches
 * (i/a back to insert), and delete-under-cursor (x). Not a full modal
 * vocabulary (no dd/dw/word-motions/counts) -- real, useful coverage
 * of the common cases rather than a claim to full vi compatibility. */
static bool handle_vi_normal_key(int key, EditBuf *buf, bool *insert_mode) {
    switch (key) {
        case 'i':
            *insert_mode = true;
            return true;
        case 'a':
            if (buf->cursor < buf->len) {
                buf->cursor++;
            }
            *insert_mode = true;
            return true;
        case 'h':
        case KEY_LEFT:
            if (buf->cursor > 0) {
                buf->cursor--;
            }
            return true;
        case 'l':
        case KEY_RIGHT:
            if (buf->cursor < buf->len) {
                buf->cursor++;
            }
            return true;
        case '0':
            buf->cursor = 0;
            return true;
        case '$':
            buf->cursor = buf->len > 0 ? buf->len - 1 : 0;
            return true;
        case 'x':
            editbuf_delete_at_cursor(buf);
            return true;
        default:
            return false;
    }
}

LineSignal line_editor_read_line(const Theme *theme, const StrList *known, History *hist, bool vi_mode,
                                  char **out_line) {
    *out_line = NULL;

    if (!enable_raw_mode()) {
        return LINE_ERROR;
    }

    EditBuf buf;
    editbuf_init(&buf);
    bool vi_insert_mode = true; /* vi mode starts in insert, like real vi's `i`-less entry here */
    long history_cursor = (long)hist->count; /* one past the newest entry == "not browsing history" */
    char *saved_line_before_history = NULL;

    redraw_input(theme, known, &buf);

    LineSignal result = LINE_ERROR;
    while (true) {
        int key = read_key();
        if (key == -1) {
            result = LINE_ERROR;
            break;
        }

        if (key == '\r' || key == '\n') {
            fputs("\r\n", stdout);
            *out_line = xstrdup(buf.data);
            if (!is_blank(buf.data)) {
                history_add(hist, buf.data);
            }
            result = LINE_SUCCESS;
            break;
        }
        if (key == 3 /* Ctrl+C */) {
            fputs("^C\r\n", stdout);
            result = LINE_CTRLC;
            break;
        }
        if (key == 4 /* Ctrl+D */) {
            if (buf.len == 0) {
                fputs("\r\n", stdout);
                result = LINE_CTRLD;
                break;
            }
            editbuf_delete_at_cursor(&buf);
            redraw_input(theme, known, &buf);
            continue;
        }

        if (vi_mode && !vi_insert_mode) {
            if (handle_vi_normal_key(key, &buf, &vi_insert_mode)) {
                redraw_input(theme, known, &buf);
                continue;
            }
            /* Unrecognized normal-mode key: ignore rather than insert. */
            continue;
        }
        if (vi_mode && key == KEY_ESCAPE) {
            vi_insert_mode = false;
            if (buf.cursor > 0) {
                buf.cursor--;
            }
            redraw_input(theme, known, &buf);
            continue;
        }

        switch (key) {
            case 1 /* Ctrl+A */:
            case KEY_HOME:
                buf.cursor = 0;
                break;
            case 5 /* Ctrl+E */:
            case KEY_END:
                buf.cursor = buf.len;
                break;
            case 11 /* Ctrl+K */:
                editbuf_kill_to_end(&buf);
                break;
            case 21 /* Ctrl+U */:
                editbuf_kill_whole_line(&buf);
                break;
            case 23 /* Ctrl+W */:
                editbuf_kill_word_before(&buf);
                break;
            case 127:
            case 8:
                editbuf_delete_before_cursor(&buf);
                break;
            case KEY_DEL:
                editbuf_delete_at_cursor(&buf);
                break;
            case KEY_LEFT:
                if (buf.cursor > 0) {
                    buf.cursor--;
                }
                break;
            case KEY_RIGHT:
                if (buf.cursor < buf.len) {
                    buf.cursor++;
                }
                break;
            case KEY_UP:
                if (hist->count > 0 && history_cursor > 0) {
                    if (history_cursor == (long)hist->count) {
                        free(saved_line_before_history);
                        saved_line_before_history = xstrdup(buf.data);
                    }
                    history_cursor--;
                    editbuf_set(&buf, hist->entries[history_cursor]);
                }
                break;
            case KEY_DOWN:
                if (history_cursor < (long)hist->count) {
                    history_cursor++;
                    if (history_cursor == (long)hist->count) {
                        editbuf_set(&buf, saved_line_before_history ? saved_line_before_history : "");
                    } else {
                        editbuf_set(&buf, hist->entries[history_cursor]);
                    }
                }
                break;
            case 9 /* Tab */: {
                size_t word_start = buf.cursor;
                while (word_start > 0 && !isspace((unsigned char)buf.data[word_start - 1])) {
                    word_start--;
                }
                char *word = xstrndup(buf.data + word_start, buf.cursor - word_start);
                bool is_first_word = true;
                for (size_t i = 0; i < word_start; i++) {
                    if (!isspace((unsigned char)buf.data[i])) {
                        is_first_word = false;
                        break;
                    }
                }

                StrList matches = (is_first_word && strchr(word, '/') == NULL) ? complete_commands(known, word)
                                                                                : complete_paths(word);
                if (matches.count == 1) {
                    /* Splice the completion in over the partial word. */
                    size_t tail_len = buf.len - buf.cursor;
                    char *tail = xstrndup(buf.data + buf.cursor, tail_len);
                    buf.data[word_start] = '\0';
                    buf.len = word_start;
                    buf.cursor = word_start;
                    for (const char *p = matches.items[0]; *p; p++) {
                        editbuf_insert(&buf, *p);
                    }
                    bool is_dir = ends_with(matches.items[0], "/");
                    if (!is_dir) {
                        editbuf_insert(&buf, ' ');
                    }
                    for (const char *p = tail; *p; p++) {
                        editbuf_insert(&buf, *p);
                    }
                    buf.cursor -= tail_len;
                    free(tail);
                } else if (matches.count > 1) {
                    fputs("\r\n", stdout);
                    for (size_t i = 0; i < matches.count; i++) {
                        printf("%s  ", matches.items[i]);
                    }
                    fputs("\r\n", stdout);
                }
                strlist_free(&matches);
                free(word);
                break;
            }
            case 18 /* Ctrl+R */: {
                if (fuzzy_history_search(theme, hist, &buf)) {
                    /* buf now holds the selected entry, cursor at end. */
                }
                break;
            }
            default:
                if (key >= 32 && key < 256 && key != 127) {
                    editbuf_insert(&buf, (char)key);
                }
                break;
        }

        redraw_input(theme, known, &buf);
    }

    free(saved_line_before_history);
    editbuf_free(&buf);
    disable_raw_mode();
    return result;
}
