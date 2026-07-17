#include "repl.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "config.h"
#include "line_editor.h"
#include "util.h"

typedef struct {
    char *key;
    char *value;
} AliasEntry;

typedef struct {
    AliasEntry *aliases;
    size_t alias_count;
    size_t alias_cap;

    /* Shell source prepended to every spawned command: function
     * definitions from functions.calm plus each installed plugin's
     * init.sh, concatenated once at startup. */
    char *prelude;

    /* directory.calm's `auto_cd` -- a bare, unambiguous directory name
     * typed at the prompt changes into it instead of failing with
     * "command not found". */
    bool auto_cd;
    /* terminal.calm's `bell`. */
    bool bell;

    /* Backing store for pushd/popd/dirs, capped at dir_stack_size. */
    char **dir_stack;
    size_t dir_stack_count;
    size_t dir_stack_cap;
    size_t dir_stack_size;
} ReplContext;

static void alias_add(ReplContext *ctx, const char *key, const char *value) {
    if (ctx->alias_count == ctx->alias_cap) {
        ctx->alias_cap = ctx->alias_cap == 0 ? 8 : ctx->alias_cap * 2;
        ctx->aliases = xrealloc(ctx->aliases, ctx->alias_cap * sizeof(AliasEntry));
    }
    ctx->aliases[ctx->alias_count].key = xstrdup(key);
    ctx->aliases[ctx->alias_count].value = xstrdup(value);
    ctx->alias_count++;
}

static const char *alias_lookup(const ReplContext *ctx, const char *key) {
    for (size_t i = 0; i < ctx->alias_count; i++) {
        if (strcmp(ctx->aliases[i].key, key) == 0) {
            return ctx->aliases[i].value;
        }
    }
    return NULL;
}

/* Pushes a directory onto the pushd/popd stack, dropping the oldest
 * entry once dir_stack_size is exceeded. */
static void push_dir(ReplContext *ctx, const char *path) {
    if (ctx->dir_stack_count == ctx->dir_stack_cap) {
        ctx->dir_stack_cap = ctx->dir_stack_cap == 0 ? 8 : ctx->dir_stack_cap * 2;
        ctx->dir_stack = xrealloc(ctx->dir_stack, ctx->dir_stack_cap * sizeof(char *));
    }
    ctx->dir_stack[ctx->dir_stack_count++] = xstrdup(path);
    if (ctx->dir_stack_count > ctx->dir_stack_size) {
        free(ctx->dir_stack[0]);
        memmove(ctx->dir_stack, ctx->dir_stack + 1, (ctx->dir_stack_count - 1) * sizeof(char *));
        ctx->dir_stack_count--;
    }
}

/* Returns a newly allocated copy of the top of the stack (caller must
 * free()), or NULL if empty; pops it off either way. */
static char *pop_dir(ReplContext *ctx) {
    if (ctx->dir_stack_count == 0) {
        return NULL;
    }
    char *top = ctx->dir_stack[--ctx->dir_stack_count];
    return top;
}

/* Expands a typed line against the alias table. Whole-line matches take
 * priority over first-word matches, matching how shell aliases behave. */
static char *ctx_expand(const ReplContext *ctx, const char *line) {
    const char *whole = alias_lookup(ctx, line);
    if (whole) {
        return xstrdup(whole);
    }

    size_t split = 0;
    while (line[split] && !isspace((unsigned char)line[split])) {
        split++;
    }
    char *first = xstrndup(line, split);
    const char *rest = line + split;
    while (*rest == ' ' || *rest == '\t') {
        rest++;
    }

    const char *target = alias_lookup(ctx, first);
    char *result;
    if (target && rest[0] == '\0') {
        result = xstrdup(target);
    } else if (target) {
        result = xsprintf("%s %s", target, rest);
    } else {
        result = xstrdup(line);
    }
    free(first);
    return result;
}

static void apply_environment(const CalmDocument *cfg) {
    const CalmSection *section = calm_document_section(cfg, "environment");
    if (!section) {
        return;
    }
    for (size_t i = 0; i < section->count; i++) {
        if (section->entries[i].value.type == CALM_STR) {
            setenv(section->entries[i].key, section->entries[i].value.str, 1);
        }
    }
}

/* Builds the shell preamble sourced before every command: one
 * `name() { ... }` per entry in [functions], followed by each
 * installed plugin's init.sh. */
