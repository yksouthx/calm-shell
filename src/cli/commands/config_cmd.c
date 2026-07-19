#include "cli/commands/config_cmd.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "config/calmconf.h"
#include "config/paths.h"
#include "config/scaffold.h"
#include "exec/process.h"

static void print_usage(void) {
    fprintf(stderr,
            "usage: calm config edit\n"
            "       calm config path\n"
            "       calm config check\n");
}

static int config_edit(void) {
    if (!ensure_scaffold()) {
        fprintf(stderr, "calm: could not write to ~/.config/calm-shell\n");
        return 1;
    }
    char *path = config_file();
    if (!path) {
        fprintf(stderr, "calm: could not resolve config file path\n");
        return 1;
    }
    const char *editor = getenv("EDITOR");
    if (!editor || editor[0] == '\0') {
        editor = "vi";
    }
    char *argv[] = {(char *)editor, path, NULL};
    int status = 0;
    bool spawned = process_run_foreground(argv, &status);
    free(path);
    if (!spawned) {
        fprintf(stderr, "calm: could not launch `%s`\n", editor);
        return 1;
    }
    return status;
}

static int config_path(void) {
    char *dir = config_dir();
    if (!dir) {
        fprintf(stderr, "calm: could not resolve config directory (is $HOME set?)\n");
        return 1;
    }
    printf("%s\n", dir);
    free(dir);
    return 0;
}

static int config_check(void) {
    char *path = config_file();
    if (!path) {
        fprintf(stderr, "calm: could not resolve config file path\n");
        return 1;
    }
    CalmDocument doc;
    char *err = NULL;
    bool ok = calm_parse_file(path, &doc, &err);
    if (ok) {
        printf("%s parses cleanly (%zu section%s).\n", path, doc.count, doc.count == 1 ? "" : "s");
        calm_document_free(&doc);
    } else {
        fprintf(stderr, "%s: %s\n", path, err ? err : "unknown error");
    }
    free(err);
    free(path);
    return ok ? 0 : 1;
}

int config_cmd_run(int argc, char **argv) {
    if (argc < 3) {
        print_usage();
        return 1;
    }
    if (strcmp(argv[2], "edit") == 0) {
        return config_edit();
    }
    if (strcmp(argv[2], "path") == 0) {
        return config_path();
    }
    if (strcmp(argv[2], "check") == 0) {
        return config_check();
    }
    print_usage();
    return 1;
}
