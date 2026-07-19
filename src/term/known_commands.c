#include "term/known_commands.h"

#include <dirent.h>
#include <stdlib.h>
#include <string.h>

#include "util/xalloc.h"

const char *const CALM_BUILTIN_COMMANDS[] = {"cd", "pushd", "popd", "dirs", "exit", "quit"};
const size_t CALM_BUILTIN_COMMANDS_COUNT = sizeof(CALM_BUILTIN_COMMANDS) / sizeof(CALM_BUILTIN_COMMANDS[0]);

static void scan_path_commands(StrList *out) {
    const char *path_var = getenv("PATH");
    if (!path_var) {
        return;
    }
    char *copy = xstrdup(path_var);
    char *saveptr = NULL;
    for (char *dir = strtok_r(copy, ":", &saveptr); dir != NULL; dir = strtok_r(NULL, ":", &saveptr)) {
        DIR *d = opendir(dir);
        if (!d) {
            continue;
        }
        struct dirent *entry;
        while ((entry = readdir(d)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }
            strlist_add(out, entry->d_name);
        }
        closedir(d);
    }
    free(copy);
}

StrList known_commands_build(const CalmDocument *cfg) {
    StrList out;
    strlist_init(&out);
    scan_path_commands(&out);

    static const char *const sections[] = {"aliases", "directory.aliases", "functions"};
    for (size_t s = 0; s < sizeof(sections) / sizeof(sections[0]); s++) {
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