static char *build_prelude(const CalmDocument *cfg) {
    StrBuilder sb;
    sb_init(&sb);

    const CalmSection *functions = calm_document_section(cfg, "functions");
    if (functions) {
        for (size_t i = 0; i < functions->count; i++) {
            if (functions->entries[i].value.type != CALM_STR) {
                continue;
            }
            sb_append(&sb, functions->entries[i].key);
            sb_append(&sb, "() {\n");
            sb_append(&sb, functions->entries[i].value.str);
            sb_append(&sb, "\n}\n");
        }
    }

    char *plugins = plugins_dir();
    if (plugins) {
        DIR *d = opendir(plugins);
        if (d) {
            struct dirent *entry;
            while ((entry = readdir(d)) != NULL) {
                if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                    continue;
                }
                char *plugin_dir = join_path(plugins, entry->d_name);
                char *init_path = join_path(plugin_dir, "init.sh");
                char *contents = read_file_to_string(init_path);
                if (contents) {
                    sb_append(&sb, "# plugin: ");
                    sb_append(&sb, entry->d_name);
                    sb_append(&sb, "\n");
                    sb_append(&sb, contents);
                    sb_append(&sb, "\n");
                    free(contents);
                }
                free(init_path);
                free(plugin_dir);
            }
            closedir(d);
        }
        free(plugins);
    }

    return sb_take(&sb);
}

static ReplContext repl_context_load(const CalmDocument *cfg) {
    apply_environment(cfg);

    ReplContext ctx = {0};

    const CalmSection *aliases = calm_document_section(cfg, "aliases");
    if (aliases) {
        for (size_t i = 0; i < aliases->count; i++) {
            if (aliases->entries[i].value.type == CALM_STR) {
                alias_add(&ctx, aliases->entries[i].key, aliases->entries[i].value.str);
            }
        }
    }
    const CalmSection *dir_aliases = calm_document_section(cfg, "directory.aliases");
    if (dir_aliases) {
        for (size_t i = 0; i < dir_aliases->count; i++) {
            if (dir_aliases->entries[i].value.type == CALM_STR) {
                alias_add(&ctx, dir_aliases->entries[i].key, dir_aliases->entries[i].value.str);
            }
        }
    }

    ctx.prelude = build_prelude(cfg);
    ctx.auto_cd = calm_document_get_bool(cfg, "directory", "auto_cd", false);
    ctx.bell = calm_document_get_bool(cfg, "terminal", "bell", false);
    long long stack_size = calm_document_get_int(cfg, "directory", "dir_stack_size", 20);
    ctx.dir_stack_size = stack_size > 0 ? (size_t)stack_size : 20;

    return ctx;
}

static void repl_context_free(ReplContext *ctx) {
    for (size_t i = 0; i < ctx->alias_count; i++) {
        free(ctx->aliases[i].key);
        free(ctx->aliases[i].value);
    }
    free(ctx->aliases);
    free(ctx->prelude);
    for (size_t i = 0; i < ctx->dir_stack_count; i++) {
        free(ctx->dir_stack[i]);
    }
    free(ctx->dir_stack);
}

/* Updates the process's cwd and OLDPWD together. */
static void apply_cwd_change(const char *new_dir) {
    char prev[4096];
    bool have_prev = getcwd(prev, sizeof(prev)) != NULL;
    if (chdir(new_dir) != 0) {
        fprintf(stderr, "cd: %s: %s\n", new_dir, strerror(errno));
        return;
    }
    if (have_prev) {
        setenv("OLDPWD", prev, 1);
    }
}

/* Reads back the $PWD a spawned command's subshell reported and applies
 * it if it's a real, different directory -- this is what makes `cd`
 * inside a function or plugin (or `mkdir foo && cd foo` typed directly)
 * actually stick, without a persistent shell process. */
static void sync_cwd_from_state_file(const char *state_file) {
    FILE *f = fopen(state_file, "rb");
    if (!f) {
        return;
    }
    char buf[4096];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[n] = '\0';

    char *nul = memchr(buf, '\0', n);
    /* fread stops at n bytes read; the NUL terminator we wrote at buf[n]
     * doesn't count as "found in the file" -- look for an embedded one
     * within the bytes actually read. */
    size_t reported_len = nul ? (size_t)(nul - buf) : n;
    if (reported_len == 0 || reported_len >= sizeof(buf)) {
        return;
    }
    char reported[4096];
    memcpy(reported, buf, reported_len);
    reported[reported_len] = '\0';

    struct stat st;
    if (stat(reported, &st) != 0 || !S_ISDIR(st.st_mode)) {
        return;
    }
    char cwd[4096];
    if (getcwd(cwd, sizeof(cwd)) && strcmp(cwd, reported) == 0) {
        return;
    }
    apply_cwd_change(reported);
}

