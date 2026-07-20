#include "cli/commands/profile_cmd.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config/paths.h"
#include "config/profile.h"
#include "config/scaffold.h"
#include "util/fsutil.h"
#include "util/strutil.h"
#include "util/xalloc.h"

static void print_usage(void) {
    fprintf(stderr,
            "usage: calm profile                list profiles (same as `calm profile list`)\n"
            "       calm profile list            list available profiles\n"
            "       calm profile current          print the active profile, if any\n"
            "       calm profile <name>          activate a profile\n");
}

static int profile_list(void) {
    if (!ensure_scaffold()) {
        fprintf(stderr, "calm: could not write to ~/.config/calm-shell\n");
        return 1;
    }
    char *dir = profiles_dir();
    if (!dir) {
        fprintf(stderr, "calm: could not resolve ~/.config/calm-shell/profiles\n");
        return 1;
    }
    DIR *d = opendir(dir);
    if (!d) {
        printf("(no profiles yet -- create one at %s/<name>.calm)\n", dir);
        free(dir);
        return 0;
    }

    char *active = profile_active_name();

    struct dirent *entry;
    int count = 0;
    while ((entry = readdir(d)) != NULL) {
        if (!ends_with(entry->d_name, ".calm")) {
            continue;
        }
        size_t name_len = strlen(entry->d_name) - strlen(".calm");
        char *name = xstrndup(entry->d_name, name_len);
        printf("%s %s\n", (active && strcmp(name, active) == 0) ? "*" : " ", name);
        free(name);
        count++;
    }
    closedir(d);
    free(dir);
    free(active);
    if (count == 0) {
        printf("(no profiles yet)\n");
    }
    return 0;
}

static int profile_current(void) {
    char *active = profile_active_name();
    if (active) {
        printf("%s\n", active);
        free(active);
    } else {
        printf("(no profile active)\n");
    }
    return 0;
}

static int profile_activate(const char *name) {
    if (!ensure_scaffold()) {
        fprintf(stderr, "calm: could not write to ~/.config/calm-shell\n");
        return 1;
    }

    Profile p;
    char *err = NULL;
    if (!profile_load(name, &p, &err)) {
        fprintf(stderr, "calm: no such profile `%s` (see `calm profile list`)\n%s\n", name,
                err ? err : "(profiles live in ~/.config/calm-shell/profiles/<name>.calm)");
        free(err);
        return 1;
    }
    free(err);

    char *warnings = NULL;
    bool ok = profile_apply(&p, &warnings);
    profile_free(&p);

    if (!ok) {
        fprintf(stderr, "calm: could not activate profile `%s`\n", name);
        free(warnings);
        return 1;
    }

    printf("Profile set to `%s`.\n", name);
    if (warnings) {
        fputs(warnings, stdout);
        free(warnings);
    }
    return 0;
}

int profile_cmd_run(int argc, char **argv) {
    if (argc < 3 || strcmp(argv[2], "list") == 0) {
        return profile_list();
    }
    if (strcmp(argv[2], "current") == 0) {
        return profile_current();
    }
    if (strcmp(argv[2], "help") == 0 || strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0) {
        print_usage();
        return 0;
    }
    return profile_activate(argv[2]);
}
