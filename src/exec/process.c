#include "exec/process.h"

#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "util/fsutil.h"
#include "util/strbuf.h"
#include "util/xalloc.h"

bool process_run_foreground(char *const argv[], int *exit_status) {
    pid_t pid = fork();
    if (pid < 0) {
        if (exit_status) {
            *exit_status = -1;
        }
        return false;
    }
    if (pid == 0) {
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        execvp(argv[0], argv);
        _exit(127);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    if (exit_status) {
        *exit_status = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    }
    return true;
}

char *process_capture_stdout(char *const argv[]) {
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
        close(fds[0]);
        dup2(fds[1], STDOUT_FILENO);
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) {
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }
        close(fds[1]);
        execvp(argv[0], argv);
        _exit(127);
    }

    close(fds[1]);
    StrBuf sb;
    strbuf_init(&sb);
    char buf[4096];
    ssize_t n;
    while ((n = read(fds[0], buf, sizeof(buf))) > 0) {
        strbuf_append_n(&sb, buf, (size_t)n);
    }
    close(fds[0]);

    int status = 0;
    waitpid(pid, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        strbuf_free(&sb);
        return NULL;
    }
    return strbuf_take(&sb);
}

bool process_command_exists(const char *name) {
    if (strchr(name, '/') != NULL) {
        return access(name, X_OK) == 0;
    }
    const char *path_var = getenv("PATH");
    if (!path_var || path_var[0] == '\0') {
        return false;
    }
    char *copy = xstrdup(path_var);
    char *saveptr = NULL;
    bool found = false;
    for (char *dir = strtok_r(copy, ":", &saveptr); dir != NULL; dir = strtok_r(NULL, ":", &saveptr)) {
        if (dir[0] == '\0') {
            continue; /* an empty PATH entry conventionally means "." */
        }
        char *candidate = join_path(dir, name);
        if (access(candidate, X_OK) == 0) {
            free(candidate);
            found = true;
            break;
        }
        free(candidate);
    }
    free(copy);
    return found;
}
