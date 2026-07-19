#include "shell/execute.h"

#include <stdlib.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "config/paths.h"
#include "util/fsutil.h"
#include "util/strbuf.h"
#include "util/strutil.h"
#include "util/xalloc.h"

/* --- prelude ------------------------------------------------------------ */

static void append_functions(StrBuf *out, const CalmDocument *cfg) {
    const CalmSection *functions = calm_document_section(cfg, "functions");
    if (!functions) {
        return;
    }
    for (size_t i = 0; i < functions->count; i++) {
        if (functions->entries[i].value.type != CALM_STRING) {
            continue;
        }
        strbuf_append(out, functions->entries[i].key);
        strbuf_append(out, "() {\n");
        strbuf_append(out, functions->entries[i].value.as.string);
        strbuf_append(out, "\n}\n");
    }
}

/* Sources every installed plugin's entry script (from
 * <plugins_dir>/<plugin>/plugin.calm's `entry` field, run relative to
 * the plugin's own directory). A plugin that's missing its manifest,
 * or whose manifest doesn't parse, is silently skipped rather than
 * failing every prompt -- one broken plugin shouldn't take the whole
 * shell down. */
static void append_plugin_sources(StrBuf *out) {
    char *dir = plugins_dir();
    if (!dir) {
        return;
    }
    DIR *d = opendir(dir);
    if (!d) {
        free(dir);
        return;
    }
    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (entry->d_name[0] == '.') {
            continue;
        }
        char *plugin_dir = join_path(dir, entry->d_name);
        if (!path_is_dir(plugin_dir)) {
            free(plugin_dir);
            continue;
        }
        char *manifest_path = join_path(plugin_dir, "plugin.calm");
        CalmDocument manifest;
        char *err = NULL;
        if (calm_parse_file(manifest_path, &manifest, &err)) {
            const char *entry_script = calm_document_get_str(&manifest, "", "entry");
            if (entry_script) {
                char *script_path = join_path(plugin_dir, entry_script);
                if (path_is_file(script_path)) {
                    strbuf_append(out, ". \"");
                    strbuf_append(out, script_path);
                    strbuf_append(out, "\"\n");
                }
                free(script_path);
            }
            calm_document_free(&manifest);
        }
        free(err);
        free(manifest_path);
        free(plugin_dir);
    }
    closedir(d);
    free(dir);
}

char *execute_build_prelude(const CalmDocument *cfg) {
    StrBuf out;
    strbuf_init(&out);
    append_functions(&out, cfg);
    append_plugin_sources(&out);
    return strbuf_take(&out);
}

/* --- routing: builtin fast path vs. full /bin/sh -------------------------- */

bool execute_needs_full_shell(const char *line) {
    bool in_single = false, in_double = false;
    for (const char *p = line; *p; p++) {
        if (*p == '\\' && !in_single && p[1] != '\0') {
            p++;
            continue;
        }
        if (*p == '\'' && !in_double) {
            in_single = !in_single;
            continue;
        }
        if (*p == '"' && !in_single) {
            in_double = !in_double;
            continue;
        }
        if (in_single || in_double) {
            continue;
        }
        switch (*p) {
            case '|':
            case '&':
            case ';':
            case '>':
            case '<':
            case '`':
            case '$':
            case '\n':
                return true;
            default:
                break;
        }
    }
    return false;
}

/* --- execution ------------------------------------------------------------ */

int execute_run(ShellNavState *nav, const char *prelude, const char *line) {
    int fds[2];
    if (pipe(fds) != 0) {
        fprintf(stderr, "calm: could not create a cwd-tracking pipe: %s\n", strerror(errno));
        return -1;
    }

    /* fd 3 carries back just the subshell's final `pwd`, alongside its
     * ordinary stdio (shared with the terminal, so the command stays
     * fully interactive). `$__calm_status` preserves the user command's
     * own exit code across the bookkeeping statements that follow it. */
    char *script = xsprintf(
        "%s\n"
        "%s\n"
        "__calm_status=$?\n"
        "pwd >&3 2>/dev/null\n"
        "exit \"$__calm_status\"\n",
        prelude, line);

    pid_t pid = fork();
    if (pid < 0) {
        free(script);
        close(fds[0]);
        close(fds[1]);
        return -1;
    }
    if (pid == 0) {
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        close(fds[0]);
        dup2(fds[1], 3);
        close(fds[1]);
        char *argv[] = {(char *)"/bin/sh", (char *)"-c", script, NULL};
        execv("/bin/sh", argv);
        _exit(127);
    }

    close(fds[1]);
    free(script);

    int status = 0;
    waitpid(pid, &status, 0);
    int exit_status = WIFEXITED(status) ? WEXITSTATUS(status) : 130;

    /* By now the direct child (/bin/sh) has already exited, so its
     * `pwd >&3` write is sitting in the pipe buffer -- read it
     * non-blocking rather than looping to EOF, since a backgrounded
     * job (`line` itself containing `sleep 100 &`) can hold its own
     * inherited copy of fd 3 open long after the script we care about
     * has finished. */
    int flags = fcntl(fds[0], F_GETFL, 0);
    fcntl(fds[0], F_SETFL, flags | O_NONBLOCK);
    StrBuf captured;
    strbuf_init(&captured);
    char buf[512];
    ssize_t n;
    while ((n = read(fds[0], buf, sizeof(buf))) > 0) {
        strbuf_append_n(&captured, buf, (size_t)n);
    }
    close(fds[0]);

    char *new_cwd = strbuf_take(&captured);
    rtrim_in_place(new_cwd);
    if (new_cwd[0] != '\0') {
        char *current = current_working_dir();
        if (!current || strcmp(current, new_cwd) != 0) {
            shell_nav_sync_cwd(nav, new_cwd);
        }
        free(current);
    }
    free(new_cwd);

    return exit_status;
}