static void update_terminal_title(void) {
    char *path = display_path();
    printf("\x1b]0;calm \xE2\x80\x94 %s\x07", path);
    free(path);
    fflush(stdout);
}

/* directory.calm's auto_cd: a bare word with no whitespace that isn't
 * one of the directory-changing builtins itself, and that resolves
 * (after ~/$VAR expansion) to a real directory, is treated as `cd
 * <word>` rather than being handed to `sh` to fail on. */
static char *auto_cd_target(const char *line, bool enabled) {
    if (!enabled) {
        return NULL;
    }
    if (line[0] == '\0') {
        return NULL;
    }
    for (const char *p = line; *p; p++) {
        if (isspace((unsigned char)*p)) {
            return NULL;
        }
    }
    if (strcmp(line, "cd") == 0 || strcmp(line, "pushd") == 0 || strcmp(line, "popd") == 0 ||
        strcmp(line, "dirs") == 0 || strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0) {
        return NULL;
    }
    char *expanded = calm_expand(line);
    struct stat st;
    if (stat(expanded, &st) == 0 && S_ISDIR(st.st_mode)) {
        return expanded;
    }
    free(expanded);
    return NULL;
}

static void ring_bell(bool enabled) {
    if (enabled) {
        fputs("\x07", stdout);
        fflush(stdout);
    }
}

/* Prints the directory stack the way dirs/pushd/popd conventionally
 * do: current directory first, then the stack from most to least
 * recent. */
static void print_dir_stack(const ReplContext *ctx) {
    char cwd[4096];
    if (getcwd(cwd, sizeof(cwd))) {
        fputs(cwd, stdout);
    }
    for (size_t i = ctx->dir_stack_count; i > 0; i--) {
        printf(" %s", ctx->dir_stack[i - 1]);
    }
    fputs("\n", stdout);
}

static void run_command(const char *command, ReplContext *ctx, const char *state_file) {
    char *trimmed = xstrdup(command);
    trim_in_place(trimmed);

    /* cd, pushd, popd, and dirs stay fast, dependency-free builtins --
     * no subprocess spawn, no state-file round trip. */
    if (strcmp(trimmed, "cd") == 0 || starts_with(trimmed, "cd ")) {
        const char *target = trimmed + 2;
        while (*target == ' ') {
            target++;
        }
        char *dest = NULL;
        if (target[0] == '\0') {
            dest = home_dir();
        } else if (strcmp(target, "-") == 0) {
            const char *oldpwd = getenv("OLDPWD");
            dest = oldpwd ? xstrdup(oldpwd) : NULL;
        } else {
            dest = calm_expand(target);
        }

        struct stat st;
        if (dest && stat(dest, &st) == 0 && S_ISDIR(st.st_mode)) {
            apply_cwd_change(dest);
        } else if (dest) {
            fprintf(stderr, "cd: %s: not a directory\n", dest);
            ring_bell(ctx->bell);
        } else {
            fprintf(stderr, "cd: could not resolve target directory\n");
            ring_bell(ctx->bell);
        }
        free(dest);
        free(trimmed);
        return;
    }

    if (strcmp(trimmed, "dirs") == 0) {
        print_dir_stack(ctx);
        free(trimmed);
        return;
    }

    if (strcmp(trimmed, "popd") == 0) {
        char *top = pop_dir(ctx);
        if (top) {
            apply_cwd_change(top);
            print_dir_stack(ctx);
            free(top);
        } else {
            fprintf(stderr, "popd: directory stack empty\n");
            ring_bell(ctx->bell);
        }
        free(trimmed);
        return;
    }

    if (strcmp(trimmed, "pushd") == 0 || starts_with(trimmed, "pushd ")) {
        const char *target = trimmed + 5;
        while (*target == ' ') {
            target++;
        }
        if (target[0] == '\0') {
            fprintf(stderr, "pushd: no directory specified\n");
            ring_bell(ctx->bell);
            free(trimmed);
            return;
        }
        char *path = calm_expand(target);
        struct stat st;
        if (stat(path, &st) != 0 || !S_ISDIR(st.st_mode)) {
            fprintf(stderr, "pushd: %s: not a directory\n", path);
            ring_bell(ctx->bell);
            free(path);
            free(trimmed);
            return;
        }
        char cwd[4096];
        if (getcwd(cwd, sizeof(cwd))) {
            push_dir(ctx, cwd);
        }
        apply_cwd_change(path);
        print_dir_stack(ctx);
        free(path);
        free(trimmed);
        return;
    }

    StrBuilder script;
    sb_init(&script);
    if (ctx->prelude[0] != '\0') {
        sb_append(&script, ctx->prelude);
        sb_append(&script, "\n");
    }
    sb_append(&script, trimmed);
    /* Runs unconditionally after the command (newline sequencing, not
     * `&&`), so even a failed command still reports where it left us. */
    char *state_line = xsprintf("\nprintf '%%s\\0' \"$PWD\" > '%s' 2>/dev/null", state_file);
    sb_append(&script, state_line);
    free(state_line);
    char *script_str = sb_take(&script);

    pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr, "calm: failed to run `%s`: %s\n", trimmed, strerror(errno));
        ring_bell(ctx->bell);
        free(script_str);
        free(trimmed);
        return;
    }
    if (pid == 0) {
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        execl("/bin/sh", "sh", "-c", script_str, (char *)NULL);
        _exit(127);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    free(script_str);
    free(trimmed);

    sync_cwd_from_state_file(state_file);
}

