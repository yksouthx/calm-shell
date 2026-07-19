/* The pushd/popd/dirs directory stack, capped at a configurable size
 * (oldest entries dropped first, same shape as `directory.calm`'s
 * other settings). */
#ifndef CALM_SHELL_DIR_STACK_H
#define CALM_SHELL_DIR_STACK_H

#include <stddef.h>

typedef struct {
    char **dirs; /* most-recently-pushed last */
    size_t count;
    size_t cap;
    size_t max_size; /* 0 means unbounded */
} DirStack;

void dir_stack_init(DirStack *s, size_t max_size);
void dir_stack_free(DirStack *s);

void dir_stack_push(DirStack *s, const char *dir);
/* Removes and returns the most recently pushed directory (caller must
 * free()), or NULL if the stack is empty. */
char *dir_stack_pop(DirStack *s);

#endif /* CALM_SHELL_DIR_STACK_H */
