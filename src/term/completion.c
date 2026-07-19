#include "term/completion.h"

#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>

#include "config/calmconf.h"
#include "term/known_commands.h"
#include "util/fsutil.h"
#include "util/strutil.h"
#include "util/xalloc.h"

StrList complete_commands(const StrList *known, const char *word) {
    StrList out;
    strlist_init(&out);
    for (size_t i = 0; i < CALM_BUILTIN_COMMANDS_COUNT; i++) {
        if (starts_with(CALM_BUILTIN_COMMANDS[i], word)) {
            strlist_add(&out, CALM_BUILTIN_COMMANDS[i]);
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

StrList complete_paths(const char *word) {
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
        char *cwd = current_working_dir();
        scan_dir = cwd ? cwd : xstrdup(".");
    } else {
        scan_dir = calm_expand(typed_dir);
    }

    DIR *d = opendir(scan_dir);
    if (!d) {
        free(scan_dir);
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
        char *full = join_path(scan_dir, entry->d_name);
        bool is_dir = path_is_dir(full);
        free(full);

        char *value = xsprintf("%s%s%s", typed_dir, entry->d_name, is_dir ? "/" : "");
        strlist_add(&out, value);
        free(value);
    }
    closedir(d);
    strlist_sort_unique(&out);
    free(scan_dir);
    free(typed_dir);
    return out;
}
