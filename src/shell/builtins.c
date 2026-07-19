#include "shell/builtins.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config/calmconf.h"
#include "util/fsutil.h"
#include "util/strutil.h"
#include "util/xalloc.h"

void shell_nav_init(ShellNavState *nav, size_t dir_stack_size) {
    dir_stack_init(&nav->dir_stack, dir_stack_size);
    nav->previous_dir = NULL;
    nav->last_error = false;
}

void shell_nav_free(ShellNavState *nav) {
    dir_stack_free(&nav->dir_stack);
    free(nav->previous_dir);
    nav->previous_dir = NULL;
}

bool shell_nav_sync_cwd(ShellNavState *nav, const char *target) {
    char *old_cwd = current_working_dir();
    if (chdir(target) != 0) {
        fprintf(stderr, "cd: %s: %s\n", target, strerror(errno));
        nav->last_error = true;
        free(old_cwd);
        return false;
    }
    if (old_cwd) {
        setenv("OLDPWD", old_cwd, 1);
        free(nav->previous_dir);
        nav->previous_dir = old_cwd;
    }
    char *new_cwd = current_working_dir();
    if (new_cwd) {
        setenv("PWD", new_cwd, 1);
        free(new_cwd);
    }
    return true;
}

static char *resolve_target(const char *raw) {
    return calm_expand(raw);
}

static void do_cd(ShellNavState *nav, const char *arg) {
    if (!arg || arg[0] == '\0') {
        char *home = home_dir();
        if (!home) {
            fprintf(stderr, "cd: HOME not set\n");
            nav->last_error = true;
            return;
        }
        shell_nav_sync_cwd(nav, home);
        free(home);
        return;
    }
    if (strcmp(arg, "-") == 0) {
        if (!nav->previous_dir) {
            fprintf(stderr, "cd: no previous directory\n");
            nav->last_error = true;
            return;
        }
        char *target = xstrdup(nav->previous_dir);
        if (shell_nav_sync_cwd(nav, target)) {
            printf("%s\n", target);
        }
        free(target);
        return;
    }
    char *resolved = resolve_target(arg);
    shell_nav_sync_cwd(nav, resolved);
    free(resolved);
}

static void do_pushd(ShellNavState *nav, const char *arg) {
    char *cwd = current_working_dir();
    if (!cwd) {
        fprintf(stderr, "pushd: could not determine current directory\n");
        nav->last_error = true;
        return;
    }
    if (!arg || arg[0] == '\0') {
        /* No argument: swap with the top of the stack, bash-style. */
        char *top = dir_stack_pop(&nav->dir_stack);
        if (!top) {
            fprintf(stderr, "pushd: no other directory\n");
            nav->last_error = true;
            free(cwd);
            return;
        }
        if (shell_nav_sync_cwd(nav, top)) {
            dir_stack_push(&nav->dir_stack, cwd);
        }
        free(top);
        free(cwd);
        return;
    }
    char *resolved = resolve_target(arg);
    if (shell_nav_sync_cwd(nav, resolved)) {
        dir_stack_push(&nav->dir_stack, cwd);
    }
    free(resolved);
    free(cwd);
}

static void do_popd(ShellNavState *nav) {
    char *dir = dir_stack_pop(&nav->dir_stack);
    if (!dir) {
        fprintf(stderr, "popd: directory stack empty\n");
        nav->last_error = true;
        return;
    }
    if (shell_nav_sync_cwd(nav, dir)) {
        printf("%s\n", dir);
    }
    free(dir);
}

static void do_dirs(const ShellNavState *nav) {
    char *cwd = current_working_dir();
    printf("%s", cwd ? cwd : "?");
    free(cwd);
    for (size_t i = nav->dir_stack.count; i > 0; i--) {
        printf(" %s", nav->dir_stack.dirs[i - 1]);
    }
    printf("\n");
}

/* Splits `line` into its first word and (trimmed) remainder, without
 * modifying `line`. `*first_word` and `*rest` are newly allocated. */
static void split_first_word(const char *line, char **first_word, char **rest) {
    size_t i = 0;
    while (line[i] && !isspace((unsigned char)line[i])) {
        i++;
    }
    *first_word = xstrndup(line, i);
    while (line[i] && isspace((unsigned char)line[i])) {
        i++;
    }
    *rest = xstrdup(line + i);
}

BuiltinOutcome builtins_dispatch(ShellNavState *nav, const char *line, int *exit_status) {
    nav->last_error = false;
    char *first_word, *rest;
    split_first_word(line, &first_word, &rest);

    BuiltinOutcome outcome = BUILTIN_NOT_HANDLED;
    if (strcmp(first_word, "cd") == 0) {
        do_cd(nav, rest);
        outcome = BUILTIN_HANDLED;
    } else if (strcmp(first_word, "pushd") == 0) {
        do_pushd(nav, rest);
        outcome = BUILTIN_HANDLED;
    } else if (strcmp(first_word, "popd") == 0) {
        do_popd(nav);
        outcome = BUILTIN_HANDLED;
    } else if (strcmp(first_word, "dirs") == 0) {
        do_dirs(nav);
        outcome = BUILTIN_HANDLED;
    } else if (strcmp(first_word, "exit") == 0 || strcmp(first_word, "quit") == 0) {
        int code = 0;
        if (rest[0] != '\0') {
            code = atoi(rest); /* non-numeric args just exit 0, matching a bare `exit` */
        }
        if (exit_status) {
            *exit_status = code;
        }
        outcome = BUILTIN_REQUEST_EXIT;
    }

    free(first_word);
    free(rest);
    return outcome;
}

bool builtins_is_auto_cd_target(const char *line) {
    char *trimmed = xstrdup(line);
    trim_in_place(trimmed);
    bool single_word = trimmed[0] != '\0';
    for (const char *p = trimmed; *p; p++) {
        if (isspace((unsigned char)*p)) {
            single_word = false;
            break;
        }
    }
    if (!single_word) {
        free(trimmed);
        return false;
    }
    char *resolved = calm_expand(trimmed);
    bool is_dir = path_is_dir(resolved);
    free(resolved);
    free(trimmed);
    return is_dir;
}
