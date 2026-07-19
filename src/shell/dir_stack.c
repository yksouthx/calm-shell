#include "shell/dir_stack.h"

#include <stdlib.h>

#include "util/xalloc.h"

void dir_stack_init(DirStack *s, size_t max_size) {
    s->dirs = NULL;
    s->count = 0;
    s->cap = 0;
    s->max_size = max_size;
}

void dir_stack_free(DirStack *s) {
    for (size_t i = 0; i < s->count; i++) {
        free(s->dirs[i]);
    }
    free(s->dirs);
    s->dirs = NULL;
    s->count = 0;
    s->cap = 0;
}

void dir_stack_push(DirStack *s, const char *dir) {
    if (s->count == s->cap) {
        s->cap = s->cap == 0 ? 8 : s->cap * 2;
        s->dirs = xrealloc(s->dirs, s->cap * sizeof(char *));
    }
    s->dirs[s->count++] = xstrdup(dir);

    if (s->max_size > 0 && s->count > s->max_size) {
        /* Drop the oldest (bottom-of-stack) entry to make room, since
         * the most-recently-pushed directories are the ones a user
         * actually wants popd to still reach. */
        free(s->dirs[0]);
        for (size_t i = 1; i < s->count; i++) {
            s->dirs[i - 1] = s->dirs[i];
        }
        s->count--;
    }
}

char *dir_stack_pop(DirStack *s) {
    if (s->count == 0) {
        return NULL;
    }
    return s->dirs[--s->count];
}
