#include "doctor.h"

#include <ctype.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "../calm_format.h"
#include "../config.h"
#include "../util.h"

typedef enum { STATUS_OK, STATUS_WARN, STATUS_FAIL } StatusKind;

static void print_status(const char *label, StatusKind kind, const char *msg) {
    const char *icon;
    switch (kind) {
        case STATUS_OK:
            icon = "\x1b[38;2;166;227;161m\xE2\x9C\x93\x1b[0m";
            break;
        case STATUS_WARN:
            icon = "\x1b[38;2;249;226;175m!\x1b[0m";
            break;
        default:
            icon = "\x1b[38;2;243;139;168m\xE2\x9C\x97\x1b[0m";
            break;
    }
    printf("  %s %-28s \x1b[2m%s\x1b[0m\n", icon, label, msg);
}

static void check_is_arch_based(char *msg_out, size_t msg_cap, StatusKind *kind) {
    char *os_release = read_file_to_string("/etc/os-release");
    if (!os_release) {
        *kind = STATUS_WARN;
        snprintf(msg_out, msg_cap, "/etc/os-release not found");
        return;
    }
    for (char *p = os_release; *p; p++) {
        *p = (char)tolower((unsigned char)*p);
    }
    if (strstr(os_release, "endeavouros")) {
        *kind = STATUS_OK;
        snprintf(msg_out, msg_cap, "EndeavourOS detected");
    } else if (strstr(os_release, "arch")) {
        *kind = STATUS_OK;
        snprintf(msg_out, msg_cap, "Arch-based distro detected");
    } else {
        *kind = STATUS_WARN;
        snprintf(msg_out, msg_cap, "not Arch-based -- some features may not work");
    }
    free(os_release);
}

static bool command_exists(const char *cmd) {
    pid_t pid = fork();
    if (pid < 0) {
        return false;
    }
    if (pid == 0) {
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) {
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }
        execlp("which", "which", cmd, (char *)NULL);
        _exit(127);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

static void check_git(char *msg_out, size_t msg_cap, StatusKind *kind) {
    if (command_exists("git")) {
        *kind = STATUS_OK;
        snprintf(msg_out, msg_cap, "found");
    } else {
        *kind = STATUS_FAIL;
        snprintf(msg_out, msg_cap, "not found -- install with `pacman -S git`");
    }
}

static void check_hyprland(char *msg_out, size_t msg_cap, StatusKind *kind) {
    const char *sig = getenv("HYPRLAND_INSTANCE_SIGNATURE");
    if ((sig && sig[0] != '\0') || command_exists("hyprctl")) {
        *kind = STATUS_OK;
        snprintf(msg_out, msg_cap, "Hyprland detected");
    } else {
        *kind = STATUS_WARN;
        snprintf(msg_out, msg_cap, "not running under Hyprland");
    }
}

static void check_truecolor(char *msg_out, size_t msg_cap, StatusKind *kind) {
    const char *v = getenv("COLORTERM");
    if (v && (strstr(v, "truecolor") || strstr(v, "24bit"))) {
        *kind = STATUS_OK;
        snprintf(msg_out, msg_cap, "truecolor supported");
    } else {
        *kind = STATUS_WARN;
        snprintf(msg_out, msg_cap, "COLORTERM not set -- pastel palette may render inaccurately");
    }
}

static bool check_config_dir(char *msg_out, size_t msg_cap, StatusKind *kind) {
    if (!ensure_scaffold()) {
        *kind = STATUS_FAIL;
        snprintf(msg_out, msg_cap, "could not create config dir");
        return true;
    }
    char *dir = config_dir();
    *kind = STATUS_OK;
    snprintf(msg_out, msg_cap, "%s", dir ? dir : "?");
    free(dir);
    return true;
}

static void check_calm_config(char *msg_out, size_t msg_cap, StatusKind *kind) {
    char *path = config_file();
    if (!path) {
        *kind = STATUS_FAIL;
        snprintf(msg_out, msg_cap, "could not resolve config path");
        return;
    }
    CalmDocument doc;
    char *err = NULL;
    if (calm_parse_file(path, &doc, &err)) {
        calm_document_free(&doc);
        *kind = STATUS_OK;
        snprintf(msg_out, msg_cap, "config.calm parses cleanly");
    } else {
        *kind = STATUS_FAIL;
        snprintf(msg_out, msg_cap, "config.calm failed to parse: %s", err ? err : "unknown error");
    }
    free(err);
    free(path);
}

static void check_editor(char *msg_out, size_t msg_cap, StatusKind *kind) {
    const char *e = getenv("EDITOR");
    if (e && e[0] != '\0') {
        *kind = STATUS_OK;
        snprintf(msg_out, msg_cap, "%s", e);
    } else {
        *kind = STATUS_WARN;
        snprintf(msg_out, msg_cap, "$EDITOR not set -- `calm config` will fall back to nano");
    }
}

int doctor_cmd_run(void) {
    printf("\x1b[38;2;203;166;247m\x1b[1mCalm-shell doctor\x1b[0m\n");
    printf("\x1b[2mDiagnosing system compatibility...\x1b[0m\n\n");

    char msg[512];
    StatusKind kind;

    check_is_arch_based(msg, sizeof(msg), &kind);
    print_status("OS", kind, msg);

    check_git(msg, sizeof(msg), &kind);
    print_status("git", kind, msg);

    check_hyprland(msg, sizeof(msg), &kind);
    print_status("Hyprland", kind, msg);

    check_truecolor(msg, sizeof(msg), &kind);
    print_status("Terminal colors", kind, msg);

    check_editor(msg, sizeof(msg), &kind);
    print_status("$EDITOR", kind, msg);

    check_config_dir(msg, sizeof(msg), &kind);
    print_status("Config directory", kind, msg);

    check_calm_config(msg, sizeof(msg), &kind);
    print_status("config.calm", kind, msg);

    printf("\n\x1b[2mDone.\x1b[0m\n");
    return 0;
}
