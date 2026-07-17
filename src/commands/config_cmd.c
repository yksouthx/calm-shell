#include "config_cmd.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#include "../config.h"
#include "../util.h"

int config_cmd_open(void) {
    if (!ensure_scaffold()) {
        fprintf(stderr, "error: could not set up config directory\n");
        return 1;
    }
    char *path = config_file();
    if (!path) {
        fprintf(stderr, "error: could not resolve config file path\n");
        return 1;
    }

    const char *editor = getenv("EDITOR");
    if (!editor || editor[0] == '\0') {
        editor = "nano";
    }

    printf("\x1b[38;2;137;180;250m\xE2\x86\x92\x1b[0m Opening %s with %s\n", path, editor);

    pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr, "error: failed to launch %s: fork failed\n", editor);
        free(path);
        return 1;
    }
    if (pid == 0) {
        execlp(editor, editor, path, (char *)NULL);
        _exit(127);
    }

    int status = 0;
    waitpid(pid, &status, 0);
    free(path);

    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        return 0;
    }
    if (WIFEXITED(status) && WEXITSTATUS(status) == 127) {
        fprintf(stderr, "error: failed to launch %s. Set $EDITOR and try again.\n", editor);
        return 1;
    }
    fprintf(stderr, "error: %s exited with a non-zero status\n", editor);
    return 1;
}