int repl_run(const Theme *theme, const CalmDocument *cfg, const char *icon) {
    ReplContext ctx = repl_context_load(cfg);

    /* The shell and any foreground child share the terminal's
     * foreground process group, so a Ctrl+C at the terminal delivers
     * SIGINT to both. Ignoring it here means `calm` itself survives;
     * run_command resets it to default disposition in the child so the
     * actual running command still gets interrupted normally. The line
     * editor puts the terminal in raw mode while editing, which
     * disables ISIG entirely (Ctrl+C arrives as a literal byte,
     * translated to LINE_CTRLC), so this ignore only matters for the
     * "child command is running" window. */
    signal(SIGINT, SIG_IGN);

    StrList known = known_commands_build(cfg);

    char *hist_dir = history_dir();
    char *hist_path = hist_dir ? join_path(hist_dir, "calm_history.txt") : NULL;
    free(hist_dir);
    History hist;
    history_load(hist_path, &hist);
    free(hist_path);

    bool vi_mode = calm_document_get_str(cfg, "keyboard", "edit_mode") &&
                   strcmp(calm_document_get_str(cfg, "keyboard", "edit_mode"), "vi") == 0;
    bool set_title = calm_document_get_bool(cfg, "terminal", "set_title", true);

    char *state_file = xsprintf("/tmp/calm-shell-state-%d", (int)getpid());

    while (true) {
        if (set_title) {
            update_terminal_title();
        }
        render_prompt_box(theme, icon);

        char *line = NULL;
        LineSignal sig = line_editor_read_line(theme, &known, &hist, vi_mode, &line);

        if (sig == LINE_SUCCESS) {
            char *trimmed = xstrdup(line);
            trim_in_place(trimmed);
            free(line);
            if (trimmed[0] == '\0') {
                free(trimmed);
                continue;
            }
            if (strcmp(trimmed, "exit") == 0 || strcmp(trimmed, "quit") == 0) {
                free(trimmed);
                break;
            }
            char *expanded = ctx_expand(&ctx, trimmed);
            char *cd_target = auto_cd_target(expanded, ctx.auto_cd);
            if (cd_target) {
                apply_cwd_change(cd_target);
                free(cd_target);
            } else {
                run_command(expanded, &ctx, state_file);
            }
            free(expanded);
            free(trimmed);
        } else if (sig == LINE_CTRLC) {
            free(line);
            continue;
        } else if (sig == LINE_CTRLD) {
            free(line);
            break;
        } else {
            free(line);
            fprintf(stderr, "calm: line editor error\n");
            break;
        }
    }

    unlink(state_file);
    free(state_file);
    history_free(&hist);
    strlist_free(&known);
    repl_context_free(&ctx);
    return 0;
}
